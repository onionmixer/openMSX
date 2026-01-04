#include "DebugStreamWorker.hh"

#include "DebugHttpServer.hh"
#include "DebugStreamFormatter.hh"
#include "DebugTelnetServer.hh"
#include "Dasm.hh"

#include <chrono>
#include <span>

namespace openmsx {

DebugStreamWorker::DebugStreamWorker(DebugHttpServer& server_,
                                     DebugStreamFormatter& formatter_)
	: server(server_)
	, formatter(formatter_)
{
}

DebugStreamWorker::~DebugStreamWorker()
{
	stop();
}

void DebugStreamWorker::enqueue(const CpuStreamEntry& entry)
{
	// Fast path: just try to push, drop if full
	// CPU thread must never block
	// Return value intentionally ignored - dropping entries is acceptable
	(void)queue.tryPush(entry);
}

void DebugStreamWorker::start()
{
	if (running.load(std::memory_order_acquire)) {
		return;  // Already running
	}

	running.store(true, std::memory_order_release);
	thread = std::thread([this]() { workerLoop(); });
}

void DebugStreamWorker::stop()
{
	if (!running.load(std::memory_order_acquire)) {
		return;  // Not running
	}

	running.store(false, std::memory_order_release);
	poller.abort();

	if (thread.joinable()) {
		thread.join();
	}
}

void DebugStreamWorker::workerLoop()
{
	CpuStreamEntry entry;
	int clientCheckCounter = 0;
	constexpr int CLIENT_CHECK_INTERVAL = 100;  // Check clients every N iterations

	while (running.load(std::memory_order_acquire)) {
		// Periodically update client status
		if (++clientCheckCounter >= CLIENT_CHECK_INTERVAL) {
			clientCheckCounter = 0;
			auto* telnetServer = server.getStreamServer();
			bool hasClients = telnetServer && telnetServer->getClientCount() > 0;
			hasActiveClients.store(hasClients, std::memory_order_release);
		}

		if (queue.tryPop(entry)) {
			processEntry(entry);
		} else {
			// Queue empty - brief sleep to avoid busy waiting
			// Use a short sleep since we want low latency
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
	}

	// Drain remaining entries on shutdown
	while (queue.tryPop(entry)) {
		processEntry(entry);
	}
}

void DebugStreamWorker::processEntry(const CpuStreamEntry& entry)
{
	if (!entry.valid) return;

	// Check if there are clients to send to
	auto* telnetServer = server.getStreamServer();
	if (!telnetServer || telnetServer->getClientCount() == 0) {
		return;
	}

	// Determine actual instruction length from pre-fetched bytes
	auto lenOpt = instructionLength(
		std::span<const uint8_t>(entry.opcode.data(), entry.opcodeLen));
	size_t instrLen = lenOpt.value_or(1);

	// Disassemble from pre-fetched bytes (no memory access needed!)
	std::string dasmOutput;
	dasm(std::span<const uint8_t>(entry.opcode.data(), instrLen),
	     entry.pc, dasmOutput);

	// Format and broadcast trace execution
	std::string traceData = formatter.getTraceExec(entry.pc, dasmOutput);
	server.broadcastStreamData(traceData);

	// Format and broadcast CPU register state
	std::string regData = formatter.getCPURegistersSnapshot(
		entry.af, entry.bc, entry.de, entry.hl,
		entry.ix, entry.iy, entry.sp, entry.pc);
	server.broadcastStreamData(regData);
}

} // namespace openmsx
