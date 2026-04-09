#ifndef RS232HOST_HH
#define RS232HOST_HH

#include "RS232Device.hh"
#include "EventListener.hh"
#include "StringSetting.hh"
#include "Poller.hh"
#include "circular_buffer.hh"

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <thread>
#include <termios.h>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;

class RS232Host final : public RS232Device, private EventListener
{
public:
	RS232Host(EventDistributor& eventDistributor, Scheduler& scheduler,
	          CommandController& commandController);
	~RS232Host() override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;

	// input
	void signal(EmuTime time) override;

	// output
	void recvByte(uint8_t value, EmuTime time) override;

	// control lines
	[[nodiscard]] std::optional<bool> getDSR(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getCTS(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getDCD(EmuTime time) const override;
	void setDTR(bool status, EmuTime time) override;
	void setRTS(bool status, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();  // receiver thread
	bool signalEvent(const Event& event) override;

	EventDistributor& eventDistributor;
	Scheduler& scheduler;

	StringSetting rs232HostDeviceSetting;

	std::thread thread;
	std::mutex mutex;
	std::optional<Poller> poller;
	cb_queue<char> queue;

	std::atomic<int> fd{-1};
	struct termios originalTermios{};

	bool DTR{false};
	bool RTS{true};

	std::atomic<int> cachedModemBits{0};
	std::chrono::steady_clock::time_point lastModemPoll{};
	void updateModemLinesCache();
};

} // namespace openmsx

#endif
