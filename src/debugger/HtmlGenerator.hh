#ifndef HTMLGENERATOR_HH
#define HTMLGENERATOR_HH

#include "DebugHttpServerPort.hh"
#include <string>
#include <cstdint>

namespace openmsx {

class DebugInfoProvider;

/**
 * HTML page generator for the Debug HTTP Server.
 * Generates styled HTML dashboards for viewing debug information in a browser.
 */
class HtmlGenerator {
public:
	// Page generation for each debug type
	static std::string generateMachinePage(DebugInfoProvider& provider);
	static std::string generateIOPage(DebugInfoProvider& provider);
	static std::string generateCPUPage(DebugInfoProvider& provider);
	static std::string generateMemoryPage(DebugInfoProvider& provider,
	                                       unsigned start, unsigned size);

	// Generate page based on type
	static std::string generatePage(DebugInfoType type,
	                                 DebugInfoProvider& provider,
	                                 unsigned memStart = 0,
	                                 unsigned memSize = 256);

private:
	// Common page structure
	static std::string wrapPage(const std::string& title,
	                            const std::string& content,
	                            DebugInfoType activeTab);

	// CSS and navigation
	static std::string getCSS();
	static std::string getNavigation(DebugInfoType activeTab);

	// Utility functions
	static std::string escapeHtml(const std::string& s);
	static std::string toHex8(uint8_t value);
	static std::string toHex16(uint16_t value);
	static std::string flagBox(const std::string& name, bool value);
	static std::string statusDot(bool active);
	static std::string valueBox(const std::string& label, const std::string& value);
	static std::string registerBox(const std::string& name, uint16_t value);
	static std::string registerBox8(const std::string& name, uint8_t value);
};

} // namespace openmsx

#endif // HTMLGENERATOR_HH
