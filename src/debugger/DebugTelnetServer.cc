#include "DebugTelnetServer.hh"

#include "DebugTelnetConnection.hh"
#include "DebugStreamFormatter.hh"
#include "MSXException.hh"

#include "one_of.hh"

#include <algorithm>
#include <bit>
#include <cerrno>

#ifndef _WIN32
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace openmsx {

DebugTelnetServer::DebugTelnetServer(int port_, DebugStreamFormatter& formatter_,
                                     ClientConnectCallback onClientConnect_)
	: port(port_)
	, formatter(formatter_)
	, onClientConnect(std::move(onClientConnect_))
{
}

DebugTelnetServer::~DebugTelnetServer()
{
	stop();
}

SOCKET DebugTelnetServer::createListenSocket()
{
	SOCKET sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == OPENMSX_INVALID_SOCKET) {
		throw MSXException("Failed to create socket: ", sock_error());
	}

	// Allow address reuse
	int optval = 1;
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
	           std::bit_cast<const char*>(&optval), sizeof(optval));

	// Bind to localhost only for security
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1 only
	addr.sin_port = htons(static_cast<uint16_t>(port));

	if (bind(sd, std::bit_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		sock_close(sd);
		throw MSXException("Failed to bind socket to port ", port, ": ", sock_error());
	}

	if (listen(sd, SOMAXCONN) == SOCKET_ERROR) {
		sock_close(sd);
		throw MSXException("Failed to listen on socket: ", sock_error());
	}

	return sd;
}

void DebugTelnetServer::start()
{
	if (running) return;

	try {
		listenSocket = createListenSocket();
		running = true;
		thread = std::thread([this]() { mainLoop(); });
	} catch (MSXException& e) {
		lastError = e.getMessage();
		if (listenSocket != OPENMSX_INVALID_SOCKET) {
			sock_close(listenSocket);
			listenSocket = OPENMSX_INVALID_SOCKET;
		}
		throw;
	}
}

void DebugTelnetServer::stop()
{
	if (!running) return;

	running = false;
	poller.abort();

	if (listenSocket != OPENMSX_INVALID_SOCKET) {
		sock_close(listenSocket);
		listenSocket = OPENMSX_INVALID_SOCKET;
	}

	if (thread.joinable()) {
		thread.join();
	}

	// Close all connections
	{
		std::lock_guard<std::mutex> lock(connectionsMutex);
		for (auto& conn : connections) {
			if (conn) {
				conn->stop();
			}
		}
		connections.clear();
	}

	// Reset cached client count
	activeClientCount.store(0);
}

void DebugTelnetServer::mainLoop()
{
#ifndef _WIN32
	// Set socket to non-blocking
	fcntl(listenSocket, F_SETFL, O_NONBLOCK);
#endif

	int cleanupCounter = 0;
	while (running) {
#ifndef _WIN32
		if (poller.poll(listenSocket)) {
			break;
		}
#endif
		SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);

		if (poller.aborted() || !running) {
			if (clientSocket != OPENMSX_INVALID_SOCKET) {
				sock_close(clientSocket);
			}
			break;
		}

		if (clientSocket == OPENMSX_INVALID_SOCKET) {
			if (errno == one_of(EAGAIN, EWOULDBLOCK)) {
				// Periodically clean up even when no new connections arrive
				++cleanupCounter;
				if (cleanupCounter >= 10) {  // Every ~10 poll cycles
					cleanupCounter = 0;
					cleanupConnections();
				}
				continue;
			} else {
				break;
			}
		}

#ifndef _WIN32
		// Reset to blocking mode for the client socket
		fcntl(clientSocket, F_SETFL, 0);
#endif

		acceptConnection(clientSocket);

		// Clean up after each new connection
		cleanupConnections();
		cleanupCounter = 0;
	}
}

void DebugTelnetServer::acceptConnection(SOCKET clientSocket)
{
	auto connection = std::make_unique<DebugTelnetConnection>(
		clientSocket, formatter);
	connection->start();

	{
		std::lock_guard<std::mutex> lock(connectionsMutex);
		connections.push_back(std::move(connection));
	}

	// Update cached client count
	++activeClientCount;

	// Notify that a client connected (triggers CPU loop exit for debug streaming)
	if (onClientConnect) {
		onClientConnect();
	}
}

void DebugTelnetServer::broadcast(const std::string& data)
{
	std::lock_guard<std::mutex> lock(connectionsMutex);

	// Prepare data with proper line ending for telnet
	std::string line = data;
	if (line.empty() || line.back() != '\n') {
		line += "\r\n";
	} else if (line.size() >= 2 && line[line.size()-2] != '\r') {
		line.insert(line.size()-1, "\r");
	}

	// Send to all connected clients
	std::vector<DebugTelnetConnection*> deadConnections;
	for (auto& conn : connections) {
		if (conn && !conn->isClosed()) {
			if (!conn->send(line)) {
				deadConnections.push_back(conn.get());
			}
		}
	}

	// Mark dead connections for cleanup
	for (auto* dead : deadConnections) {
		dead->markClosed();
	}
}

void DebugTelnetServer::cleanupConnections()
{
	std::lock_guard<std::mutex> lock(connectionsMutex);

	// First, stop all closed connections to ensure threads are joined
	for (auto& conn : connections) {
		if (conn && conn->isClosed()) {
			conn->stop();  // This will join the thread
		}
	}

	// Then remove them from the vector
	std::erase_if(connections, [](const auto& conn) {
		return !conn || conn->isClosed();
	});

	// Update cached client count to actual active count
	size_t count = std::count_if(connections.begin(), connections.end(),
		[](const auto& conn) { return conn && !conn->isClosed(); });
	activeClientCount.store(count);
}

} // namespace openmsx
