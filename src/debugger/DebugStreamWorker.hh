#ifndef DEBUG_STREAM_WORKER_HH
#define DEBUG_STREAM_WORKER_HH

#include "CpuStreamEntry.hh"
#include "SPSCRingBuffer.hh"
#include "Poller.hh"

#include <atomic>
#include <thread>

namespace openmsx {

class DebugHttpServer;
class DebugStreamFormatter;

/**
 * Worker thread for CPU debug stream processing.
 *
 * This class implements the consumer side of a producer-consumer pattern
 * for CPU trace streaming. The CPU emulation thread (producer) enqueues
 * minimal state snapshots, while this worker thread (consumer) handles
 * the expensive operations: disassembly, JSON formatting, and network
 * transmission.
 *
 * Design goals:
 * - Minimize CPU thread overhead (just copy registers and enqueue)
 * - No blocking on queue full (drop entries instead)
 * - Lock-free communication via SPSC ring buffer
 * - All heavy I/O operations offloaded to worker thread
 */
class DebugStreamWorker final
{
public:
	static constexpr size_t QUEUE_CAPACITY = 8192;

	DebugStreamWorker(DebugHttpServer& server,
	                  DebugStreamFormatter& formatter);
	~DebugStreamWorker();

	DebugStreamWorker(const DebugStreamWorker&) = delete;
	DebugStreamWorker(DebugStreamWorker&&) = delete;
	DebugStreamWorker& operator=(const DebugStreamWorker&) = delete;
	DebugStreamWorker& operator=(DebugStreamWorker&&) = delete;

	/**
	 * Enqueue a CPU state entry for processing.
	 * Called from the CPU emulation thread. This must be fast!
	 * If the queue is full, the entry is silently dropped.
	 */
	void enqueue(const CpuStreamEntry& entry);

	/**
	 * Start the worker thread.
	 */
	void start();

	/**
	 * Stop the worker thread.
	 * Drains remaining queue entries before stopping.
	 */
	void stop();

	/**
	 * Check if the worker is running.
	 */
	[[nodiscard]] bool isRunning() const { return running.load(std::memory_order_acquire); }

	/**
	 * Check if there are connected clients (for fast path check).
	 * This is updated periodically by the worker thread.
	 */
	[[nodiscard]] bool hasClients() const { return hasActiveClients.load(std::memory_order_acquire); }

private:
	void workerLoop();
	void processEntry(const CpuStreamEntry& entry);

private:
	DebugHttpServer& server;
	DebugStreamFormatter& formatter;

	SPSCRingBuffer<CpuStreamEntry, QUEUE_CAPACITY> queue;

	std::thread thread;
	Poller poller;
	std::atomic<bool> running{false};
	std::atomic<bool> hasActiveClients{false};
};

} // namespace openmsx

#endif // DEBUG_STREAM_WORKER_HH
