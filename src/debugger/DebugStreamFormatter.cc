#include "DebugStreamFormatter.hh"

#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "CPURegs.hh"
#include "MSXCPUInterface.hh"
#include "MSXDevice.hh"
#include "HardwareConfig.hh"
#include "Version.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "DisplayMode.hh"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace openmsx {

DebugStreamFormatter::DebugStreamFormatter(Reactor& reactor_)
	: reactor(reactor_)
{
}

MSXMotherBoard* DebugStreamFormatter::getMotherBoard()
{
	return reactor.getMotherBoard();
}

std::string DebugStreamFormatter::toHex8(uint8_t value)
{
	std::ostringstream oss;
	oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
	    << static_cast<int>(value);
	return oss.str();
}

std::string DebugStreamFormatter::toHex16(uint16_t value)
{
	std::ostringstream oss;
	oss << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
	    << static_cast<int>(value);
	return oss.str();
}

int64_t DebugStreamFormatter::getTimestamp()
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string DebugStreamFormatter::escapeJson(const std::string& str)
{
	std::string result;
	result.reserve(str.size());
	for (char c : str) {
		switch (c) {
			case '"':  result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\b': result += "\\b"; break;
			case '\f': result += "\\f"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) {
					std::ostringstream oss;
					oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
					    << static_cast<int>(static_cast<unsigned char>(c));
					result += oss.str();
				} else {
					result += c;
				}
		}
	}
	return result;
}

std::string DebugStreamFormatter::formatLine(
	const char* cat, const char* sec, const char* fld,
	const std::string& val,
	const std::map<std::string, std::string>& extra)
{
	std::ostringstream json;
	json << "{\"emu\":\"msx\""
	     << ",\"cat\":\"" << cat << "\""
	     << ",\"sec\":\"" << sec << "\""
	     << ",\"fld\":\"" << fld << "\""
	     << ",\"val\":\"" << escapeJson(val) << "\"";

	for (const auto& kv : extra) {
		json << ",\"" << kv.first << "\":\"" << escapeJson(kv.second) << "\"";
	}

	json << "}";
	return json.str();
}

//-----------------------------------------------------------------------------
// System messages (cat: sys)
//-----------------------------------------------------------------------------

std::string DebugStreamFormatter::getHelloMessage()
{
	std::lock_guard<std::mutex> lock(accessMutex);
	// Spec: val should be "openMSX <version>", ver is protocol version
	// Version::full() already returns "openMSX <version>"
	return formatLine("sys", "conn", "hello", Version::full(),
		{{"ver", PROTOCOL_VERSION}, {"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getGoodbyeMessage()
{
	std::lock_guard<std::mutex> lock(accessMutex);
	return formatLine("sys", "conn", "goodbye", "disconnecting",
		{{"ts", std::to_string(getTimestamp())}});
}

//-----------------------------------------------------------------------------
// Full state snapshot (for initial connection)
//-----------------------------------------------------------------------------

std::vector<std::string> DebugStreamFormatter::getFullSnapshot()
{
	std::lock_guard<std::mutex> lock(accessMutex);
	std::vector<std::string> lines;

	// ===== System timestamp (NEW - from 65501) =====
	lines.push_back(formatLine("sys", "info", "timestamp",
		std::to_string(getTimestamp())));

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		lines.push_back(formatLine("mach", "info", "status", "no_machine"));
		return lines;
	}

	// ===== Machine info =====
	lines.push_back(formatLine("mach", "info", "id",
		std::string(board->getMachineID())));
	lines.push_back(formatLine("mach", "info", "name",
		std::string(board->getMachineName())));
	lines.push_back(formatLine("mach", "info", "type",
		std::string(board->getMachineType())));
	lines.push_back(formatLine("mach", "info", "status",
		board->isPowered() ? "running" : "powered_off"));

	// ===== Extensions (NEW - from 65501) =====
	const auto& extensions = board->getExtensions();
	int extIdx = 0;
	for (const auto& ext : extensions) {
		std::string idxStr = std::to_string(extIdx);
		lines.push_back(formatLine("mach", "ext", idxStr.c_str(),
			std::string(ext->getName())));
		extIdx++;
	}
	// Also add extension count for convenience
	lines.push_back(formatLine("mach", "ext", "count",
		std::to_string(extensions.size())));

	// ===== CPU type =====
	auto& cpu = board->getCPU();
	lines.push_back(formatLine("cpu", "info", "type",
		cpu.isR800Active() ? "R800" : "Z80"));

	// ===== CPU registers =====
	auto& regs = cpu.getRegisters();

	lines.push_back(formatLine("cpu", "reg", "af", toHex16(regs.getAF())));
	lines.push_back(formatLine("cpu", "reg", "bc", toHex16(regs.getBC())));
	lines.push_back(formatLine("cpu", "reg", "de", toHex16(regs.getDE())));
	lines.push_back(formatLine("cpu", "reg", "hl", toHex16(regs.getHL())));
	lines.push_back(formatLine("cpu", "reg", "af2", toHex16(regs.getAF2())));
	lines.push_back(formatLine("cpu", "reg", "bc2", toHex16(regs.getBC2())));
	lines.push_back(formatLine("cpu", "reg", "de2", toHex16(regs.getDE2())));
	lines.push_back(formatLine("cpu", "reg", "hl2", toHex16(regs.getHL2())));
	lines.push_back(formatLine("cpu", "reg", "ix", toHex16(regs.getIX())));
	lines.push_back(formatLine("cpu", "reg", "iy", toHex16(regs.getIY())));
	lines.push_back(formatLine("cpu", "reg", "sp", toHex16(regs.getSP())));
	lines.push_back(formatLine("cpu", "reg", "pc", toHex16(regs.getPC())));
	lines.push_back(formatLine("cpu", "reg", "i", toHex8(regs.getI())));
	lines.push_back(formatLine("cpu", "reg", "r", toHex8(regs.getR())));

	// ===== Individual 8-bit registers (NEW - from 65501) =====
	lines.push_back(formatLine("cpu", "reg8", "a", toHex8(regs.getA())));
	lines.push_back(formatLine("cpu", "reg8", "f", toHex8(regs.getF())));
	lines.push_back(formatLine("cpu", "reg8", "b", toHex8(regs.getB())));
	lines.push_back(formatLine("cpu", "reg8", "c", toHex8(regs.getC())));
	lines.push_back(formatLine("cpu", "reg8", "d", toHex8(regs.getD())));
	lines.push_back(formatLine("cpu", "reg8", "e", toHex8(regs.getE())));
	lines.push_back(formatLine("cpu", "reg8", "h", toHex8(regs.getH())));
	lines.push_back(formatLine("cpu", "reg8", "l", toHex8(regs.getL())));

	// ===== Flags =====
	uint8_t f = regs.getF();
	std::string flagStr;
	flagStr += (f & 0x80) ? 'S' : '-';  // Sign
	flagStr += (f & 0x40) ? 'Z' : '-';  // Zero
	flagStr += (f & 0x20) ? '5' : '-';  // Undocumented
	flagStr += (f & 0x10) ? 'H' : '-';  // Half-carry
	flagStr += (f & 0x08) ? '3' : '-';  // Undocumented
	flagStr += (f & 0x04) ? 'P' : '-';  // Parity/Overflow
	flagStr += (f & 0x02) ? 'N' : '-';  // Subtract
	flagStr += (f & 0x01) ? 'C' : '-';  // Carry
	lines.push_back(formatLine("cpu", "flags", "all", flagStr,
		{{"raw", toHex8(f)}}));

	// ===== Individual flags (NEW - from 65501) =====
	lines.push_back(formatLine("cpu", "flag", "s", (f & 0x80) ? "1" : "0"));
	lines.push_back(formatLine("cpu", "flag", "z", (f & 0x40) ? "1" : "0"));
	lines.push_back(formatLine("cpu", "flag", "h", (f & 0x10) ? "1" : "0"));
	lines.push_back(formatLine("cpu", "flag", "pv", (f & 0x04) ? "1" : "0"));
	lines.push_back(formatLine("cpu", "flag", "n", (f & 0x02) ? "1" : "0"));
	lines.push_back(formatLine("cpu", "flag", "c", (f & 0x01) ? "1" : "0"));

	// ===== Interrupt state =====
	lines.push_back(formatLine("cpu", "int", "iff1",
		regs.getIFF1() ? "1" : "0"));
	lines.push_back(formatLine("cpu", "int", "iff2",
		regs.getIFF2() ? "1" : "0"));
	lines.push_back(formatLine("cpu", "int", "im",
		std::to_string(static_cast<int>(regs.getIM()))));
	lines.push_back(formatLine("cpu", "int", "halt",
		regs.getHALT() ? "1" : "0"));

	// ===== Slot mapping with device names (ENHANCED - from 65501) =====
	auto& cpuInterface = board->getCPUInterface();
	for (int page = 0; page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(page);
		int ss = cpuInterface.getSecondarySlot(page);
		bool expanded = cpuInterface.isExpanded(ps);

		std::string slotStr = std::to_string(ps);
		if (expanded) {
			slotStr += "-" + std::to_string(ss);
		}
		std::string pageField = "page" + std::to_string(page);

		// Build extra fields with device name
		std::map<std::string, std::string> extra;
		extra["addr"] = toHex16(static_cast<uint16_t>(page * 0x4000));
		extra["expanded"] = expanded ? "1" : "0";

		// Get device name for this slot (NEW - from 65501)
		MSXDevice* device = cpuInterface.getVisibleMSXDevice(page);
		if (device) {
			extra["device"] = std::string(device->getName());
		}

		lines.push_back(formatLine("mem", "slot", pageField.c_str(),
			slotStr, extra));
	}

	// ===== Slot expansion status (NEW - from 65501) =====
	for (int ps = 0; ps < 4; ++ps) {
		std::string slotField = "slot" + std::to_string(ps);
		lines.push_back(formatLine("mem", "expanded", slotField.c_str(),
			cpuInterface.isExpanded(ps) ? "1" : "0"));
	}

	// ===== Video mode info =====
	MSXDevice* vdpDevice = board->findDevice("VDP");
	if (vdpDevice) {
		VDP* vdp = dynamic_cast<VDP*>(vdpDevice);
		if (vdp) {
			DisplayMode mode = vdp->getDisplayMode();
			uint8_t base = mode.getBase();

			std::string modeName;
			bool isText = mode.isTextMode();

			switch (base) {
				case DisplayMode::TEXT1:    modeName = "TEXT1"; break;
				case DisplayMode::TEXT2:    modeName = "TEXT2"; break;
				case DisplayMode::TEXT1Q:   modeName = "TEXT1Q"; break;
				case DisplayMode::GRAPHIC1: modeName = "GRAPHIC1"; break;
				case DisplayMode::GRAPHIC2: modeName = "GRAPHIC2"; break;
				case DisplayMode::GRAPHIC3: modeName = "GRAPHIC3"; break;
				case DisplayMode::GRAPHIC4: modeName = "GRAPHIC4"; break;
				case DisplayMode::GRAPHIC5: modeName = "GRAPHIC5"; break;
				case DisplayMode::GRAPHIC6: modeName = "GRAPHIC6"; break;
				case DisplayMode::GRAPHIC7: modeName = "GRAPHIC7"; break;
				case DisplayMode::MULTICOLOR: modeName = "MULTICOLOR"; break;
				default: modeName = "UNKNOWN"; break;
			}

			lines.push_back(formatLine("mach", "video", "mode", modeName,
				{{"text_support", isText ? "1" : "0"},
				 {"base", toHex8(base)}}));

			// ===== Text screen (only in TEXT modes) =====
			if (isText) {
				VDPVRAM& vram = vdp->getVRAM();
				auto vramData = vram.getData();
				int nameTableBase = vdp->getNameTableBase();
				int columns = (base == DisplayMode::TEXT2) ? 80 : 40;
				int rows = 24;

				for (int row = 0; row < rows; ++row) {
					std::string rowText;
					rowText.reserve(columns);

					int rowAddr = nameTableBase + (row * columns);

					for (int col = 0; col < columns; ++col) {
						int addr = rowAddr + col;
						if (addr < static_cast<int>(vramData.size())) {
							uint8_t charCode = vramData[addr];
							if (charCode >= 32 && charCode < 127) {
								rowText += static_cast<char>(charCode);
							} else {
								rowText += ' ';
							}
						} else {
							rowText += ' ';
						}
					}

					// Pad to fixed width
					rowText.resize(columns, ' ');

					lines.push_back(formatLine("mem", "text", "row", rowText,
						{{"idx", std::to_string(row)},
						 {"addr", toHex16(static_cast<uint16_t>(rowAddr))}}));
				}
			}
		}
	}

	return lines;
}

//-----------------------------------------------------------------------------
// CPU information (cat: cpu)
//-----------------------------------------------------------------------------

std::string DebugStreamFormatter::getCPURegisters()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return formatLine("cpu", "reg", "error", "no_machine");
	}

	auto& cpu = board->getCPU();
	auto& regs = cpu.getRegisters();

	// Format all registers in a single line for efficient streaming
	std::ostringstream val;
	val << "AF=" << toHex16(regs.getAF())
	    << " BC=" << toHex16(regs.getBC())
	    << " DE=" << toHex16(regs.getDE())
	    << " HL=" << toHex16(regs.getHL())
	    << " IX=" << toHex16(regs.getIX())
	    << " IY=" << toHex16(regs.getIY())
	    << " SP=" << toHex16(regs.getSP())
	    << " PC=" << toHex16(regs.getPC());

	return formatLine("cpu", "reg", "all", val.str(),
		{{"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getCPUFlags()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return formatLine("cpu", "flags", "error", "no_machine");
	}

	auto& cpu = board->getCPU();
	auto& regs = cpu.getRegisters();
	uint8_t f = regs.getF();

	std::string flagStr;
	flagStr += (f & 0x80) ? 'S' : '-';
	flagStr += (f & 0x40) ? 'Z' : '-';
	flagStr += (f & 0x20) ? '5' : '-';
	flagStr += (f & 0x10) ? 'H' : '-';
	flagStr += (f & 0x08) ? '3' : '-';
	flagStr += (f & 0x04) ? 'P' : '-';
	flagStr += (f & 0x02) ? 'N' : '-';
	flagStr += (f & 0x01) ? 'C' : '-';

	return formatLine("cpu", "flags", "all", flagStr,
		{{"raw", toHex8(f)}});
}

std::string DebugStreamFormatter::getCPUState()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return formatLine("cpu", "state", "error", "no_machine");
	}

	auto& cpu = board->getCPU();
	auto& regs = cpu.getRegisters();

	std::ostringstream val;
	val << "IFF1=" << (regs.getIFF1() ? "1" : "0")
	    << " IFF2=" << (regs.getIFF2() ? "1" : "0")
	    << " IM=" << static_cast<int>(regs.getIM())
	    << " HALT=" << (regs.getHALT() ? "1" : "0");

	return formatLine("cpu", "state", "int", val.str(),
		{{"type", cpu.isR800Active() ? "R800" : "Z80"}});
}

//-----------------------------------------------------------------------------
// Memory information (cat: mem)
//-----------------------------------------------------------------------------

std::string DebugStreamFormatter::getMemoryRead(uint16_t addr, uint8_t value)
{
	// OUTPUT_SPEC_V01: mem/read/byte
	return formatLine("mem", "read", "byte", toHex8(value),
		{{"addr", toHex16(addr)}, {"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getMemoryWrite(uint16_t addr, uint8_t value)
{
	// OUTPUT_SPEC_V01: mem/write/byte
	return formatLine("mem", "write", "byte", toHex8(value),
		{{"addr", toHex16(addr)}, {"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getSlotChange(int page, int primary, int secondary, bool expanded)
{
	// OUTPUT_SPEC_V01: mem/bank/page_pri, mem/bank/page_sec
	std::string slotStr = std::to_string(primary);
	if (expanded) {
		slotStr += "-" + std::to_string(secondary);
	}
	return formatLine("mem", "bank", "page_pri", std::to_string(primary),
		{{"idx", std::to_string(page)},
		 {"sec", std::to_string(secondary)},
		 {"expanded", expanded ? "1" : "0"},
		 {"ts", std::to_string(getTimestamp())}});
}

//-----------------------------------------------------------------------------
// I/O port information (cat: io)
//-----------------------------------------------------------------------------

std::string DebugStreamFormatter::getIOPortRead(uint8_t port, uint8_t value)
{
	// OUTPUT_SPEC_V01: io/port/read
	return formatLine("io", "port", "read", toHex8(value),
		{{"addr", toHex8(port)}, {"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getIOPortWrite(uint8_t port, uint8_t value)
{
	// OUTPUT_SPEC_V01: io/port/write
	return formatLine("io", "port", "write", toHex8(value),
		{{"addr", toHex8(port)}, {"ts", std::to_string(getTimestamp())}});
}

//-----------------------------------------------------------------------------
// CPU register updates (for real-time streaming)
//-----------------------------------------------------------------------------

std::string DebugStreamFormatter::getRegisterUpdate(const char* reg, uint16_t value)
{
	// OUTPUT_SPEC_V01: cpu/reg/<fld>
	return formatLine("cpu", "reg", reg, toHex16(value),
		{{"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getRegister8Update(const char* reg, uint8_t value)
{
	// OUTPUT_SPEC_V01: cpu/reg/<fld> for 8-bit registers (i, r)
	return formatLine("cpu", "reg", reg, toHex8(value),
		{{"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getFlagUpdate(const char* flag, bool value)
{
	// OUTPUT_SPEC_V01: cpu/flag/<fld>
	return formatLine("cpu", "flag", flag, value ? "1" : "0",
		{{"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getCPURegistersSnapshot(
	uint16_t af, uint16_t bc, uint16_t de, uint16_t hl,
	uint16_t ix, uint16_t iy, uint16_t sp, uint16_t pc)
{
	// Format all registers in a single line for efficient streaming
	// This method receives values directly from CPUCore, avoiding indirect access
	std::ostringstream val;
	val << "AF=" << toHex16(af)
	    << " BC=" << toHex16(bc)
	    << " DE=" << toHex16(de)
	    << " HL=" << toHex16(hl)
	    << " IX=" << toHex16(ix)
	    << " IY=" << toHex16(iy)
	    << " SP=" << toHex16(sp)
	    << " PC=" << toHex16(pc);

	return formatLine("cpu", "reg", "all", val.str(),
		{{"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getMemoryBank()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return formatLine("mem", "slot", "error", "no_machine");
	}

	auto& cpuInterface = board->getCPUInterface();

	std::ostringstream val;
	for (int page = 0; page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(page);
		int ss = cpuInterface.getSecondarySlot(page);
		bool expanded = cpuInterface.isExpanded(ps);

		if (page > 0) val << " ";
		val << "P" << page << "=" << ps;
		if (expanded) {
			val << "-" << ss;
		}
	}

	return formatLine("mem", "slot", "map", val.str(),
		{{"ts", std::to_string(getTimestamp())}});
}

//-----------------------------------------------------------------------------
// Machine information (cat: mach)
//-----------------------------------------------------------------------------

std::string DebugStreamFormatter::getMachineInfo()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return formatLine("mach", "info", "status", "no_machine");
	}

	std::ostringstream val;
	val << std::string(board->getMachineName())
	    << " (" << std::string(board->getMachineType()) << ")";

	return formatLine("mach", "info", "name", val.str(),
		{{"id", std::string(board->getMachineID())}});
}

std::string DebugStreamFormatter::getMachineStatus(const std::string& mode)
{
	std::lock_guard<std::mutex> lock(accessMutex);

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return formatLine("mach", "status", "mode", "no_machine");
	}

	return formatLine("mach", "status", "mode", mode,
		{{"powered", board->isPowered() ? "1" : "0"},
		 {"ts", std::to_string(getTimestamp())}});
}

//-----------------------------------------------------------------------------
// Debug information (cat: dbg)
//-----------------------------------------------------------------------------

std::string DebugStreamFormatter::getBreakpointHit(int index, uint16_t addr)
{
	return formatLine("dbg", "bp", "hit", std::to_string(index),
		{{"addr", toHex16(addr)}, {"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getWatchpointHit(int index, uint16_t addr, const char* type)
{
	return formatLine("dbg", "wp", "hit", std::to_string(index),
		{{"addr", toHex16(addr)}, {"type", type}, {"ts", std::to_string(getTimestamp())}});
}

std::string DebugStreamFormatter::getTraceExec(uint16_t addr, const std::string& disasm)
{
	return formatLine("dbg", "trace", "exec", disasm,
		{{"addr", toHex16(addr)}, {"ts", std::to_string(getTimestamp())}});
}

//-----------------------------------------------------------------------------
// Text screen (cat: mem, sec: text) - TEXT1/TEXT2 modes only
//-----------------------------------------------------------------------------

std::vector<std::string> DebugStreamFormatter::getTextScreen()
{
	std::lock_guard<std::mutex> lock(accessMutex);
	std::vector<std::string> lines;

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return lines;
	}

	// Find VDP device
	MSXDevice* device = board->findDevice("VDP");
	if (!device) {
		return lines;
	}

	VDP* vdp = dynamic_cast<VDP*>(device);
	if (!vdp) {
		return lines;
	}

	DisplayMode mode = vdp->getDisplayMode();
	if (!mode.isTextMode()) {
		// Not in text mode - return empty
		return lines;
	}

	VDPVRAM& vram = vdp->getVRAM();
	auto vramData = vram.getData();

	// Get name table base address from VDP registers
	int nameTableBase = vdp->getNameTableBase();

	// Determine columns based on mode
	// TEXT1 = 40 columns, TEXT2 = 80 columns
	int columns = (mode.getBase() == DisplayMode::TEXT2) ? 80 : 40;
	int rows = 24;

	for (int row = 0; row < rows; ++row) {
		std::string rowText;
		rowText.reserve(columns);

		int rowAddr = nameTableBase + (row * columns);

		for (int col = 0; col < columns; ++col) {
			int addr = rowAddr + col;
			if (addr < static_cast<int>(vramData.size())) {
				uint8_t charCode = vramData[addr];
				// Convert to printable ASCII
				// MSX uses standard ASCII for printable chars (32-126)
				// Non-printable chars are replaced with space
				if (charCode >= 32 && charCode < 127) {
					rowText += static_cast<char>(charCode);
				} else {
					rowText += ' ';
				}
			} else {
				rowText += ' ';
			}
		}

		// Trim trailing spaces for cleaner output
		size_t endPos = rowText.find_last_not_of(' ');
		if (endPos != std::string::npos) {
			rowText = rowText.substr(0, endPos + 1);
		} else {
			rowText.clear(); // All spaces
		}

		// Pad to fixed width for consistent output
		rowText.resize(columns, ' ');

		lines.push_back(formatLine("mem", "text", "row", rowText,
			{{"idx", std::to_string(row)},
			 {"addr", toHex16(static_cast<uint16_t>(rowAddr))}}));
	}

	return lines;
}

std::string DebugStreamFormatter::getTextScreenRow(int row)
{
	std::lock_guard<std::mutex> lock(accessMutex);

	if (row < 0 || row >= 24) {
		return "";
	}

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return "";
	}

	// Find VDP device
	MSXDevice* device = board->findDevice("VDP");
	if (!device) {
		return "";
	}

	VDP* vdp = dynamic_cast<VDP*>(device);
	if (!vdp) {
		return "";
	}

	DisplayMode mode = vdp->getDisplayMode();
	if (!mode.isTextMode()) {
		return "";
	}

	VDPVRAM& vram = vdp->getVRAM();
	auto vramData = vram.getData();

	int nameTableBase = vdp->getNameTableBase();
	int columns = (mode.getBase() == DisplayMode::TEXT2) ? 80 : 40;

	std::string rowText;
	rowText.reserve(columns);

	int rowAddr = nameTableBase + (row * columns);

	for (int col = 0; col < columns; ++col) {
		int addr = rowAddr + col;
		if (addr < static_cast<int>(vramData.size())) {
			uint8_t charCode = vramData[addr];
			if (charCode >= 32 && charCode < 127) {
				rowText += static_cast<char>(charCode);
			} else {
				rowText += ' ';
			}
		} else {
			rowText += ' ';
		}
	}

	// Pad to fixed width
	rowText.resize(columns, ' ');

	return formatLine("mem", "text", "row", rowText,
		{{"idx", std::to_string(row)},
		 {"addr", toHex16(static_cast<uint16_t>(rowAddr))}});
}

std::string DebugStreamFormatter::getScreenModeInfo()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		return formatLine("mach", "video", "mode", "no_machine");
	}

	// Find VDP device
	MSXDevice* device = board->findDevice("VDP");
	if (!device) {
		return formatLine("mach", "video", "mode", "no_vdp");
	}

	VDP* vdp = dynamic_cast<VDP*>(device);
	if (!vdp) {
		return formatLine("mach", "video", "mode", "invalid_vdp");
	}

	DisplayMode mode = vdp->getDisplayMode();
	uint8_t base = mode.getBase();

	std::string modeName;
	std::string textSupport = "0";

	switch (base) {
		case DisplayMode::TEXT1:
			modeName = "TEXT1";
			textSupport = "1";
			break;
		case DisplayMode::TEXT2:
			modeName = "TEXT2";
			textSupport = "1";
			break;
		case DisplayMode::TEXT1Q:
			modeName = "TEXT1Q";
			textSupport = "1";
			break;
		case DisplayMode::GRAPHIC1:
			modeName = "GRAPHIC1";
			break;
		case DisplayMode::GRAPHIC2:
			modeName = "GRAPHIC2";
			break;
		case DisplayMode::GRAPHIC3:
			modeName = "GRAPHIC3";
			break;
		case DisplayMode::GRAPHIC4:
			modeName = "GRAPHIC4";
			break;
		case DisplayMode::GRAPHIC5:
			modeName = "GRAPHIC5";
			break;
		case DisplayMode::GRAPHIC6:
			modeName = "GRAPHIC6";
			break;
		case DisplayMode::GRAPHIC7:
			modeName = "GRAPHIC7";
			break;
		case DisplayMode::MULTICOLOR:
			modeName = "MULTICOLOR";
			break;
		default:
			modeName = "UNKNOWN";
			break;
	}

	return formatLine("mach", "video", "mode", modeName,
		{{"text_support", textSupport},
		 {"base", toHex8(base)}});
}

} // namespace openmsx
