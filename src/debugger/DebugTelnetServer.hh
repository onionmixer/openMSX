#ifndef DEBUG_TELNET_SERVER_HH
#define DEBUG_TELNET_SERVER_HH

#include "Socket.hh"
#include "Poller.hh"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace openmsx {

class DebugStreamFormatter;
class DebugTelnetConnection;

/**
 * Telnet-compatible debug streaming server.
 * Broadcasts debug information to all connected clients in JSON Lines format
 * according to OUTPUT_SPEC_V01.md specification.
 *
 * Features:
 * - Telnet protocol compatible (minimal negotiation)
 * - Multi-client support with broadcast capability
 * - JSON Lines format output (one JSON object per line)
 * - Thread-safe broadcasting
 * - Push-based real-time streaming
 *
 * Port: 65505 (configurable via settings)
 */
class DebugTelnetServer final
{
public:
	DebugTelnetServer(int port, DebugStreamFormatter& formatter);
	~DebugTelnetServer();

	DebugTelnetServer(const DebugTelnetServer&) = delete;
	DebugTelnetServer(DebugTelnetServer&&) = delete;
	DebugTelnetServer& operator=(const DebugTelnetServer&) = delete;
	DebugTelnetServer& operator=(DebugTelnetServer&&) = delete;

	void start();
	void stop();

	[[nodiscard]] bool isRunning() const { return running; }
	[[nodiscard]] int getPort() const { return port; }
	[[nodiscard]] std::string getLastError() const { return lastError; }

	// Broadcast data to all connected clients (thread-safe)
	void broadcast(const std::string& data);

	// Get number of connected clients
	[[nodiscard]] size_t getClientCount() const;

	// Called periodically to clean up closed connections
	void cleanupConnections();

private:
	void mainLoop();
	[[nodiscard]] SOCKET createListenSocket();
	void acceptConnection(SOCKET clientSocket);

private:
	int port;
	DebugStreamFormatter& formatter;

	SOCKET listenSocket = OPENMSX_INVALID_SOCKET;
	std::thread thread;
	Poller poller;
	[[no_unique_address]] SocketActivator socketActivator;

	mutable std::mutex connectionsMutex;
	std::vector<std::unique_ptr<DebugTelnetConnection>> connections;

	std::atomic<bool> running{false};
	std::string lastError;
};

} // namespace openmsx

#endif // DEBUG_TELNET_SERVER_HH
