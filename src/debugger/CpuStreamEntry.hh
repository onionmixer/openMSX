#ifndef CPU_STREAM_ENTRY_HH
#define CPU_STREAM_ENTRY_HH

#include <array>
#include <cstdint>

namespace openmsx {

/**
 * Entry structure for CPU stream queue.
 * Contains all data needed to generate debug trace output.
 *
 * This structure is used to pass CPU state from the emulation thread
 * to the debug stream worker thread without blocking. The instruction
 * bytes are pre-fetched in the CPU thread to ensure thread-safety.
 */
struct CpuStreamEntry {
	uint16_t pc;              // Program Counter at instruction start
	uint16_t af, bc, de, hl;  // Main register pairs
	uint16_t ix, iy, sp;      // Index registers and stack pointer
	std::array<uint8_t, 4> opcode;  // Pre-fetched instruction bytes (max 4 for Z80)
	uint8_t opcodeLen;        // Number of valid opcode bytes
	bool valid;               // Entry validity flag

	CpuStreamEntry() : pc(0), af(0), bc(0), de(0), hl(0),
	                   ix(0), iy(0), sp(0), opcode{}, opcodeLen(0), valid(false) {}
};

} // namespace openmsx

#endif // CPU_STREAM_ENTRY_HH
