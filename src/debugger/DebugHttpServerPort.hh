#ifndef DEBUG_HTTP_SERVER_PORT_HH
#define DEBUG_HTTP_SERVER_PORT_HH

#include "Socket.hh"

#include "Poller.hh"

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace openmsx {

class DebugInfoProvider;
class DebugHttpConnection;

/**
 * Type of debug information served by this port.
 */
enum class DebugInfoType {
	MACHINE,  // Machine info: slots, drives, cartridges
	IO,       // I/O info: port read/write activity
	CPU,      // CPU info: registers, flags, PC
	MEMORY    // Memory info: memory dump
};

[[nodiscard]] constexpr const char* toString(DebugInfoType type) {
	switch (type) {
		case DebugInfoType::MACHINE: return "machine";
		case DebugInfoType::IO:      return "io";
		case DebugInfoType::CPU:     return "cpu";
		case DebugInfoType::MEMORY:  return "memory";
	}
	return "unknown";
}

/**
 * Individual HTTP server port for a specific debug info type.
 * Runs in its own thread and handles multiple client connections.
 */
class DebugHttpServerPort final
{
public:
	DebugHttpServerPort(int port, DebugInfoType type,
	                    DebugInfoProvider& infoProvider);
	~DebugHttpServerPort();

	DebugHttpServerPort(const DebugHttpServerPort&) = delete;
	DebugHttpServerPort(DebugHttpServerPort&&) = delete;
	DebugHttpServerPort& operator=(const DebugHttpServerPort&) = delete;
	DebugHttpServerPort& operator=(DebugHttpServerPort&&) = delete;

	[[nodiscard]] int getPort() const { return port; }
	[[nodiscard]] DebugInfoType getType() const { return type; }
	[[nodiscard]] bool isRunning() const { return running; }

	void start();
	void stop();

	// Called periodically to clean up closed connections
	void cleanupConnections();

private:
	void mainLoop();
	[[nodiscard]] SOCKET createListenSocket();
	void acceptConnection(SOCKET clientSocket);

private:
	int port;
	DebugInfoType type;
	DebugInfoProvider& infoProvider;

	SOCKET listenSocket = OPENMSX_INVALID_SOCKET;
	std::thread thread;
	Poller poller;
	[[no_unique_address]] SocketActivator socketActivator;

	std::mutex connectionsMutex;
	std::vector<std::unique_ptr<DebugHttpConnection>> connections;

	bool running = false;
};

} // namespace openmsx

#endif // DEBUG_HTTP_SERVER_PORT_HH
