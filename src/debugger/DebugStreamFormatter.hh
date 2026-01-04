#ifndef DEBUG_STREAM_FORMATTER_HH
#define DEBUG_STREAM_FORMATTER_HH

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace openmsx {

class Reactor;
class MSXMotherBoard;

/**
 * Generates debug information in JSON Lines format
 * according to OUTPUT_SPEC_V01.md specification.
 *
 * Output format:
 *   {"emu":"msx","cat":"<category>","sec":"<section>","fld":"<field>","val":"<value>"}
 *
 * Categories:
 *   cpu  - CPU registers, flags, interrupt state
 *   mem  - Memory read/write, slot mapping
 *   io   - I/O port access
 *   mach - Machine info, status
 *   dbg  - Breakpoints, trace
 *   sys  - Connection events, errors
 */
class DebugStreamFormatter final
{
public:
	explicit DebugStreamFormatter(Reactor& reactor);

	//-------------------------------------------------------------------------
	// System messages (cat: sys)
	//-------------------------------------------------------------------------
	[[nodiscard]] std::string getHelloMessage();
	[[nodiscard]] std::string getGoodbyeMessage();

	//-------------------------------------------------------------------------
	// Full state snapshot (for initial connection)
	//-------------------------------------------------------------------------
	[[nodiscard]] std::vector<std::string> getFullSnapshot();

	//-------------------------------------------------------------------------
	// CPU information (cat: cpu)
	//-------------------------------------------------------------------------
	[[nodiscard]] std::string getCPURegisters();
	[[nodiscard]] std::string getCPUFlags();
	[[nodiscard]] std::string getCPUState();

	//-------------------------------------------------------------------------
	// Memory information (cat: mem)
	//-------------------------------------------------------------------------
	[[nodiscard]] std::string getMemoryRead(uint16_t addr, uint8_t value);
	[[nodiscard]] std::string getMemoryWrite(uint16_t addr, uint8_t value);
	[[nodiscard]] std::string getMemoryBank();
	[[nodiscard]] std::string getSlotChange(int page, int primary, int secondary, bool expanded);

	//-------------------------------------------------------------------------
	// I/O port information (cat: io)
	//-------------------------------------------------------------------------
	[[nodiscard]] std::string getIOPortRead(uint8_t port, uint8_t value);
	[[nodiscard]] std::string getIOPortWrite(uint8_t port, uint8_t value);

	//-------------------------------------------------------------------------
	// CPU register updates (for real-time streaming)
	//-------------------------------------------------------------------------
	[[nodiscard]] std::string getRegisterUpdate(const char* reg, uint16_t value);
	[[nodiscard]] std::string getRegister8Update(const char* reg, uint8_t value);
	[[nodiscard]] std::string getFlagUpdate(const char* flag, bool value);

	// Direct register snapshot (called from CPUCore with actual register values)
	[[nodiscard]] std::string getCPURegistersSnapshot(
		uint16_t af, uint16_t bc, uint16_t de, uint16_t hl,
		uint16_t ix, uint16_t iy, uint16_t sp, uint16_t pc);

	//-------------------------------------------------------------------------
	// Machine information (cat: mach)
	//-------------------------------------------------------------------------
	[[nodiscard]] std::string getMachineInfo();
	[[nodiscard]] std::string getMachineStatus(const std::string& mode);

	//-------------------------------------------------------------------------
	// Debug information (cat: dbg)
	//-------------------------------------------------------------------------
	[[nodiscard]] std::string getBreakpointHit(int index, uint16_t addr);
	[[nodiscard]] std::string getWatchpointHit(int index, uint16_t addr, const char* type);
	[[nodiscard]] std::string getTraceExec(uint16_t addr, const std::string& disasm);

	//-------------------------------------------------------------------------
	// Text screen (cat: mem, sec: text) - TEXT1/TEXT2 modes only
	//-------------------------------------------------------------------------
	/**
	 * Get all text screen rows (24 rows for TEXT1/TEXT2 modes).
	 * Returns empty vector if not in text mode.
	 * Each string is a JSON line: {"emu":"msx","cat":"mem","sec":"text","fld":"row","val":"...","idx":"N","addr":"XXXX"}
	 */
	[[nodiscard]] std::vector<std::string> getTextScreen();

	/**
	 * Get a single text screen row.
	 * Returns empty string if not in text mode or row out of range.
	 */
	[[nodiscard]] std::string getTextScreenRow(int row);

	/**
	 * Get current screen mode info.
	 * Returns mode name and whether text extraction is supported.
	 */
	[[nodiscard]] std::string getScreenModeInfo();

private:
	// Format a single JSON line
	[[nodiscard]] std::string formatLine(
		const char* cat, const char* sec, const char* fld,
		const std::string& val,
		const std::map<std::string, std::string>& extra = {});

	// Hex formatting helpers
	[[nodiscard]] static std::string toHex8(uint8_t value);
	[[nodiscard]] static std::string toHex16(uint16_t value);

	// Get current timestamp in milliseconds
	[[nodiscard]] static int64_t getTimestamp();

	// Get active motherboard (may be nullptr)
	[[nodiscard]] MSXMotherBoard* getMotherBoard();

	// JSON string escape
	[[nodiscard]] static std::string escapeJson(const std::string& str);

private:
	Reactor& reactor;
	mutable std::mutex accessMutex;

	static constexpr const char* PROTOCOL_VERSION = "1.0";
};

} // namespace openmsx

#endif // DEBUG_STREAM_FORMATTER_HH
