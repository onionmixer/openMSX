#include "HtmlGenerator.hh"

#include "DebugInfoProvider.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "CPURegs.hh"
#include "MSXCPUInterface.hh"
#include "HardwareConfig.hh"
#include "MSXDevice.hh"

#include <iomanip>
#include <sstream>

namespace openmsx {

// ============================================================================
// CSS Styles (Catppuccin Mocha theme)
// ============================================================================

std::string HtmlGenerator::getCSS()
{
	return R"CSS(
:root {
	--bg-base: #1e1e2e;
	--bg-surface: #313244;
	--bg-overlay: #45475a;
	--text-primary: #cdd6f4;
	--text-muted: #6c7086;
	--text-subtle: #a6adc8;
	--accent-blue: #89b4fa;
	--accent-green: #a6e3a1;
	--accent-yellow: #f9e2af;
	--accent-red: #f38ba8;
	--accent-mauve: #cba6f7;
	--accent-teal: #94e2d5;
	--border-color: #45475a;
}

* { margin: 0; padding: 0; box-sizing: border-box; }

body {
	font-family: 'Courier New', 'Monaco', 'Consolas', monospace;
	background: var(--bg-base);
	color: var(--text-primary);
	line-height: 1.6;
	min-height: 100vh;
}

.container {
	max-width: 1200px;
	margin: 0 auto;
	padding: 20px;
}

/* Header */
header {
	background: var(--bg-surface);
	border-bottom: 2px solid var(--accent-mauve);
	padding: 15px 20px;
	margin-bottom: 20px;
}

header h1 {
	color: var(--accent-mauve);
	font-size: 1.5em;
	margin-bottom: 10px;
}

header h1 span {
	color: var(--text-muted);
	font-size: 0.7em;
	font-weight: normal;
}

/* Navigation */
nav {
	display: flex;
	gap: 10px;
}

nav a {
	color: var(--text-muted);
	text-decoration: none;
	padding: 8px 16px;
	border-radius: 4px;
	background: var(--bg-overlay);
	transition: all 0.2s;
}

nav a:hover {
	background: var(--accent-blue);
	color: var(--bg-base);
}

nav a.active {
	background: var(--accent-mauve);
	color: var(--bg-base);
	font-weight: bold;
}

/* Main content */
main {
	padding: 0 20px 20px;
}

/* Grid layout */
.grid {
	display: grid;
	grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
	gap: 20px;
}

.grid-2 {
	display: grid;
	grid-template-columns: repeat(2, 1fr);
	gap: 15px;
}

.grid-3 {
	display: grid;
	grid-template-columns: repeat(3, 1fr);
	gap: 10px;
}

.grid-4 {
	display: grid;
	grid-template-columns: repeat(4, 1fr);
	gap: 10px;
}

/* Section boxes */
.section {
	background: var(--bg-surface);
	border-radius: 8px;
	padding: 15px;
	border-left: 4px solid var(--accent-blue);
}

.section.green { border-left-color: var(--accent-green); }
.section.yellow { border-left-color: var(--accent-yellow); }
.section.red { border-left-color: var(--accent-red); }
.section.mauve { border-left-color: var(--accent-mauve); }
.section.teal { border-left-color: var(--accent-teal); }

.section h2 {
	color: var(--accent-green);
	font-size: 1.1em;
	margin-bottom: 15px;
	padding-bottom: 8px;
	border-bottom: 1px solid var(--border-color);
}

/* Register display */
.register {
	background: var(--bg-overlay);
	padding: 10px;
	border-radius: 4px;
	text-align: center;
}

.register .name {
	color: var(--text-muted);
	font-size: 0.85em;
	margin-bottom: 4px;
}

.register .value {
	color: var(--accent-yellow);
	font-size: 1.3em;
	font-weight: bold;
}

.register .value.small {
	font-size: 1.1em;
}

/* Flag boxes */
.flags {
	display: flex;
	gap: 8px;
	flex-wrap: wrap;
}

.flag {
	width: 36px;
	height: 36px;
	display: flex;
	align-items: center;
	justify-content: center;
	border-radius: 4px;
	font-weight: bold;
	font-size: 0.9em;
}

.flag.on {
	background: var(--accent-green);
	color: var(--bg-base);
}

.flag.off {
	background: var(--bg-overlay);
	color: var(--text-muted);
}

/* Status indicators */
.status-dot {
	display: inline-block;
	width: 10px;
	height: 10px;
	border-radius: 50%;
	margin-right: 6px;
}

.status-dot.on { background: var(--accent-green); }
.status-dot.off { background: var(--text-muted); }

/* Info rows */
.info-row {
	display: flex;
	justify-content: space-between;
	padding: 8px 0;
	border-bottom: 1px solid var(--border-color);
}

.info-row:last-child { border-bottom: none; }

.info-row .label {
	color: var(--text-muted);
}

.info-row .value {
	color: var(--accent-yellow);
	font-weight: bold;
}

.info-row .value.text {
	color: var(--text-primary);
	font-weight: normal;
}

/* Tables */
table {
	width: 100%;
	border-collapse: collapse;
	font-size: 0.95em;
}

th, td {
	padding: 8px 12px;
	text-align: left;
	border-bottom: 1px solid var(--border-color);
}

th {
	background: var(--bg-overlay);
	color: var(--accent-blue);
	font-weight: bold;
}

td { color: var(--text-primary); }

tr:hover td { background: rgba(137, 180, 250, 0.1); }

.hex { color: var(--accent-yellow); font-family: monospace; }
.addr { color: var(--accent-blue); }
.device { color: var(--accent-teal); }

/* Memory dump */
.memory-dump {
	font-family: 'Courier New', monospace;
	font-size: 0.9em;
	overflow-x: auto;
}

.memory-dump .line {
	display: flex;
	padding: 2px 0;
}

.memory-dump .line:hover {
	background: var(--bg-overlay);
}

.memory-dump .address {
	color: var(--accent-blue);
	min-width: 60px;
	margin-right: 15px;
}

.memory-dump .bytes {
	color: var(--accent-yellow);
	margin-right: 15px;
	letter-spacing: 1px;
}

.memory-dump .ascii {
	color: var(--text-muted);
}

/* Footer */
footer {
	background: var(--bg-surface);
	padding: 10px 20px;
	margin-top: 20px;
	display: flex;
	justify-content: space-between;
	font-size: 0.85em;
	color: var(--text-muted);
}

.cpu-badge {
	display: inline-block;
	padding: 4px 12px;
	border-radius: 4px;
	font-weight: bold;
	font-size: 1.1em;
}

.cpu-badge.z80 {
	background: var(--accent-blue);
	color: var(--bg-base);
}

.cpu-badge.r800 {
	background: var(--accent-mauve);
	color: var(--bg-base);
}

/* Slot grid */
.slot-box {
	background: var(--bg-overlay);
	padding: 8px 12px;
	border-radius: 4px;
	text-align: center;
}

.slot-box .slot-label {
	color: var(--text-muted);
	font-size: 0.8em;
}

.slot-box .slot-value {
	color: var(--accent-yellow);
	font-size: 1.2em;
	font-weight: bold;
}

/* Responsive */
@media (max-width: 768px) {
	.grid { grid-template-columns: 1fr; }
	.grid-2, .grid-3, .grid-4 { grid-template-columns: repeat(2, 1fr); }
	nav { flex-wrap: wrap; }
}
)CSS";
}

// ============================================================================
// Navigation
// ============================================================================

std::string HtmlGenerator::getNavigation(DebugInfoType activeTab)
{
	std::ostringstream nav;
	nav << "<nav>\n";
	nav << "  <a href=\"http://127.0.0.1:65501/\" class=\""
	    << (activeTab == DebugInfoType::MACHINE ? "active" : "") << "\">Machine</a>\n";
	nav << "  <a href=\"http://127.0.0.1:65502/\" class=\""
	    << (activeTab == DebugInfoType::IO ? "active" : "") << "\">I/O</a>\n";
	nav << "  <a href=\"http://127.0.0.1:65503/\" class=\""
	    << (activeTab == DebugInfoType::CPU ? "active" : "") << "\">CPU</a>\n";
	nav << "  <a href=\"http://127.0.0.1:65504/\" class=\""
	    << (activeTab == DebugInfoType::MEMORY ? "active" : "") << "\">Memory</a>\n";
	nav << "</nav>\n";
	return nav.str();
}

// ============================================================================
// Page wrapper
// ============================================================================

std::string HtmlGenerator::wrapPage(const std::string& title,
                                     const std::string& content,
                                     DebugInfoType activeTab)
{
	std::ostringstream html;
	html << "<!DOCTYPE html>\n";
	html << "<html lang=\"en\">\n";
	html << "<head>\n";
	html << "  <meta charset=\"UTF-8\">\n";
	html << "  <meta http-equiv=\"refresh\" content=\"1\">\n";
	html << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
	html << "  <title>openMSX Debug - " << escapeHtml(title) << "</title>\n";
	html << "  <style>" << getCSS() << "</style>\n";
	html << "</head>\n";
	html << "<body>\n";
	html << "<header>\n";
	html << "  <h1>openMSX Debug Server <span>- " << escapeHtml(title) << "</span></h1>\n";
	html << getNavigation(activeTab);
	html << "</header>\n";
	html << "<main>\n";
	html << "<div class=\"container\">\n";
	html << content;
	html << "</div>\n";
	html << "</main>\n";
	html << "<footer>\n";
	html << "  <span>Auto-refresh: 1s</span>\n";
	html << "  <span>openMSX Debug HTTP Server</span>\n";
	html << "</footer>\n";
	html << "</body>\n";
	html << "</html>\n";
	return html.str();
}

// ============================================================================
// Utility functions
// ============================================================================

std::string HtmlGenerator::escapeHtml(const std::string& s)
{
	std::string result;
	result.reserve(s.size());
	for (char c : s) {
		switch (c) {
			case '&':  result += "&amp;"; break;
			case '<':  result += "&lt;"; break;
			case '>':  result += "&gt;"; break;
			case '"':  result += "&quot;"; break;
			case '\'': result += "&#39;"; break;
			default:   result += c;
		}
	}
	return result;
}

std::string HtmlGenerator::toHex8(uint8_t value)
{
	std::ostringstream oss;
	oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
	    << static_cast<int>(value);
	return oss.str();
}

std::string HtmlGenerator::toHex16(uint16_t value)
{
	std::ostringstream oss;
	oss << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
	    << static_cast<int>(value);
	return oss.str();
}

std::string HtmlGenerator::flagBox(const std::string& name, bool value)
{
	std::ostringstream html;
	html << "<div class=\"flag " << (value ? "on" : "off") << "\">"
	     << escapeHtml(name) << "</div>";
	return html.str();
}

std::string HtmlGenerator::statusDot(bool active)
{
	return std::string("<span class=\"status-dot ") + (active ? "on" : "off") + "\"></span>";
}

std::string HtmlGenerator::registerBox(const std::string& name, uint16_t value)
{
	std::ostringstream html;
	html << "<div class=\"register\">\n";
	html << "  <div class=\"name\">" << escapeHtml(name) << "</div>\n";
	html << "  <div class=\"value\">" << toHex16(value) << "</div>\n";
	html << "</div>\n";
	return html.str();
}

std::string HtmlGenerator::registerBox8(const std::string& name, uint8_t value)
{
	std::ostringstream html;
	html << "<div class=\"register\">\n";
	html << "  <div class=\"name\">" << escapeHtml(name) << "</div>\n";
	html << "  <div class=\"value small\">" << toHex8(value) << "</div>\n";
	html << "</div>\n";
	return html.str();
}

// ============================================================================
// Machine Page
// ============================================================================

std::string HtmlGenerator::generateMachinePage(DebugInfoProvider& provider)
{
	std::ostringstream content;
	MSXMotherBoard* board = provider.getMotherBoard();

	if (!board) {
		content << "<div class=\"section red\">\n";
		content << "  <h2>No Machine</h2>\n";
		content << "  <p>No machine is currently loaded.</p>\n";
		content << "</div>\n";
		return wrapPage("Machine", content.str(), DebugInfoType::MACHINE);
	}

	auto& cpu = board->getCPU();
	auto& cpuInterface = board->getCPUInterface();

	content << "<div class=\"grid\">\n";

	// System Info section
	content << "<div class=\"section mauve\">\n";
	content << "  <h2>System Information</h2>\n";
	content << "  <div class=\"info-row\"><span class=\"label\">Machine ID</span>"
	        << "<span class=\"value text\">" << escapeHtml(std::string(board->getMachineID())) << "</span></div>\n";
	content << "  <div class=\"info-row\"><span class=\"label\">Machine Name</span>"
	        << "<span class=\"value text\">" << escapeHtml(std::string(board->getMachineName())) << "</span></div>\n";
	content << "  <div class=\"info-row\"><span class=\"label\">Machine Type</span>"
	        << "<span class=\"value\">" << escapeHtml(std::string(board->getMachineType())) << "</span></div>\n";
	content << "  <div class=\"info-row\"><span class=\"label\">Status</span>"
	        << "<span class=\"value\">" << statusDot(board->isPowered())
	        << (board->isPowered() ? "Running" : "Powered Off") << "</span></div>\n";
	content << "</div>\n";

	// CPU Type section
	content << "<div class=\"section\">\n";
	content << "  <h2>CPU</h2>\n";
	content << "  <div style=\"text-align:center; padding: 10px;\">\n";
	content << "    <span class=\"cpu-badge " << (cpu.isR800Active() ? "r800" : "z80") << "\">"
	        << (cpu.isR800Active() ? "R800" : "Z80") << "</span>\n";
	content << "  </div>\n";
	content << "</div>\n";

	content << "</div>\n"; // end grid

	// Slots table
	content << "<div class=\"section green\" style=\"margin-top: 20px;\">\n";
	content << "  <h2>Memory Slots</h2>\n";
	content << "  <table>\n";
	content << "    <tr><th>Page</th><th>Address</th><th>Primary</th><th>Secondary</th><th>Expanded</th><th>Device</th></tr>\n";

	for (int page = 0; page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(page);
		int ss = cpuInterface.getSecondarySlot(page);
		bool expanded = cpuInterface.isExpanded(ps);
		MSXDevice* device = cpuInterface.getVisibleMSXDevice(page);

		content << "    <tr>\n";
		content << "      <td>" << page << "</td>\n";
		content << "      <td class=\"addr\">" << toHex16(static_cast<uint16_t>(page * 0x4000)) << "h</td>\n";
		content << "      <td class=\"hex\">" << ps << "</td>\n";
		content << "      <td class=\"hex\">" << (expanded ? std::to_string(ss) : "-") << "</td>\n";
		content << "      <td>" << (expanded ? "Yes" : "No") << "</td>\n";
		content << "      <td class=\"device\">" << (device ? escapeHtml(std::string(device->getName())) : "-") << "</td>\n";
		content << "    </tr>\n";
	}

	content << "  </table>\n";
	content << "</div>\n";

	// Extensions
	const auto& extensions = board->getExtensions();
	content << "<div class=\"section teal\" style=\"margin-top: 20px;\">\n";
	content << "  <h2>Extensions</h2>\n";
	if (extensions.empty()) {
		content << "  <p style=\"color: var(--text-muted);\">No extensions loaded</p>\n";
	} else {
		content << "  <ul style=\"list-style: none;\">\n";
		for (const auto& ext : extensions) {
			content << "    <li style=\"padding: 5px 0;\">" << escapeHtml(std::string(ext->getName())) << "</li>\n";
		}
		content << "  </ul>\n";
	}
	content << "</div>\n";

	return wrapPage("Machine", content.str(), DebugInfoType::MACHINE);
}

// ============================================================================
// I/O Page
// ============================================================================

std::string HtmlGenerator::generateIOPage(DebugInfoProvider& provider)
{
	std::ostringstream content;
	MSXMotherBoard* board = provider.getMotherBoard();

	if (!board) {
		content << "<div class=\"section red\">\n";
		content << "  <h2>No Machine</h2>\n";
		content << "  <p>No machine is currently loaded.</p>\n";
		content << "</div>\n";
		return wrapPage("I/O", content.str(), DebugInfoType::IO);
	}

	auto& cpuInterface = board->getCPUInterface();

	// Primary Slot Selection
	content << "<div class=\"section\">\n";
	content << "  <h2>Primary Slot Selection (Port A8h)</h2>\n";
	content << "  <div class=\"grid-4\" style=\"margin-top: 10px;\">\n";
	for (int page = 0; page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(page);
		content << "    <div class=\"slot-box\">\n";
		content << "      <div class=\"slot-label\">Page " << page << "</div>\n";
		content << "      <div class=\"slot-value\">" << ps << "</div>\n";
		content << "    </div>\n";
	}
	content << "  </div>\n";
	content << "</div>\n";

	// Secondary Slots
	content << "<div class=\"section green\" style=\"margin-top: 20px;\">\n";
	content << "  <h2>Secondary Slot Selection</h2>\n";
	content << "  <table>\n";
	content << "    <tr><th>Slot</th><th>Expanded</th><th>Page 0</th><th>Page 1</th><th>Page 2</th><th>Page 3</th></tr>\n";

	for (int ps = 0; ps < 4; ++ps) {
		bool expanded = cpuInterface.isExpanded(ps);
		content << "    <tr>\n";
		content << "      <td><strong>Slot " << ps << "</strong></td>\n";
		content << "      <td>" << statusDot(expanded) << (expanded ? "Yes" : "No") << "</td>\n";

		for (int page = 0; page < 4; ++page) {
			if (expanded) {
				int currentPs = cpuInterface.getPrimarySlot(page);
				if (currentPs == ps) {
					int ss = cpuInterface.getSecondarySlot(page);
					content << "      <td class=\"hex\">" << ss << "</td>\n";
				} else {
					content << "      <td class=\"hex\">-</td>\n";
				}
			} else {
				content << "      <td>-</td>\n";
			}
		}
		content << "    </tr>\n";
	}

	content << "  </table>\n";
	content << "</div>\n";

	// Expansion Status Summary
	content << "<div class=\"section yellow\" style=\"margin-top: 20px;\">\n";
	content << "  <h2>Expansion Status</h2>\n";
	content << "  <div class=\"grid-4\" style=\"margin-top: 10px;\">\n";
	for (int ps = 0; ps < 4; ++ps) {
		bool expanded = cpuInterface.isExpanded(ps);
		content << "    <div class=\"slot-box\" style=\"background: "
		        << (expanded ? "var(--accent-green)" : "var(--bg-overlay)")
		        << "; color: " << (expanded ? "var(--bg-base)" : "var(--text-muted)") << ";\">\n";
		content << "      <div class=\"slot-label\" style=\"color: inherit;\">Slot " << ps << "</div>\n";
		content << "      <div class=\"slot-value\" style=\"color: inherit;\">" << (expanded ? "EXP" : "-") << "</div>\n";
		content << "    </div>\n";
	}
	content << "  </div>\n";
	content << "</div>\n";

	return wrapPage("I/O", content.str(), DebugInfoType::IO);
}

// ============================================================================
// CPU Page
// ============================================================================

std::string HtmlGenerator::generateCPUPage(DebugInfoProvider& provider)
{
	std::ostringstream content;
	MSXMotherBoard* board = provider.getMotherBoard();

	if (!board) {
		content << "<div class=\"section red\">\n";
		content << "  <h2>No Machine</h2>\n";
		content << "  <p>No machine is currently loaded.</p>\n";
		content << "</div>\n";
		return wrapPage("CPU", content.str(), DebugInfoType::CPU);
	}

	auto& cpu = board->getCPU();
	auto& regs = cpu.getRegisters();
	uint8_t f = regs.getF();

	// CPU Type badge
	content << "<div style=\"text-align: center; margin-bottom: 20px;\">\n";
	content << "  <span class=\"cpu-badge " << (cpu.isR800Active() ? "r800" : "z80") << "\">"
	        << (cpu.isR800Active() ? "R800" : "Z80") << "</span>\n";
	content << "  <span style=\"margin-left: 15px; color: var(--text-muted);\">"
	        << statusDot(board->isPowered())
	        << (board->isPowered() ? "Running" : "Stopped") << "</span>\n";
	content << "</div>\n";

	content << "<div class=\"grid\">\n";

	// Main Registers
	content << "<div class=\"section\">\n";
	content << "  <h2>Main Registers</h2>\n";
	content << "  <div class=\"grid-3\">\n";
	content << registerBox("AF", regs.getAF());
	content << registerBox("BC", regs.getBC());
	content << registerBox("DE", regs.getDE());
	content << registerBox("HL", regs.getHL());
	content << registerBox("IX", regs.getIX());
	content << registerBox("IY", regs.getIY());
	content << "  </div>\n";
	content << "</div>\n";

	// Special Registers
	content << "<div class=\"section green\">\n";
	content << "  <h2>Special Registers</h2>\n";
	content << "  <div class=\"grid-3\">\n";
	content << registerBox("PC", regs.getPC());
	content << registerBox("SP", regs.getSP());
	content << registerBox8("I", regs.getI());
	content << registerBox8("R", regs.getR());
	content << "  </div>\n";
	content << "</div>\n";

	// Alternate Registers
	content << "<div class=\"section yellow\">\n";
	content << "  <h2>Alternate Registers</h2>\n";
	content << "  <div class=\"grid-4\">\n";
	content << registerBox("AF'", regs.getAF2());
	content << registerBox("BC'", regs.getBC2());
	content << registerBox("DE'", regs.getDE2());
	content << registerBox("HL'", regs.getHL2());
	content << "  </div>\n";
	content << "</div>\n";

	// Flags
	content << "<div class=\"section mauve\">\n";
	content << "  <h2>Flags</h2>\n";
	content << "  <div class=\"flags\">\n";
	content << flagBox("S", f & 0x80);
	content << flagBox("Z", f & 0x40);
	content << flagBox("5", f & 0x20);
	content << flagBox("H", f & 0x10);
	content << flagBox("3", f & 0x08);
	content << flagBox("P", f & 0x04);
	content << flagBox("N", f & 0x02);
	content << flagBox("C", f & 0x01);
	content << "  </div>\n";
	content << "  <div style=\"margin-top: 10px; font-size: 0.85em; color: var(--text-muted);\">\n";
	content << "    S=Sign, Z=Zero, H=Half-carry, P=Parity/Overflow, N=Subtract, C=Carry\n";
	content << "  </div>\n";
	content << "</div>\n";

	// Interrupts
	content << "<div class=\"section teal\">\n";
	content << "  <h2>Interrupt State</h2>\n";
	content << "  <div class=\"grid-2\">\n";
	content << "    <div class=\"info-row\"><span class=\"label\">IFF1</span>"
	        << "<span class=\"value\">" << statusDot(regs.getIFF1())
	        << (regs.getIFF1() ? "Enabled" : "Disabled") << "</span></div>\n";
	content << "    <div class=\"info-row\"><span class=\"label\">IFF2</span>"
	        << "<span class=\"value\">" << statusDot(regs.getIFF2())
	        << (regs.getIFF2() ? "Enabled" : "Disabled") << "</span></div>\n";
	content << "    <div class=\"info-row\"><span class=\"label\">IM</span>"
	        << "<span class=\"value\">" << static_cast<int>(regs.getIM()) << "</span></div>\n";
	content << "    <div class=\"info-row\"><span class=\"label\">HALT</span>"
	        << "<span class=\"value\">" << statusDot(regs.getHALT())
	        << (regs.getHALT() ? "Halted" : "Running") << "</span></div>\n";
	content << "  </div>\n";
	content << "</div>\n";

	content << "</div>\n"; // end grid

	return wrapPage("CPU", content.str(), DebugInfoType::CPU);
}

// ============================================================================
// Memory Page
// ============================================================================

std::string HtmlGenerator::generateMemoryPage(DebugInfoProvider& provider,
                                               unsigned start, unsigned size)
{
	std::ostringstream content;
	MSXMotherBoard* board = provider.getMotherBoard();

	if (!board) {
		content << "<div class=\"section red\">\n";
		content << "  <h2>No Machine</h2>\n";
		content << "  <p>No machine is currently loaded.</p>\n";
		content << "</div>\n";
		return wrapPage("Memory", content.str(), DebugInfoType::MEMORY);
	}

	// Clamp parameters
	if (start > 0xFFFF) start = 0xFFFF;
	if (size > 0x1000) size = 0x1000; // Limit to 4KB for HTML view
	if (start + size > 0x10000) size = 0x10000 - start;

	auto& cpuInterface = board->getCPUInterface();
	EmuTime time = board->getCurrentTime();

	// Address input info
	content << "<div class=\"section\" style=\"margin-bottom: 20px;\">\n";
	content << "  <h2>Memory View</h2>\n";
	content << "  <div class=\"info-row\"><span class=\"label\">Start Address</span>"
	        << "<span class=\"value\">" << toHex16(static_cast<uint16_t>(start)) << "h</span></div>\n";
	content << "  <div class=\"info-row\"><span class=\"label\">Size</span>"
	        << "<span class=\"value\">" << size << " bytes</span></div>\n";
	content << "  <div style=\"margin-top: 10px; font-size: 0.85em; color: var(--text-muted);\">\n";
	content << "    Use query parameters: <code>?start=0x0000&size=256</code>\n";
	content << "  </div>\n";
	content << "</div>\n";

	// Slot info for current range
	unsigned startPage = start / 0x4000;
	unsigned endPage = (start + size - 1) / 0x4000;

	content << "<div class=\"section green\" style=\"margin-bottom: 20px;\">\n";
	content << "  <h2>Slot Information</h2>\n";
	content << "  <table>\n";
	content << "    <tr><th>Page</th><th>Address Range</th><th>Slot</th></tr>\n";

	for (unsigned page = startPage; page <= endPage && page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(static_cast<int>(page));
		int ss = cpuInterface.getSecondarySlot(static_cast<int>(page));
		bool expanded = cpuInterface.isExpanded(ps);

		content << "    <tr>\n";
		content << "      <td>" << page << "</td>\n";
		content << "      <td class=\"addr\">" << toHex16(static_cast<uint16_t>(page * 0x4000))
		        << "h - " << toHex16(static_cast<uint16_t>((page + 1) * 0x4000 - 1)) << "h</td>\n";
		if (expanded) {
			content << "      <td class=\"hex\">" << ps << "-" << ss << "</td>\n";
		} else {
			content << "      <td class=\"hex\">" << ps << "</td>\n";
		}
		content << "    </tr>\n";
	}

	content << "  </table>\n";
	content << "</div>\n";

	// Memory dump
	content << "<div class=\"section yellow\">\n";
	content << "  <h2>Memory Dump</h2>\n";
	content << "  <div class=\"memory-dump\">\n";

	const unsigned bytesPerLine = 16;
	for (unsigned offset = 0; offset < size; offset += bytesPerLine) {
		uint16_t addr = static_cast<uint16_t>(start + offset);
		unsigned lineBytes = std::min(bytesPerLine, size - offset);

		content << "    <div class=\"line\">\n";
		content << "      <span class=\"address\">" << toHex16(addr) << ":</span>\n";
		content << "      <span class=\"bytes\">";

		std::string ascii;
		for (unsigned i = 0; i < lineBytes; ++i) {
			uint8_t value = cpuInterface.peekMem(static_cast<uint16_t>(addr + i), time);
			content << toHex8(value);
			if (i < lineBytes - 1) content << " ";

			// ASCII representation
			if (value >= 32 && value < 127) {
				ascii += static_cast<char>(value);
			} else {
				ascii += '.';
			}
		}

		// Pad if line is short
		for (unsigned i = lineBytes; i < bytesPerLine; ++i) {
			content << "   ";
		}

		content << "</span>\n";
		content << "      <span class=\"ascii\">" << escapeHtml(ascii) << "</span>\n";
		content << "    </div>\n";
	}

	content << "  </div>\n";
	content << "</div>\n";

	return wrapPage("Memory", content.str(), DebugInfoType::MEMORY);
}

// ============================================================================
// Main page generator
// ============================================================================

std::string HtmlGenerator::generatePage(DebugInfoType type,
                                         DebugInfoProvider& provider,
                                         unsigned memStart,
                                         unsigned memSize)
{
	switch (type) {
		case DebugInfoType::MACHINE:
			return generateMachinePage(provider);
		case DebugInfoType::IO:
			return generateIOPage(provider);
		case DebugInfoType::CPU:
			return generateCPUPage(provider);
		case DebugInfoType::MEMORY:
			return generateMemoryPage(provider, memStart, memSize);
	}
	return wrapPage("Error", "<p>Unknown page type</p>", type);
}

} // namespace openmsx
