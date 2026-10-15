// This class implements the 16C550 UART (National NS16550A / TI TL16C550C).
//
// Registers, divisor latch, 16-byte RX/TX FIFOs, line status, character
// timing, interrupts (IER/IIR, FIFO trigger level, character time-out),
// modem status delta bits, loopback (MCR bit 4) and the TL16C550C auto flow
// control (MCR bit 5: auto-RTS / auto-CTS, FIFO mode only).
// Not implemented: parity/framing/break detection, DMA mode signals, and the
// delayed THRE interrupt rule of the FIFO mode.

#ifndef NS16550_HH
#define NS16550_HH

#include "SerialDataInterface.hh"

#include "EmuDuration.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"

#include "outer.hh"

#include <array>
#include <cstdint>
#include <optional>

namespace openmsx {

class NS16550Interface : public SerialDataInterface
{
public:
	virtual void setDTR(bool status, EmuTime time) = 0;
	virtual void setRTS(bool status, EmuTime time) = 0;
	[[nodiscard]] virtual bool getCTS(EmuTime time) = 0;
	[[nodiscard]] virtual bool getDSR(EmuTime time) = 0;
	[[nodiscard]] virtual bool getRI (EmuTime time) = 0;
	[[nodiscard]] virtual bool getDCD(EmuTime time) = 0;
	// ask the connected device to deliver its next byte (if any)
	virtual void signal(EmuTime time) = 0;
	// INTRPT output (active high)
	virtual void setINT(bool status, EmuTime time) = 0;
	// SerialDataInterface::recvByte() = byte transmitted by the UART

protected:
	NS16550Interface() = default;
	~NS16550Interface() = default;
};

class NS16550 final
{
public:
	static constexpr unsigned FIFO_SIZE = 16;

	NS16550(Scheduler& scheduler, NS16550Interface& interface,
	        double clockHz, EmuTime time);

	void powerUp(EmuTime time);
	void reset(EmuTime time); // master reset (MR pin)

	// port = register number 0..7
	[[nodiscard]] uint8_t readIO(uint8_t port, EmuTime time);
	[[nodiscard]] uint8_t peekIO(uint8_t port, EmuTime time) const;
	void writeIO(uint8_t port, uint8_t value, EmuTime time);

	// receiver side, used by the connector
	[[nodiscard]] bool isRecvReady() const {
		return recvReady && divisor != 0 && !loopback();
	}
	[[nodiscard]] bool isRecvFull() const;
	void recvByte(uint8_t value, EmuTime time);

	// forward the current outputs, take the inputs as the new reference
	// and ask for input (after plugging a device)
	void syncOutputs(EmuTime time);

	void execRecv(EmuTime time);
	void execTrans(EmuTime time);
	void execTimeout(EmuTime time);
	void execModem(EmuTime time);
	void execCTS(EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] std::optional<EmuDuration> charDuration() const;
	void retime(EmuTime time);
	void updateFrame();
	void writeFCR(uint8_t value, EmuTime time);
	void writeMCR(uint8_t value, EmuTime time);
	void writeIER(uint8_t value, EmuTime time);
	void writeTHR(uint8_t value, EmuTime time);
	[[nodiscard]] uint8_t readRBR(EmuTime time);
	[[nodiscard]] uint8_t peekRBR() const;
	[[nodiscard]] uint8_t calcLSR() const;
	[[nodiscard]] uint8_t calcIIR() const;
	[[nodiscard]] uint8_t modemInputs(EmuTime time) const;
	[[nodiscard]] static uint8_t modemDeltas(uint8_t oldIn, uint8_t newIn);
	void sampleModem(EmuTime time);
	void sendOutputs(EmuTime time);
	void storeByte(uint8_t value, EmuTime time);
	void armTimeout(EmuTime time);
	void startTrans(EmuTime time);
	void resumeTrans(EmuTime time);
	void scheduleCTSPoll(EmuTime time);
	void updateHold();
	[[nodiscard]] bool ctsActive(EmuTime time) const;
	void clearRecvFifo();
	void clearTransFifo();
	void updateIRQ(EmuTime time);
	[[nodiscard]] bool fifoEnabled() const { return (fcr & 0x01) != 0; }
	[[nodiscard]] bool loopback() const { return (mcr & 0x10) != 0; }
	// TL16C550C auto flow control works in the FIFO mode only
	[[nodiscard]] bool afe() const { return (mcr & 0x20) && fifoEnabled(); }
	[[nodiscard]] bool autoRTS() const { return afe() && (mcr & 0x02); }
	[[nodiscard]] unsigned triggerLevel() const;

	struct SyncRecv final : Schedulable {
		friend class NS16550;
		explicit SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& uart = OUTER(NS16550, syncRecv);
			uart.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans final : Schedulable {
		friend class NS16550;
		explicit SyncTrans(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& uart = OUTER(NS16550, syncTrans);
			uart.execTrans(time);
		}
	} syncTrans;
	struct SyncTimeout final : Schedulable {
		friend class NS16550;
		explicit SyncTimeout(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& uart = OUTER(NS16550, syncTimeout);
			uart.execTimeout(time);
		}
	} syncTimeout;
	struct SyncModem final : Schedulable {
		friend class NS16550;
		explicit SyncModem(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& uart = OUTER(NS16550, syncModem);
			uart.execModem(time);
		}
	} syncModem;
	struct SyncCTS final : Schedulable {
		friend class NS16550;
		explicit SyncCTS(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& uart = OUTER(NS16550, syncCTS);
			uart.execCTS(time);
		}
	} syncCTS;

	NS16550Interface& interface;
	const EmuDuration clockPeriod;

	// registers
	uint16_t divisor = 0;
	uint8_t ier = 0;
	uint8_t fcr = 0; // bit0 FIFO enable, bit3 DMA mode, bits 7-6 RX trigger
	uint8_t lcr = 0;
	uint8_t mcr = 0;
	uint8_t lsrErrors = 0; // OE/PE/FE/BI, cleared by an LSR read
	uint8_t scr = 0;
	uint8_t rbr = 0;       // non-FIFO receive buffer
	bool rbrFull = false;  // non-FIFO data ready

	// FIFOs (ring buffers)
	std::array<uint8_t, FIFO_SIZE> rxFifo = {};
	std::array<uint8_t, FIFO_SIZE> txFifo = {};
	uint8_t rxHead = 0, rxCount = 0;
	uint8_t txHead = 0, txCount = 0;

	// receiver pacing: one byte per character time
	bool recvReady = true;
	bool recvPending = false; // waiting for the end of a character time

	// transmitter shift register
	uint8_t txShift = 0;
	bool txShifting = false;

	// interrupts
	bool threLatch = false; // THRE interrupt source
	bool ctiFlag = false;   // character time-out occurred
	bool intState = false;  // current INTRPT output

	// auto flow control
	bool rtsHeld = false;   // auto-RTS: receiver FIFO asked the sender to stop
	bool txWaitCTS = false; // auto-CTS: next byte waits for CTS

	// modem status: inputs as last seen (bits 4-7) and delta bits (0-3)
	uint8_t msrInputs = 0;
	uint8_t msrDelta = 0;
};
SERIALIZE_CLASS_VERSION(NS16550, 1);

} // namespace openmsx

#endif
