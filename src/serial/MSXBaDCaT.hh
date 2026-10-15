// BaDCaT WiFi modem cartridge (Andres Ortiz): a 16C550C UART at I/O 0x80-0x87
// with a 1.8432 MHz clock; its interrupt output drives the MSX /INT line.
// Only the external RS-232 port is emulated (the
// cartridge's wifi switch in the OFF position); the ESP8266 modem is not.

#ifndef MSXBADCAT_HH
#define MSXBADCAT_HH

#include "IRQHelper.hh"
#include "MSXDevice.hh"
#include "NS16550.hh"
#include "RS232Connector.hh"

#include "Observer.hh"

#include <memory>

namespace openmsx {

class BooleanSetting;
class Setting;

class MSXBaDCaT final : public MSXDevice, public RS232Connector
                      , private Observer<Setting>
{
public:
	explicit MSXBaDCaT(DeviceConfig& config);
	~MSXBaDCaT() override;

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	// Connector
	void plug(Pluggable& device, EmuTime time) override;

	// RS232Connector (input)
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(uint8_t value, EmuTime time) override;
	[[nodiscard]] bool ready() override;
	[[nodiscard]] bool acceptsData() override;
	[[nodiscard]] bool isRxBusy() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	struct Interface final : NS16550Interface {
		void setDTR(bool status, EmuTime time) override;
		void setRTS(bool status, EmuTime time) override;
		[[nodiscard]] bool getCTS(EmuTime time) override;
		[[nodiscard]] bool getDSR(EmuTime time) override;
		[[nodiscard]] bool getRI (EmuTime time) override;
		[[nodiscard]] bool getDCD(EmuTime time) override;
		void setDataBits(DataBits bits) override;
		void setStopBits(StopBits bits) override;
		void setParityBit(bool enable, Parity parity) override;
		void recvByte(uint8_t value, EmuTime time) override;
		void signal(EmuTime time) override;
		void setINT(bool status, EmuTime time) override;
	} interface;

	// true (default): the plugged device waits while the RX FIFO is full,
	// so no byte is ever lost (like the i8251 path in this fork).
	// false: bytes keep arriving at the line rate and overrun like on
	// real hardware.
	const std::unique_ptr<BooleanSetting> rxBackpressure;

	// INTRPT of the UART to the MSX /INT line (not gated by OUT2: BDterm
	// gets interrupts with MCR=$02, TELNETF with MCR=$0F/$2F).
	// Must be constructed before uart (its constructor resets the UART).
	IRQHelper irq;

	NS16550 uart;
};
SERIALIZE_CLASS_VERSION(MSXBaDCaT, 1);

} // namespace openmsx

#endif
