#include "DebugHttpConnection.hh"

#include "DebugInfoProvider.hh"
#include "HtmlGenerator.hh"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace openmsx {

DebugHttpConnection::DebugHttpConnection(SOCKET socket_, DebugInfoType type_,
                                         DebugInfoProvider& infoProvider_)
	: socket(socket_)
	, type(type_)
	, infoProvider(infoProvider_)
{
}

DebugHttpConnection::~DebugHttpConnection()
{
	stop();
}

void DebugHttpConnection::start()
{
	thread = std::thread([this]() { run(); });
}

void DebugHttpConnection::stop()
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

void DebugHttpConnection::run()
{
	try {
		std::string rawRequest = readRequest();
		if (rawRequest.empty()) {
			closed.store(true);
			return;
		}

		HttpRequest request = parseHttpRequest(rawRequest);
		if (!request.valid) {
			sendErrorResponse(400, "Bad Request");
			closed.store(true);
			return;
		}

		handleRequest(request);
	} catch (...) {
		// Silently ignore exceptions in connection thread
	}
	closed.store(true);
}

std::string DebugHttpConnection::readRequest()
{
	std::string result;
	char buffer[4096];

	// Set a read timeout
#ifdef _WIN32
	DWORD timeout = 5000; // 5 seconds
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
	           reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
	           reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif

	while (true) {
		auto bytesRead = recv(socket, buffer, sizeof(buffer) - 1, 0);
		if (bytesRead <= 0) {
			break;
		}
		buffer[bytesRead] = '\0';
		result += buffer;

		// Check if we have received the full HTTP request (headers end with \r\n\r\n)
		if (result.find("\r\n\r\n") != std::string::npos) {
			break;
		}

		// Prevent overly large requests
		if (result.size() > 65536) {
			return {};
		}
	}

	return result;
}

HttpRequest DebugHttpConnection::parseHttpRequest(const std::string& raw)
{
	HttpRequest request;

	// Find the first line
	auto firstLineEnd = raw.find("\r\n");
	if (firstLineEnd == std::string::npos) {
		return request;
	}

	std::string firstLine = raw.substr(0, firstLineEnd);

	// Parse method, path, and HTTP version
	std::istringstream lineStream(firstLine);
	std::string httpVersion;
	if (!(lineStream >> request.method >> request.path >> httpVersion)) {
		return request;
	}

	// Check for valid HTTP method
	if (request.method != "GET" && request.method != "HEAD") {
		return request;
	}

	// Parse query string from path
	auto queryPos = request.path.find('?');
	if (queryPos != std::string::npos) {
		std::string query = request.path.substr(queryPos + 1);
		request.path = request.path.substr(0, queryPos);
		parseQueryString(query, request);
	}

	// Parse headers
	size_t headerStart = firstLineEnd + 2;
	while (headerStart < raw.size()) {
		auto headerEnd = raw.find("\r\n", headerStart);
		if (headerEnd == std::string::npos) break;

		if (headerEnd == headerStart) {
			// Empty line - end of headers
			break;
		}

		std::string headerLine = raw.substr(headerStart, headerEnd - headerStart);
		auto colonPos = headerLine.find(':');
		if (colonPos != std::string::npos) {
			std::string key = headerLine.substr(0, colonPos);
			std::string value = headerLine.substr(colonPos + 1);

			// Trim whitespace from value
			size_t valueStart = value.find_first_not_of(" \t");
			if (valueStart != std::string::npos) {
				value = value.substr(valueStart);
			}

			// Convert header name to lowercase
			std::transform(key.begin(), key.end(), key.begin(),
			               [](unsigned char c) { return std::tolower(c); });
			request.headers[key] = value;
		}

		headerStart = headerEnd + 2;
	}

	request.valid = true;
	return request;
}

void DebugHttpConnection::parseQueryString(const std::string& query, HttpRequest& request)
{
	size_t pos = 0;
	while (pos < query.size()) {
		auto ampPos = query.find('&', pos);
		if (ampPos == std::string::npos) {
			ampPos = query.size();
		}

		std::string param = query.substr(pos, ampPos - pos);
		auto eqPos = param.find('=');
		if (eqPos != std::string::npos) {
			std::string key = param.substr(0, eqPos);
			std::string value = param.substr(eqPos + 1);
			request.queryParams[key] = value;
		}

		pos = ampPos + 1;
	}
}

void DebugHttpConnection::sendHttpResponse(int statusCode, const std::string& contentType,
                                           const std::string& body)
{
	std::string statusText;
	switch (statusCode) {
		case 200: statusText = "OK"; break;
		case 400: statusText = "Bad Request"; break;
		case 404: statusText = "Not Found"; break;
		case 405: statusText = "Method Not Allowed"; break;
		case 500: statusText = "Internal Server Error"; break;
		default:  statusText = "Unknown"; break;
	}

	std::ostringstream response;
	response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
	response << "Content-Type: " << contentType << "\r\n";
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Access-Control-Allow-Origin: *\r\n";
	response << "Cache-Control: no-cache\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	std::string responseStr = response.str();
	send(socket, responseStr.c_str(), static_cast<int>(responseStr.size()), 0);
}

void DebugHttpConnection::sendErrorResponse(int statusCode, const std::string& message)
{
	std::string body = "{\"error\":\"" + message + "\"}";
	sendHttpResponse(statusCode, "application/json", body);
}

void DebugHttpConnection::sendSSEHeader()
{
	std::ostringstream response;
	response << "HTTP/1.1 200 OK\r\n";
	response << "Content-Type: text/event-stream\r\n";
	response << "Access-Control-Allow-Origin: *\r\n";
	response << "Cache-Control: no-cache\r\n";
	response << "Connection: keep-alive\r\n";
	response << "\r\n";

	std::string responseStr = response.str();
	send(socket, responseStr.c_str(), static_cast<int>(responseStr.size()), 0);
}

void DebugHttpConnection::sendSSEEvent(const std::string& data)
{
	std::ostringstream event;
	event << "data: " << data << "\n\n";

	std::string eventStr = event.str();
	send(socket, eventStr.c_str(), static_cast<int>(eventStr.size()), 0);
}

void DebugHttpConnection::handleRequest(const HttpRequest& request)
{
	// Parse memory parameters if this is a memory port
	if (type == DebugInfoType::MEMORY) {
		auto startIt = request.queryParams.find("start");
		if (startIt != request.queryParams.end()) {
			try {
				memoryStart = static_cast<unsigned>(std::stoul(startIt->second, nullptr, 0));
			} catch (...) {
				memoryStart = 0;
			}
		}

		auto sizeIt = request.queryParams.find("size");
		if (sizeIt != request.queryParams.end()) {
			try {
				memorySize = static_cast<unsigned>(std::stoul(sizeIt->second, nullptr, 0));
				// Limit size to prevent huge responses
				if (memorySize > 65536) memorySize = 65536;
			} catch (...) {
				memorySize = 256;
			}
		}
	}

	// Parse refresh interval for streaming
	auto intervalIt = request.queryParams.find("interval");
	if (intervalIt != request.queryParams.end()) {
		try {
			refreshInterval = std::stoi(intervalIt->second);
			if (refreshInterval < 10) refreshInterval = 10;
			if (refreshInterval > 10000) refreshInterval = 10000;
		} catch (...) {
			refreshInterval = 100;
		}
	}

	// Route based on path
	if (request.path == "/") {
		// Root path - serve HTML dashboard
		handleHtmlRequest(request);
	} else if (request.path == "/info") {
		// /info - serve based on Accept header
		handleInfoRequest(request);
	} else if (request.path == "/api" || request.path == "/api/info") {
		// API endpoints always return JSON
		handleApiRequest(request);
	} else if (request.path == "/stream") {
		handleStreamRequest(request);
	} else {
		sendErrorResponse(404, "Not Found");
	}
}

void DebugHttpConnection::handleHtmlRequest(const HttpRequest& /*request*/)
{
	std::string html = HtmlGenerator::generatePage(
		type, infoProvider, memoryStart, memorySize);
	sendHttpResponse(200, "text/html; charset=utf-8", html);
}

void DebugHttpConnection::handleApiRequest(const HttpRequest& /*request*/)
{
	std::string json = generateInfo();
	sendHttpResponse(200, "application/json", json);
}

void DebugHttpConnection::handleInfoRequest(const HttpRequest& request)
{
	// Check Accept header to determine response type
	auto acceptIt = request.headers.find("accept");
	bool wantsHtml = false;

	if (acceptIt != request.headers.end()) {
		const auto& accept = acceptIt->second;
		// Browser typically sends "text/html" first in Accept header
		wantsHtml = (accept.find("text/html") != std::string::npos);
	}

	if (wantsHtml) {
		handleHtmlRequest(request);
	} else {
		handleApiRequest(request);
	}
}

void DebugHttpConnection::handleStreamRequest(const HttpRequest& /*request*/)
{
	sendSSEHeader();

	while (!closed.load() && !poller.aborted()) {
		std::string info = generateInfo();
		sendSSEEvent(info);

		// Sleep for the refresh interval
		std::this_thread::sleep_for(std::chrono::milliseconds(refreshInterval));

		// Check if connection is still alive by trying to peek
		char peekBuf;
		int result = recv(socket, &peekBuf, 1, MSG_PEEK | MSG_DONTWAIT);
		if (result == 0) {
			// Connection closed by client
			break;
		}
	}
}

std::string DebugHttpConnection::generateInfo()
{
	switch (type) {
		case DebugInfoType::MACHINE:
			return infoProvider.getMachineInfo();
		case DebugInfoType::IO:
			return infoProvider.getIOInfo();
		case DebugInfoType::CPU:
			return infoProvider.getCPUInfo();
		case DebugInfoType::MEMORY:
			return infoProvider.getMemoryInfo(memoryStart, memorySize);
	}
	return "{\"error\":\"Unknown info type\"}";
}

} // namespace openmsx
