#ifndef DEBUG_HTTP_CONNECTION_HH
#define DEBUG_HTTP_CONNECTION_HH

#include "DebugHttpServerPort.hh"
#include "Socket.hh"

#include "Poller.hh"

#include <atomic>
#include <map>
#include <string>
#include <thread>

namespace openmsx {

class DebugInfoProvider;

/**
 * Parsed HTTP request structure.
 */
struct HttpRequest {
	std::string method;
	std::string path;
	std::map<std::string, std::string> queryParams;
	std::map<std::string, std::string> headers;
	bool valid = false;
};

/**
 * Handles a single HTTP client connection.
 * Supports both single request/response and SSE (Server-Sent Events) streaming.
 */
class DebugHttpConnection final
{
public:
	DebugHttpConnection(SOCKET socket, DebugInfoType type,
	                    DebugInfoProvider& infoProvider);
	~DebugHttpConnection();

	DebugHttpConnection(const DebugHttpConnection&) = delete;
	DebugHttpConnection(DebugHttpConnection&&) = delete;
	DebugHttpConnection& operator=(const DebugHttpConnection&) = delete;
	DebugHttpConnection& operator=(DebugHttpConnection&&) = delete;

	void start();
	void stop();
	[[nodiscard]] bool isClosed() const { return closed.load(); }

private:
	void run();

	// HTTP parsing
	[[nodiscard]] std::string readRequest();
	[[nodiscard]] HttpRequest parseHttpRequest(const std::string& raw);
	void parseQueryString(const std::string& query, HttpRequest& request);

	// HTTP response
	void sendHttpResponse(int statusCode, const std::string& contentType,
	                      const std::string& body);
	void sendErrorResponse(int statusCode, const std::string& message);

	// SSE (Server-Sent Events)
	void sendSSEHeader();
	void sendSSEEvent(const std::string& data);

	// Request handling
	void handleRequest(const HttpRequest& request);
	void handleHtmlRequest(const HttpRequest& request);
	void handleApiRequest(const HttpRequest& request);
	void handleInfoRequest(const HttpRequest& request);
	void handleStreamRequest(const HttpRequest& request);

	// Info generation
	[[nodiscard]] std::string generateInfo();

private:
	SOCKET socket;
	DebugInfoType type;
	DebugInfoProvider& infoProvider;

	std::thread thread;
	Poller poller;
	std::atomic<bool> closed{false};

	// Request parameters
	unsigned memoryStart = 0;
	unsigned memorySize = 256;
	int refreshInterval = 100;  // ms for SSE mode
};

} // namespace openmsx

#endif // DEBUG_HTTP_CONNECTION_HH
