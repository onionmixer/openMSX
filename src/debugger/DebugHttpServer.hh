#ifndef DEBUG_HTTP_SERVER_HH
#define DEBUG_HTTP_SERVER_HH

#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "Observer.hh"

#include <array>
#include <memory>

namespace openmsx {

class Reactor;
class DebugHttpServerPort;
class DebugInfoProvider;

/**
 * HTTP Debug Server for real-time debugging information.
 * Provides 4 separate ports for different types of debug info:
 *   - Port 65501: Machine info (slots, drives, cartridges)
 *   - Port 65502: I/O info (port read/write)
 *   - Port 65503: CPU info (registers, flags, PC)
 *   - Port 65504: Memory info (memory dump)
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

private:
	void startServers();
	void stopServers();
	void updateServers();

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	Reactor& reactor;
	std::unique_ptr<DebugInfoProvider> infoProvider;

	// Settings
	BooleanSetting enableSetting;
	IntegerSetting machinePortSetting;
	IntegerSetting ioPortSetting;
	IntegerSetting cpuPortSetting;
	IntegerSetting memoryPortSetting;

	// Server instances (Machine, IO, CPU, Memory)
	std::array<std::unique_ptr<DebugHttpServerPort>, 4> servers;

	bool serversRunning = false;
};

} // namespace openmsx

#endif // DEBUG_HTTP_SERVER_HH
