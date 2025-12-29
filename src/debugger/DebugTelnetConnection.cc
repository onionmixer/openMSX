#include "DebugTelnetConnection.hh"

#include "DebugStreamFormatter.hh"

#include <chrono>
#include <cstring>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace openmsx {

DebugTelnetConnection::DebugTelnetConnection(SOCKET socket_,
                                             DebugStreamFormatter& formatter_)
	: socket(socket_)
	, formatter(formatter_)
{
}

DebugTelnetConnection::~DebugTelnetConnection()
{
	stop();
}

void DebugTelnetConnection::start()
{
	thread = std::thread([this]() { run(); });
}

void DebugTelnetConnection::stop()
{
	closed.store(true);
	poller.abort();

	if (socket != OPENMSX_INVALID_SOCKET) {
		sock_close(socket);
		socket = OPENMSX_INVALID_SOCKET;
	}

	if (thread.joinable()) {
		thread.join();
	}
}

void DebugTelnetConnection::run()
{
	try {
		// Send telnet initialization sequence
		sendTelnetInit();

		// Send welcome message with initial state
		sendWelcome();

		// Keep connection alive, monitoring for disconnect
		while (!closed.load() && !poller.aborted()) {
			// Check if client disconnected by peeking
			char peekBuf;
#ifdef _WIN32
			int result = recv(socket, &peekBuf, 1, MSG_PEEK);
#else
			int result = recv(socket, &peekBuf, 1, MSG_PEEK | MSG_DONTWAIT);
#endif
			if (result == 0) {
				// Client disconnected
				break;
			}

			// Sleep briefly to avoid busy waiting
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	} catch (...) {
		// Silently ignore exceptions in connection thread
	}

	closed.store(true);
}

void DebugTelnetConnection::sendTelnetInit()
{
	// Minimal Telnet negotiation:
	// IAC WILL ECHO (0xFF 0xFB 0x01) - Server will echo
	// IAC WILL SUPPRESS-GO-AHEAD (0xFF 0xFB 0x03) - Suppress go-ahead
	unsigned char initSeq[] = {
		0xFF, 0xFB, 0x01,  // IAC WILL ECHO
		0xFF, 0xFB, 0x03,  // IAC WILL SUPPRESS-GO-AHEAD
	};

	std::lock_guard<std::mutex> lock(sendMutex);
	::send(socket, reinterpret_cast<const char*>(initSeq), sizeof(initSeq), 0);
}

void DebugTelnetConnection::sendWelcome()
{
	// Send hello message
	std::string hello = formatter.getHelloMessage();
	if (!hello.empty()) {
		send(hello);
	}

	// Send initial snapshot
	auto snapshot = formatter.getFullSnapshot();
	for (const auto& line : snapshot) {
		send(line);
	}
}

bool DebugTelnetConnection::send(const std::string& data)
{
	if (closed.load() || socket == OPENMSX_INVALID_SOCKET) {
		return false;
	}

	std::string line = data;
	// Ensure line ends with \r\n for telnet compatibility
	if (line.empty() || line.back() != '\n') {
		line += "\r\n";
	} else if (line.size() >= 2 && line[line.size()-2] != '\r') {
		// Replace \n with \r\n
		line.insert(line.size()-1, "\r");
	}

	std::lock_guard<std::mutex> lock(sendMutex);
	int result = ::send(socket, line.c_str(), static_cast<int>(line.size()), 0);
	if (result == SOCKET_ERROR) {
		closed.store(true);
		return false;
	}

	return true;
}

} // namespace openmsx
