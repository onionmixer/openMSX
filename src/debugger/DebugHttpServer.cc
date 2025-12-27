#include "DebugHttpServer.hh"

#include "DebugHttpServerPort.hh"
#include "DebugInfoProvider.hh"
#include "Reactor.hh"

#include "CommandController.hh"

namespace openmsx {

DebugHttpServer::DebugHttpServer(Reactor& reactor_)
	: reactor(reactor_)
	, infoProvider(std::make_unique<DebugInfoProvider>(reactor_))
	, enableSetting(
		reactor_.getCommandController(),
		"debug_http_enable",
		"Enable HTTP debug server for real-time debugging information",
		false, Setting::Save::YES)
	, machinePortSetting(
		reactor_.getCommandController(),
		"debug_http_machine_port",
		"Port number for machine info HTTP server",
		65501, 1024, 65535, Setting::Save::YES)
	, ioPortSetting(
		reactor_.getCommandController(),
		"debug_http_io_port",
		"Port number for I/O info HTTP server",
		65502, 1024, 65535, Setting::Save::YES)
	, cpuPortSetting(
		reactor_.getCommandController(),
		"debug_http_cpu_port",
		"Port number for CPU info HTTP server",
		65503, 1024, 65535, Setting::Save::YES)
	, memoryPortSetting(
		reactor_.getCommandController(),
		"debug_http_memory_port",
		"Port number for memory info HTTP server",
		65504, 1024, 65535, Setting::Save::YES)
{
	// Attach observers
	enableSetting.attach(*this);
	machinePortSetting.attach(*this);
	ioPortSetting.attach(*this);
	cpuPortSetting.attach(*this);
	memoryPortSetting.attach(*this);

	// Start servers if enabled
	if (enableSetting.getBoolean()) {
		startServers();
	}
}

DebugHttpServer::~DebugHttpServer()
{
	// Detach observers
	memoryPortSetting.detach(*this);
	cpuPortSetting.detach(*this);
	ioPortSetting.detach(*this);
	machinePortSetting.detach(*this);
	enableSetting.detach(*this);

	// Stop servers
	stopServers();
}

void DebugHttpServer::startServers()
{
	if (serversRunning) return;

	try {
		servers[0] = std::make_unique<DebugHttpServerPort>(
			machinePortSetting.getInt(), DebugInfoType::MACHINE, *infoProvider);
		servers[1] = std::make_unique<DebugHttpServerPort>(
			ioPortSetting.getInt(), DebugInfoType::IO, *infoProvider);
		servers[2] = std::make_unique<DebugHttpServerPort>(
			cpuPortSetting.getInt(), DebugInfoType::CPU, *infoProvider);
		servers[3] = std::make_unique<DebugHttpServerPort>(
			memoryPortSetting.getInt(), DebugInfoType::MEMORY, *infoProvider);

		for (auto& server : servers) {
			if (server) {
				server->start();
			}
		}

		serversRunning = true;
	} catch (...) {
		// Clean up on failure
		stopServers();
	}
}

void DebugHttpServer::stopServers()
{
	for (auto& server : servers) {
		if (server) {
			server->stop();
			server.reset();
		}
	}
	serversRunning = false;
}

void DebugHttpServer::updateServers()
{
	if (enableSetting.getBoolean()) {
		// Restart servers with new settings
		stopServers();
		startServers();
	} else {
		stopServers();
	}
}

void DebugHttpServer::update(const Setting& setting) noexcept
{
	if (&setting == &enableSetting ||
	    &setting == &machinePortSetting ||
	    &setting == &ioPortSetting ||
	    &setting == &cpuPortSetting ||
	    &setting == &memoryPortSetting) {
		updateServers();
	}
}

} // namespace openmsx
