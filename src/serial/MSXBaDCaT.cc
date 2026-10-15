#include "MSXBaDCaT.hh"

#include "RS232Device.hh"

#include "BooleanSetting.hh"
#include "serialize.hh"

#include "outer.hh"

#include <memory>
#include <string>
#include <string_view>

namespace openmsx {

// "BaDCaT" -> "badcat", "BaDCaT (1)" -> "badcat_1": a second cartridge (an
// I/O conflict on real hardware, but openMSX allows it) gets its own
// connector and setting names instead of colliding with the first one.
static std::string nameTag(std::string_view name)
{
	std::string result;
	for (char c : name) {
		if (('A' <= c) && (c <= 'Z')) c = char(c - 'A' + 'a');
		bool alnum = (('a' <= c) && (c <= 'z')) || (('0' <= c) && (c <= '9'));
		if (alnum) {
			result += c;
		} else if (!result.empty() && result.back() != '_') {
			result += '_';
		}
	}
	while (!result.empty() && result.back() == '_') result.pop_back();
	return result;
}

MSXBaDCaT::MSXBaDCaT(DeviceConfig& config)
	: MSXDevice(config)
	, RS232Connector(MSXDevice::getPluggingController(),
	                 nameTag(MSXDevice::getName()) + "-rs232")
	, rxBackpressure(std::make_unique<BooleanSetting>(
		getCommandController(), nameTag(MSXDevice::getName()) + "_rx_backpressure",
		"BaDCaT: hold back incoming serial data while the RX FIFO is full "
		"(false = overrun like real hardware)",
		true, Setting::Save::NO))
	, irq(getMotherBoard(), MSXDevice::getName() + ".IRQ")
	, uart(getScheduler(), interface, 1.8432e6, getCurrentTime())
{
	rxBackpressure->attach(*this);
}

MSXBaDCaT::~MSXBaDCaT()
{
	rxBackpressure->detach(*this);
}

void MSXBaDCaT::powerUp(EmuTime time)
{
	uart.powerUp(time);
}

void MSXBaDCaT::reset(EmuTime time)
{
	uart.reset(time);
}

uint8_t MSXBaDCaT::readIO(uint16_t port, EmuTime time)
{
	return uart.readIO(uint8_t(port & 0x07), time);
}

uint8_t MSXBaDCaT::peekIO(uint16_t port, EmuTime time) const
{
	return uart.peekIO(uint8_t(port & 0x07), time);
}

void MSXBaDCaT::writeIO(uint16_t port, uint8_t value, EmuTime time)
{
	uart.writeIO(uint8_t(port & 0x07), value, time);
}

void MSXBaDCaT::plug(Pluggable& device, EmuTime time)
{
	RS232Connector::plug(device, time);
	// the device starts with its own idea of DTR/RTS: send the real ones
	uart.syncOutputs(time);
}

void MSXBaDCaT::update(const Setting& /*setting*/) noexcept
{
	// switching backpressure off may unblock a waiting byte
	if (!rxBackpressure->getBoolean() && uart.isRecvReady()) {
		getPluggedRS232Dev().signal(getCurrentTime());
	}
}


// NS16550Interface (pass calls from the UART to the plugged device)

void MSXBaDCaT::Interface::setDTR(bool status, EmuTime time)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	badcat.getPluggedRS232Dev().setDTR(status, time);
}

void MSXBaDCaT::Interface::setRTS(bool status, EmuTime time)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	badcat.getPluggedRS232Dev().setRTS(status, time);
}

bool MSXBaDCaT::Interface::getCTS(EmuTime time)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	return badcat.getPluggedRS232Dev().getCTS(time).value_or(false);
}

bool MSXBaDCaT::Interface::getDSR(EmuTime time)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	return badcat.getPluggedRS232Dev().getDSR(time).value_or(false);
}

bool MSXBaDCaT::Interface::getRI(EmuTime time)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	return badcat.getPluggedRS232Dev().getRI(time).value_or(false);
}

bool MSXBaDCaT::Interface::getDCD(EmuTime time)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	return badcat.getPluggedRS232Dev().getDCD(time).value_or(false);
}

void MSXBaDCaT::Interface::setDataBits(DataBits bits)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	badcat.getPluggedRS232Dev().setDataBits(bits);
}

void MSXBaDCaT::Interface::setStopBits(StopBits bits)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	badcat.getPluggedRS232Dev().setStopBits(bits);
}

void MSXBaDCaT::Interface::setParityBit(bool enable, Parity parity)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	badcat.getPluggedRS232Dev().setParityBit(enable, parity);
}

void MSXBaDCaT::Interface::recvByte(uint8_t value, EmuTime time)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	badcat.getPluggedRS232Dev().recvByte(value, time);
}

void MSXBaDCaT::Interface::signal(EmuTime time)
{
	const auto& badcat = OUTER(MSXBaDCaT, interface);
	badcat.getPluggedRS232Dev().signal(time);
}

void MSXBaDCaT::Interface::setINT(bool status, EmuTime /*time*/)
{
	auto& badcat = OUTER(MSXBaDCaT, interface);
	badcat.irq.set(status);
}


// RS232Connector input

void MSXBaDCaT::setDataBits(DataBits /*bits*/)
{
	// frame format of incoming data is not checked
}

void MSXBaDCaT::setStopBits(StopBits /*bits*/)
{
}

void MSXBaDCaT::setParityBit(bool /*enable*/, Parity /*parity*/)
{
}

void MSXBaDCaT::recvByte(uint8_t value, EmuTime time)
{
	uart.recvByte(value, time);
}

bool MSXBaDCaT::ready()
{
	return uart.isRecvReady();
}

bool MSXBaDCaT::acceptsData()
{
	return true; // a 16550 has no receiver enable bit
}

bool MSXBaDCaT::isRxBusy() const
{
	return rxBackpressure->getBoolean() && uart.isRecvFull();
}

// version 1: initial version
template<typename Archive>
void MSXBaDCaT::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.template serializeBase<RS232Connector>(*this);
	ar.serialize("uart", uart,
	             "irq",  irq);
	// don't serialize interface; rxBackpressure is a setting (not saved,
	// like the MSXRS232 switch setting)
}
INSTANTIATE_SERIALIZE_METHODS(MSXBaDCaT);
REGISTER_MSXDEVICE(MSXBaDCaT, "BaDCaT");

} // namespace openmsx
