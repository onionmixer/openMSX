#ifndef DEBUG_HTTP_SERVER_HH
#define DEBUG_HTTP_SERVER_HH

#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "Observer.hh"

#include <array>
#include <memory>
#include <string>

namespace openmsx {

class Reactor;
class DebugHttpServerPort;
class DebugInfoProvider;
class DebugStreamFormatter;
class DebugStreamWorker;
class DebugTelnetServer;

/**
 * HTTP Debug Server for real-time debugging information.
 * Provides 4 separate HTTP ports for different types of debug info:
 *   - Port 65501: Machine info (slots, drives, cartridges)
 *   - Port 65502: I/O info (port read/write)
 *   - Port 65503: CPU info (registers, flags, PC)
 *   - Port 65504: Memory info (memory dump)
 *
 * Also provides a Telnet streaming server (port 65505) for real-time
 * push-based debug information in JSON Lines format.
 */
class DebugHttpServer final : private Observer<Setting>
{
public:
	explicit DebugHttpServer(Reactor& reactor);
	~DebugHttpServer();

	DebugHttpServer(const DebugHttpServer&) = delete;
	DebugHttpServer(DebugHttpServer&&) = delete;
	DebugHttpServer& operator=(const DebugHttpServer&) = delete;
	DebugHttpServer& operator=(DebugHttpServer&&) = delete;

	[[nodiscard]] DebugInfoProvider& getInfoProvider() { return *infoProvider; }
	[[nodiscard]] DebugStreamFormatter* getStreamFormatter() { return streamFormatter.get(); }
	[[nodiscard]] DebugTelnetServer* getStreamServer() { return streamServer.get(); }
	[[nodiscard]] DebugStreamWorker* getStreamWorker() { return streamWorker.get(); }

	// Broadcast data to all connected stream clients
	void broadcastStreamData(const std::string& data);

	// Check if streaming is enabled and has clients
	[[nodiscard]] bool isStreamingActive() const;

private:
	void startServers();
	void stopServers();
	void updateServers();
	void startStreamServer();

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	Reactor& reactor;
	std::unique_ptr<DebugInfoProvider> infoProvider;
	std::unique_ptr<DebugStreamFormatter> streamFormatter;

	// Settings for HTTP servers
	BooleanSetting enableSetting;
	IntegerSetting machinePortSetting;
	IntegerSetting ioPortSetting;
	IntegerSetting cpuPortSetting;
	IntegerSetting memoryPortSetting;

	// Settings for stream server
	BooleanSetting streamEnableSetting;
	IntegerSetting streamPortSetting;

	// HTTP Server instances (Machine, IO, CPU, Memory)
	std::array<std::unique_ptr<DebugHttpServerPort>, 4> servers;

	// Stream server (Telnet, JSON Lines output)
	std::unique_ptr<DebugTelnetServer> streamServer;

	// Stream worker thread for CPU trace processing
	std::unique_ptr<DebugStreamWorker> streamWorker;

	bool serversRunning = false;
	bool streamServerRunning = false;
};

} // namespace openmsx

#endif // DEBUG_HTTP_SERVER_HH
