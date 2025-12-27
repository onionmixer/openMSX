#include "DebugInfoProvider.hh"

#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "CPURegs.hh"
#include "MSXCPUInterface.hh"
#include "HardwareConfig.hh"
#include "CartridgeSlotManager.hh"
#include "MSXDevice.hh"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace openmsx {

DebugInfoProvider::DebugInfoProvider(Reactor& reactor_)
	: reactor(reactor_)
{
}

MSXMotherBoard* DebugInfoProvider::getMotherBoard()
{
	return reactor.getMotherBoard();
}

std::string DebugInfoProvider::getMachineInfo()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	std::ostringstream json;
	json << "{\n";
	json << jsonNumber("timestamp", static_cast<int>(getTimestamp()));

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		json << jsonString("status", "no_machine");
		json << jsonString("message", "No machine loaded", true);
		json << "}";
		return json.str();
	}

	json << jsonString("status", board->isPowered() ? "running" : "powered_off");
	json << jsonString("machine_id", std::string(board->getMachineID()));
	json << jsonString("machine_name", std::string(board->getMachineName()));
	json << jsonString("machine_type", std::string(board->getMachineType()));

	// Slot information
	auto& cpuInterface = board->getCPUInterface();
	json << "  \"slots\": {\n";
	for (int page = 0; page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(page);
		int ss = cpuInterface.getSecondarySlot(page);
		bool expanded = cpuInterface.isExpanded(ps);

		json << "    \"page" << page << "\": {\n";
		json << "      \"address\": \"" << toHex16(static_cast<uint16_t>(page * 0x4000)) << "\",\n";
		json << "      \"primary\": " << ps << ",\n";
		json << "      \"secondary\": " << (expanded ? ss : -1) << ",\n";
		json << "      \"expanded\": " << (expanded ? "true" : "false");

		// Get device name for this slot
		MSXDevice* device = cpuInterface.getVisibleMSXDevice(page);
		if (device) {
			json << ",\n      \"device\": \"" << jsonEscape(std::string(device->getName())) << "\"";
		}
		json << "\n    }";
		if (page < 3) json << ",";
		json << "\n";
	}
	json << "  },\n";

	// Extensions
	json << "  \"extensions\": [";
	const auto& extensions = board->getExtensions();
	bool first = true;
	for (const auto& ext : extensions) {
		if (!first) json << ",";
		first = false;
		json << "\n    \"" << jsonEscape(std::string(ext->getName())) << "\"";
	}
	if (!extensions.empty()) json << "\n  ";
	json << "],\n";

	// CPU type
	auto& cpu = board->getCPU();
	json << jsonString("cpu_type", cpu.isR800Active() ? "R800" : "Z80", true);

	json << "}";
	return json.str();
}

std::string DebugInfoProvider::getIOInfo()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	std::ostringstream json;
	json << "{\n";
	json << jsonNumber("timestamp", static_cast<int>(getTimestamp()));

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		json << jsonString("status", "no_machine");
		json << jsonString("message", "No machine loaded", true);
		json << "}";
		return json.str();
	}

	json << jsonString("status", "running");

	// Slot selection register (primary slots at port 0xA8)
	auto& cpuInterface = board->getCPUInterface();

	json << "  \"primary_slots\": {\n";
	for (int page = 0; page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(page);
		json << "    \"page" << page << "\": " << ps;
		if (page < 3) json << ",";
		json << "\n";
	}
	json << "  },\n";

	json << "  \"secondary_slots\": {\n";
	for (int page = 0; page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(page);
		int ss = cpuInterface.getSecondarySlot(page);
		bool expanded = cpuInterface.isExpanded(ps);
		if (expanded) {
			json << "    \"page" << page << "\": " << ss;
		} else {
			json << "    \"page" << page << "\": -1";
		}
		if (page < 3) json << ",";
		json << "\n";
	}
	json << "  },\n";

	// Expansion status
	json << "  \"expanded\": [";
	for (int ps = 0; ps < 4; ++ps) {
		json << (cpuInterface.isExpanded(ps) ? "true" : "false");
		if (ps < 3) json << ", ";
	}
	json << "]\n";

	json << "}";
	return json.str();
}

std::string DebugInfoProvider::getCPUInfo()
{
	std::lock_guard<std::mutex> lock(accessMutex);

	std::ostringstream json;
	json << "{\n";
	json << jsonNumber("timestamp", static_cast<int>(getTimestamp()));

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		json << jsonString("status", "no_machine");
		json << jsonString("message", "No machine loaded", true);
		json << "}";
		return json.str();
	}

	json << jsonString("status", board->isPowered() ? "running" : "powered_off");

	auto& cpu = board->getCPU();
	auto& regs = cpu.getRegisters();

	// Main registers
	json << "  \"registers\": {\n";
	json << "    \"af\": \"" << toHex16(regs.getAF()) << "\",\n";
	json << "    \"bc\": \"" << toHex16(regs.getBC()) << "\",\n";
	json << "    \"de\": \"" << toHex16(regs.getDE()) << "\",\n";
	json << "    \"hl\": \"" << toHex16(regs.getHL()) << "\",\n";
	json << "    \"af2\": \"" << toHex16(regs.getAF2()) << "\",\n";
	json << "    \"bc2\": \"" << toHex16(regs.getBC2()) << "\",\n";
	json << "    \"de2\": \"" << toHex16(regs.getDE2()) << "\",\n";
	json << "    \"hl2\": \"" << toHex16(regs.getHL2()) << "\",\n";
	json << "    \"ix\": \"" << toHex16(regs.getIX()) << "\",\n";
	json << "    \"iy\": \"" << toHex16(regs.getIY()) << "\",\n";
	json << "    \"sp\": \"" << toHex16(regs.getSP()) << "\",\n";
	json << "    \"pc\": \"" << toHex16(regs.getPC()) << "\",\n";
	json << "    \"i\": \"" << toHex8(regs.getI()) << "\",\n";
	json << "    \"r\": \"" << toHex8(regs.getR()) << "\"\n";
	json << "  },\n";

	// Individual 8-bit registers for convenience
	json << "  \"registers_8bit\": {\n";
	json << "    \"a\": \"" << toHex8(regs.getA()) << "\",\n";
	json << "    \"f\": \"" << toHex8(regs.getF()) << "\",\n";
	json << "    \"b\": \"" << toHex8(regs.getB()) << "\",\n";
	json << "    \"c\": \"" << toHex8(regs.getC()) << "\",\n";
	json << "    \"d\": \"" << toHex8(regs.getD()) << "\",\n";
	json << "    \"e\": \"" << toHex8(regs.getE()) << "\",\n";
	json << "    \"h\": \"" << toHex8(regs.getH()) << "\",\n";
	json << "    \"l\": \"" << toHex8(regs.getL()) << "\"\n";
	json << "  },\n";

	// Flags (from F register)
	uint8_t f = regs.getF();
	json << "  \"flags\": {\n";
	json << "    \"s\": " << ((f & 0x80) ? "true" : "false") << ",\n";   // Sign
	json << "    \"z\": " << ((f & 0x40) ? "true" : "false") << ",\n";   // Zero
	json << "    \"f5\": " << ((f & 0x20) ? "true" : "false") << ",\n";  // Undocumented
	json << "    \"h\": " << ((f & 0x10) ? "true" : "false") << ",\n";   // Half-carry
	json << "    \"f3\": " << ((f & 0x08) ? "true" : "false") << ",\n";  // Undocumented
	json << "    \"pv\": " << ((f & 0x04) ? "true" : "false") << ",\n";  // Parity/Overflow
	json << "    \"n\": " << ((f & 0x02) ? "true" : "false") << ",\n";   // Subtract
	json << "    \"c\": " << ((f & 0x01) ? "true" : "false") << "\n";    // Carry
	json << "  },\n";

	// Interrupt state
	json << "  \"interrupts\": {\n";
	json << "    \"iff1\": " << (regs.getIFF1() ? "true" : "false") << ",\n";
	json << "    \"iff2\": " << (regs.getIFF2() ? "true" : "false") << ",\n";
	json << "    \"im\": " << static_cast<int>(regs.getIM()) << ",\n";
	json << "    \"halted\": " << (regs.getHALT() ? "true" : "false") << "\n";
	json << "  },\n";

	// CPU type
	json << jsonString("cpu_type", cpu.isR800Active() ? "R800" : "Z80", true);

	json << "}";
	return json.str();
}

std::string DebugInfoProvider::getMemoryInfo(unsigned start, unsigned size)
{
	std::lock_guard<std::mutex> lock(accessMutex);

	// Clamp parameters
	if (start > 0xFFFF) start = 0xFFFF;
	if (size > 0x10000) size = 0x10000;
	if (start + size > 0x10000) size = 0x10000 - start;

	std::ostringstream json;
	json << "{\n";
	json << jsonNumber("timestamp", static_cast<int>(getTimestamp()));

	MSXMotherBoard* board = getMotherBoard();
	if (!board) {
		json << jsonString("status", "no_machine");
		json << jsonString("message", "No machine loaded", true);
		json << "}";
		return json.str();
	}

	json << jsonString("status", "running");
	json << jsonString("start", toHex16(static_cast<uint16_t>(start)));
	json << jsonNumber("size", static_cast<int>(size));

	// Read actual memory data
	auto& cpuInterface = board->getCPUInterface();
	EmuTime time = board->getCurrentTime();

	json << "  \"data\": \"";
	for (unsigned i = 0; i < size; ++i) {
		uint16_t addr = static_cast<uint16_t>(start + i);
		uint8_t value = cpuInterface.peekMem(addr, time);
		json << toHex8(value);
	}
	json << "\",\n";

	// Also provide slot information for the memory range
	json << "  \"slot_info\": [\n";
	unsigned currentPage = start / 0x4000;
	unsigned endPage = (start + size - 1) / 0x4000;

	for (unsigned page = currentPage; page <= endPage && page < 4; ++page) {
		int ps = cpuInterface.getPrimarySlot(static_cast<int>(page));
		int ss = cpuInterface.getSecondarySlot(static_cast<int>(page));
		bool expanded = cpuInterface.isExpanded(ps);

		json << "    {\n";
		json << "      \"page\": " << page << ",\n";
		json << "      \"address\": \"" << toHex16(static_cast<uint16_t>(page * 0x4000)) << "\",\n";
		json << "      \"primary\": " << ps << ",\n";
		json << "      \"secondary\": " << (expanded ? ss : -1) << "\n";
		json << "    }";
		if (page < endPage && page < 3) json << ",";
		json << "\n";
	}
	json << "  ]\n";

	json << "}";
	return json.str();
}

// JSON formatting helpers
std::string DebugInfoProvider::jsonEscape(const std::string& s)
{
	std::string result;
	result.reserve(s.size());
	for (char c : s) {
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

std::string DebugInfoProvider::toHex8(uint8_t value)
{
	std::ostringstream oss;
	oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
	    << static_cast<int>(value);
	return oss.str();
}

std::string DebugInfoProvider::toHex16(uint16_t value)
{
	std::ostringstream oss;
	oss << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
	    << static_cast<int>(value);
	return oss.str();
}

std::string DebugInfoProvider::jsonString(const std::string& key, const std::string& value, bool last)
{
	std::ostringstream oss;
	oss << "  \"" << key << "\": \"" << jsonEscape(value) << "\"";
	if (!last) oss << ",";
	oss << "\n";
	return oss.str();
}

std::string DebugInfoProvider::jsonNumber(const std::string& key, int value, bool last)
{
	std::ostringstream oss;
	oss << "  \"" << key << "\": " << value;
	if (!last) oss << ",";
	oss << "\n";
	return oss.str();
}

std::string DebugInfoProvider::jsonBool(const std::string& key, bool value, bool last)
{
	std::ostringstream oss;
	oss << "  \"" << key << "\": " << (value ? "true" : "false");
	if (!last) oss << ",";
	oss << "\n";
	return oss.str();
}

int64_t DebugInfoProvider::getTimestamp()
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace openmsx
