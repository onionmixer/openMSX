#ifndef DEBUG_TELNET_CONNECTION_HH
#define DEBUG_TELNET_CONNECTION_HH

#include "Socket.hh"
#include "Poller.hh"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace openmsx {

class DebugStreamFormatter;

/**
 * Handles a single telnet client connection for debug streaming.
 * Sends debug information in JSON Lines format to the connected client.
 *
 * Features:
 * - Telnet protocol initialization (WILL ECHO, WILL SUPPRESS-GO-AHEAD)
 * - Welcome message with initial state snapshot
 * - Thread-safe send operation
 * - Automatic disconnect detection
 */
class DebugTelnetConnection final
{
public:
	DebugTelnetConnection(SOCKET socket, DebugStreamFormatter& formatter);
	~DebugTelnetConnection();

	DebugTelnetConnection(const DebugTelnetConnection&) = delete;
	DebugTelnetConnection(DebugTelnetConnection&&) = delete;
	DebugTelnetConnection& operator=(const DebugTelnetConnection&) = delete;
	DebugTelnetConnection& operator=(DebugTelnetConnection&&) = delete;

	void start();
	void stop();

	[[nodiscard]] bool isClosed() const { return closed.load(); }
	void markClosed() { closed.store(true); }

	// Send data to this client (thread-safe)
	// Returns false if send failed (connection closed)
	bool send(const std::string& data);

private:
	void run();
	void sendTelnetInit();
	void sendWelcome();

private:
	std::atomic<SOCKET> socket{OPENMSX_INVALID_SOCKET};
	DebugStreamFormatter& formatter;

	std::thread thread;
	Poller poller;
	std::atomic<bool> closed{false};

	std::mutex sendMutex;
};

} // namespace openmsx

#endif // DEBUG_TELNET_CONNECTION_HH
