#ifndef DEBUG_INFO_PROVIDER_HH
#define DEBUG_INFO_PROVIDER_HH

#include <cstdint>
#include <mutex>
#include <string>

namespace openmsx {

class Reactor;
class MSXMotherBoard;

/**
 * Provides debug information from the emulator in JSON format.
 * Thread-safe for access from multiple HTTP server threads.
 */
class DebugInfoProvider final
{
public:
	explicit DebugInfoProvider(Reactor& reactor);

	// Thread-safe information collection methods
	[[nodiscard]] std::string getMachineInfo();
	[[nodiscard]] std::string getIOInfo();
	[[nodiscard]] std::string getCPUInfo();
	[[nodiscard]] std::string getMemoryInfo(unsigned start, unsigned size);

	// Get active motherboard (may be nullptr)
	[[nodiscard]] MSXMotherBoard* getMotherBoard();

private:
	// JSON formatting helpers
	[[nodiscard]] static std::string jsonEscape(const std::string& s);
	[[nodiscard]] static std::string toHex8(uint8_t value);
	[[nodiscard]] static std::string toHex16(uint16_t value);
	[[nodiscard]] static std::string jsonString(const std::string& key,
	                                            const std::string& value,
	                                            bool last = false);
	[[nodiscard]] static std::string jsonNumber(const std::string& key,
	                                            int value,
	                                            bool last = false);
	[[nodiscard]] static std::string jsonBool(const std::string& key,
	                                          bool value,
	                                          bool last = false);

	// Get current timestamp in milliseconds
	[[nodiscard]] static int64_t getTimestamp();

private:
	Reactor& reactor;
	std::mutex accessMutex;
};

} // namespace openmsx

#endif // DEBUG_INFO_PROVIDER_HH
