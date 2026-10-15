#include "NS16550.hh"

#include "serialize.hh"

#include <cassert>

// References: TI TL16C550C datasheet (SLLS177), register descriptions,
// table 7-5 (interrupt control functions) and the master reset table;
// MAME ins8250.cpp (ns16550_device) for comparison.

namespace openmsx {

static constexpr uint8_t LCR_DLAB   = 0x80;
static constexpr uint8_t LCR_FRAME  = 0x3F; // word length, stop bits, parity

static constexpr uint8_t LSR_DR   = 0x01;
static constexpr uint8_t LSR_OE   = 0x02;
static constexpr uint8_t LSR_THRE = 0x20;
static constexpr uint8_t LSR_TEMT = 0x40;

static constexpr uint8_t FCR_ENABLE   = 0x01;
static constexpr uint8_t FCR_RX_CLEAR = 0x02;
static constexpr uint8_t FCR_TX_CLEAR = 0x04;
static constexpr uint8_t FCR_STORED   = 0xC9; // trigger level, DMA mode, enable

static constexpr uint8_t IER_RDA  = 0x01; // also enables the character time-out
static constexpr uint8_t IER_THRE = 0x02;
static constexpr uint8_t IER_RLS  = 0x04;
static constexpr uint8_t IER_MS   = 0x08;

static constexpr uint8_t IIR_NONE = 0x01;
static constexpr uint8_t IIR_RLS  = 0x06;
static constexpr uint8_t IIR_RDA  = 0x04;
static constexpr uint8_t IIR_CTI  = 0x0C;
static constexpr uint8_t IIR_THRE = 0x02;
static constexpr uint8_t IIR_MS   = 0x00;

static constexpr uint8_t MCR_DTR  = 0x01;
static constexpr uint8_t MCR_RTS  = 0x02;
static constexpr uint8_t MCR_OUT1 = 0x04;
static constexpr uint8_t MCR_OUT2 = 0x08;
static constexpr uint8_t MCR_MASK = 0x3F; // bits 6-7 read as 0, bit 5 = AFE

static constexpr uint8_t MSR_CTS = 0x10;
static constexpr uint8_t MSR_DSR = 0x20;
static constexpr uint8_t MSR_RI  = 0x40;
static constexpr uint8_t MSR_DCD = 0x80;

NS16550::NS16550(Scheduler& scheduler, NS16550Interface& interface_,
                 double clockHz, EmuTime time)
	: syncRecv   (scheduler)
	, syncTrans  (scheduler)
	, syncTimeout(scheduler)
	, syncModem  (scheduler)
	, syncCTS    (scheduler)
	, interface(interface_)
	, clockPeriod(EmuDuration::hz(clockHz))
{
	powerUp(time);
}

void NS16550::powerUp(EmuTime time)
{
	// not touched by a master reset, undefined at power-up
	divisor = 0;
	scr = 0;
	rbr = 0;
	reset(time);
}

void NS16550::reset(EmuTime time)
{
	// master reset: IER, IIR, FCR, LCR, MCR, LSR, MSR deltas, both FIFOs and
	// the INTRPT sources; divisor latch, SCR, RBR and THR are not affected
	ier = 0;
	fcr = 0;
	lcr = 0;
	lsrErrors = 0;
	clearRecvFifo();
	clearTransFifo();
	rbrFull = false;

	syncRecv.removeSyncPoint();
	syncTrans.removeSyncPoint();
	syncTimeout.removeSyncPoint();
	syncModem.removeSyncPoint();
	syncCTS.removeSyncPoint();
	recvReady = true;
	recvPending = false;
	txShifting = false;
	threLatch = false;
	ctiFlag = false;
	rtsHeld = false;
	txWaitCTS = false;

	updateFrame();
	mcr = 0;
	sendOutputs(time);
	msrInputs = modemInputs(time);
	msrDelta = 0;

	intState = false;
	interface.setINT(false, time);
	interface.signal(time);
}

std::optional<EmuDuration> NS16550::charDuration() const
{
	if (divisor == 0) return {}; // baud generator stopped
	unsigned dataBits = 5 + (lcr & 0x03);
	unsigned parity = (lcr & 0x08) ? 1 : 0;
	unsigned stopHalf = (lcr & 0x04) ? ((dataBits == 5) ? 3 : 4) : 2;
	unsigned halfBits = 2 * (1 + dataBits + parity) + stopHalf;
	return clockPeriod * (16u * divisor * halfBits) / 2u;
}

void NS16550::retime(EmuTime time)
{
	// a new divisor or frame restarts the pending character time
	auto dur = charDuration();
	if (recvPending) {
		syncRecv.removeSyncPoint();
		if (dur) syncRecv.setSyncPoint(time + *dur);
	}
	if (txShifting) {
		syncTrans.removeSyncPoint();
		if (dur) syncTrans.setSyncPoint(time + *dur);
	}
	armTimeout(time);
	if (isRecvReady()) interface.signal(time);
}

void NS16550::updateFrame()
{
	auto dataBits = [&] {
		switch (lcr & 0x03) {
		using enum SerialDataInterface::DataBits;
		case 0:  return D5;
		case 1:  return D6;
		case 2:  return D7;
		default: return D8;
		}
	}();
	interface.setDataBits(dataBits);

	using enum SerialDataInterface::StopBits;
	interface.setStopBits(!(lcr & 0x04) ? S1 : ((lcr & 0x03) == 0 ? S1_5 : S2));

	// LCR bit 5 (stick parity) is not representable; even/odd only
	interface.setParityBit((lcr & 0x08) != 0,
		(lcr & 0x10) ? SerialDataInterface::Parity::EVEN
		             : SerialDataInterface::Parity::ODD);
}

uint8_t NS16550::readIO(uint8_t port, EmuTime time)
{
	sampleModem(time);
	uint8_t result;
	switch (port & 7) {
	case 0:
		result = (lcr & LCR_DLAB) ? uint8_t(divisor & 0xFF) : readRBR(time);
		break;
	case 2:
		result = calcIIR();
		// reading the IIR clears THRE when that is the reported source
		if ((result & 0x0F) == IIR_THRE) threLatch = false;
		break;
	case 5:
		result = calcLSR();
		lsrErrors = 0;
		break;
	case 6:
		result = msrInputs | msrDelta;
		msrDelta = 0;
		break;
	default:
		result = peekIO(port, time);
		break;
	}
	resumeTrans(time);
	updateIRQ(time);
	return result;
}

uint8_t NS16550::peekIO(uint8_t port, EmuTime time) const
{
	switch (port & 7) {
	case 0: return (lcr & LCR_DLAB) ? uint8_t(divisor & 0xFF) : peekRBR();
	case 1: return (lcr & LCR_DLAB) ? uint8_t(divisor >> 8) : uint8_t(ier & 0x0F);
	case 2: return calcIIR();
	case 3: return lcr;
	case 4: return mcr;
	case 5: return calcLSR();
	case 6: { // what a read would return, without committing anything
		uint8_t in = modemInputs(time);
		uint8_t delta = msrDelta | modemDeltas(msrInputs, in);
		if (afe()) delta &= ~0x01; // no dCTS with auto flow control
		return in | delta;
	}
	case 7: return scr;
	}
	return 0xFF; // unreachable
}

void NS16550::writeIO(uint8_t port, uint8_t value, EmuTime time)
{
	sampleModem(time);
	switch (port & 7) {
	case 0:
		if (lcr & LCR_DLAB) {
			uint16_t old = divisor;
			divisor = uint16_t((divisor & 0xFF00) | value);
			if (divisor != old) retime(time);
		} else {
			writeTHR(value, time);
		}
		break;
	case 1:
		if (lcr & LCR_DLAB) {
			uint16_t old = divisor;
			divisor = uint16_t((divisor & 0x00FF) | (value << 8));
			if (divisor != old) retime(time);
		} else {
			writeIER(value, time);
		}
		break;
	case 2:
		writeFCR(value, time);
		break;
	case 3: {
		uint8_t old = lcr;
		lcr = value;
		if ((old ^ value) & LCR_FRAME) {
			updateFrame();
			retime(time);
		}
		break;
	}
	case 4:
		writeMCR(value, time);
		break;
	case 5: // LSR: factory test only
	case 6: // MSR: read only
		break;
	case 7:
		scr = value;
		break;
	}
	resumeTrans(time); // after MCR/FCR changes (AFE off, CTS in loopback)
	updateIRQ(time);
}

void NS16550::writeIER(uint8_t value, EmuTime time)
{
	ier = value & 0x0F;
	// like MAME: writing IER with the THRE interrupt enabled while the
	// transmit holding register is empty raises it (the datasheet is silent)
	if ((ier & IER_THRE) && txCount == 0) threLatch = true;
	if (ier & IER_MS) {
		syncModem.removeSyncPoint();
		syncModem.setSyncPoint(time + EmuDuration::msec(1));
	} else {
		syncModem.removeSyncPoint();
	}
}

void NS16550::writeFCR(uint8_t value, EmuTime time)
{
	bool recvCleared = false;
	if (((value ^ fcr) & FCR_ENABLE) != 0) {
		// changing FCR0 clears both FIFOs; the first THRE interrupt after
		// it is immediate
		clearRecvFifo();
		clearTransFifo();
		rbrFull = false;
		recvCleared = true;
		threLatch = true;
		txWaitCTS = false;
		syncCTS.removeSyncPoint();
	}
	if (!(value & FCR_ENABLE)) {
		fcr = 0; // other bits are not programmed without FCR0
	} else {
		fcr = value & FCR_STORED;
		if (value & FCR_RX_CLEAR) {
			clearRecvFifo();
			recvCleared = true;
		}
		if (value & FCR_TX_CLEAR) {
			clearTransFifo(); // the shift register is not affected
			threLatch = true;
			txWaitCTS = false;
			syncCTS.removeSyncPoint();
		}
	}
	// new trigger level / FIFO state: re-evaluate auto-RTS before signal()
	bool held = rtsHeld;
	updateHold();
	if (held != rtsHeld) sendOutputs(time);
	if (recvCleared) {
		ctiFlag = false;
		syncTimeout.removeSyncPoint();
	}
	// RTS may be active again: don't wait for the device's own event
	if (isRecvReady()) interface.signal(time);
}

void NS16550::writeMCR(uint8_t value, EmuTime time)
{
	mcr = value & MCR_MASK;
	updateHold();
	sendOutputs(time);
	// in loopback the modem inputs follow MCR: see the change right away
	sampleModem(time);
	// the line may be connected again (loopback off) or RTS active again
	if (isRecvReady()) interface.signal(time);
}

void NS16550::sendOutputs(EmuTime time)
{
	// forwarded on every change of MCR; the pluggables ignore repeated
	// values. In loopback the outputs are forced inactive.
	bool loop = loopback();
	interface.setDTR(!loop && (mcr & MCR_DTR), time);
	interface.setRTS(!loop && (mcr & MCR_RTS) && !(autoRTS() && rtsHeld), time);
}

void NS16550::syncOutputs(EmuTime time)
{
	updateFrame();
	sendOutputs(time);
	msrInputs = modemInputs(time); // new device = new reference, no deltas
	if (isRecvReady()) interface.signal(time);
	resumeTrans(time);
	updateIRQ(time);
}

uint8_t NS16550::modemInputs(EmuTime time) const
{
	uint8_t result = 0;
	if (loopback()) {
		if (mcr & MCR_RTS)  result |= MSR_CTS;
		if (mcr & MCR_DTR)  result |= MSR_DSR;
		if (mcr & MCR_OUT1) result |= MSR_RI;
		if (mcr & MCR_OUT2) result |= MSR_DCD;
	} else {
		if (interface.getCTS(time)) result |= MSR_CTS;
		if (interface.getDSR(time)) result |= MSR_DSR;
		if (interface.getRI (time)) result |= MSR_RI;
		if (interface.getDCD(time)) result |= MSR_DCD;
	}
	return result;
}

uint8_t NS16550::modemDeltas(uint8_t oldIn, uint8_t newIn)
{
	uint8_t changed = oldIn ^ newIn;
	uint8_t delta = 0;
	if (changed & MSR_CTS) delta |= 0x01;
	if (changed & MSR_DSR) delta |= 0x02;
	if ((oldIn & MSR_RI) && !(newIn & MSR_RI)) delta |= 0x04; // TERI
	if (changed & MSR_DCD) delta |= 0x08;
	return delta;
}

void NS16550::sampleModem(EmuTime time)
{
	uint8_t in = modemInputs(time);
	msrDelta |= modemDeltas(msrInputs, in);
	msrInputs = in;
	// with auto flow control CTS changes cause no interrupt (dCTS cleared)
	if (afe()) msrDelta &= ~0x01;
}

void NS16550::execModem(EmuTime time)
{
	// the plugged devices do not report line changes: look every 1 ms
	// while the modem status interrupt is enabled
	sampleModem(time);
	if (ier & IER_MS) syncModem.setSyncPoint(time + EmuDuration::msec(1));
	updateIRQ(time);
}

void NS16550::updateHold()
{
	// auto-RTS (TL16C550C 7.2, figures 7-3/7-4):
	//   trigger 1/4/8: deassert when the FIFO reaches the trigger level,
	//                  reassert once reading has emptied it
	//   trigger 14:    deassert once the 16th byte is in (its first data bit
	//                  stops the sender after that byte), reassert as soon as
	//                  there is room again
	if (!autoRTS()) {
		rtsHeld = false;
	} else if (triggerLevel() == 14) {
		rtsHeld = (rxCount >= FIFO_SIZE);
	} else if (rxCount >= triggerLevel()) {
		rtsHeld = true;
	} else if (rxCount == 0) {
		rtsHeld = false;
	}
}

void NS16550::scheduleCTSPoll(EmuTime time)
{
	// never more than one pending poll (CTS can be read twice in a row
	// with a different result when a host thread updates it)
	syncCTS.removeSyncPoint();
	syncCTS.setSyncPoint(time + EmuDuration::msec(1));
}

bool NS16550::ctsActive(EmuTime time) const
{
	return (modemInputs(time) & MSR_CTS) != 0;
}

void NS16550::resumeTrans(EmuTime time)
{
	if (!txWaitCTS || txShifting) return;
	if (txCount == 0) {
		txWaitCTS = false;
		syncCTS.removeSyncPoint();
	} else if (!afe() || ctsActive(time)) {
		txWaitCTS = false;
		syncCTS.removeSyncPoint();
		startTrans(time);
	}
}

void NS16550::execCTS(EmuTime time)
{
	// auto-CTS: the plugged devices do not report line changes, look
	// every 1 ms while a byte waits for CTS
	resumeTrans(time);
	if (txWaitCTS) scheduleCTSPoll(time);
	updateIRQ(time);
}

void NS16550::writeTHR(uint8_t value, EmuTime time)
{
	threLatch = false; // before startTrans(), which may set it again
	if (fifoEnabled()) {
		if (txCount < FIFO_SIZE) {
			txFifo[(txHead + txCount) % FIFO_SIZE] = value;
			++txCount;
		} // full: the byte is lost
	} else {
		// single holding register: a new write replaces an unsent byte
		if (txCount == 0) {
			txFifo[txHead] = value;
			txCount = 1;
		} else {
			txFifo[txHead] = value;
		}
	}
	if (!txShifting) startTrans(time);
}

void NS16550::startTrans(EmuTime time)
{
	if (txCount == 0) return;
	if (afe() && !ctsActive(time)) {
		// auto-CTS: the next byte stays in the FIFO until CTS is active
		if (!txWaitCTS) {
			txWaitCTS = true;
			scheduleCTSPoll(time);
		}
		return;
	}
	txWaitCTS = false;
	syncCTS.removeSyncPoint();
	txShift = txFifo[txHead];
	txHead = uint8_t((txHead + 1) % FIFO_SIZE);
	--txCount;
	txShifting = true;
	if (txCount == 0) threLatch = true; // holding register / FIFO empty
	if (auto dur = charDuration()) {
		syncTrans.setSyncPoint(time + *dur);
	} // divisor 0: waits until a divisor is written (retime)
}

void NS16550::execTrans(EmuTime time)
{
	assert(txShifting);
	txShifting = false;
	if (loopback()) {
		storeByte(txShift, time); // TSR output looped into the receiver
	} else {
		interface.recvByte(txShift, time);
	}
	startTrans(time);
	updateIRQ(time);
}

bool NS16550::isRecvFull() const
{
	return fifoEnabled() ? (rxCount == FIFO_SIZE) : rbrFull;
}

void NS16550::storeByte(uint8_t value, EmuTime time)
{
	if (fifoEnabled()) {
		if (rxCount == FIFO_SIZE) {
			// FIFO full: the character in the shift register is lost
			lsrErrors |= LSR_OE;
		} else {
			rxFifo[(rxHead + rxCount) % FIFO_SIZE] = value;
			++rxCount;
		}
		// auto-RTS: a growing FIFO can only set the hold (never call
		// setRTS(true) here: the device holds its lock while delivering)
		bool held = rtsHeld;
		updateHold();
		if (held != rtsHeld) sendOutputs(time);
		// a new character restarts the time-out timer (an occurred
		// time-out is only cleared by reading the FIFO)
		if (!ctiFlag) armTimeout(time);
	} else {
		// RBR is overwritten by the next character
		if (rbrFull) lsrErrors |= LSR_OE;
		rbr = value;
		rbrFull = true;
	}
}

void NS16550::recvByte(uint8_t value, EmuTime time)
{
	assert(isRecvReady());
	storeByte(value, time);
	recvReady = false;
	recvPending = true;
	if (auto dur = charDuration()) {
		syncRecv.setSyncPoint(time + *dur);
	}
	updateIRQ(time);
}

void NS16550::execRecv(EmuTime time)
{
	recvPending = false;
	recvReady = true;
	interface.signal(time);
	updateIRQ(time);
}

void NS16550::armTimeout(EmuTime time)
{
	// character time-out: 4 character times without input or reads while
	// the FIFO holds data (FIFO mode only)
	syncTimeout.removeSyncPoint();
	if (!fifoEnabled() || rxCount == 0 || ctiFlag) return;
	if (auto dur = charDuration()) {
		syncTimeout.setSyncPoint(time + *dur * 4u);
	}
}

void NS16550::execTimeout(EmuTime time)
{
	if (fifoEnabled() && rxCount != 0) ctiFlag = true;
	updateIRQ(time);
}

uint8_t NS16550::peekRBR() const
{
	if (fifoEnabled()) {
		return rxCount ? rxFifo[rxHead] : 0;
	}
	return rbr;
}

uint8_t NS16550::readRBR(EmuTime time)
{
	uint8_t result = peekRBR();
	if (fifoEnabled()) {
		if (rxCount) {
			rxHead = uint8_t((rxHead + 1) % FIFO_SIZE);
			--rxCount;
		}
		ctiFlag = false; // a read clears the time-out and restarts the timer
		armTimeout(time);
		bool held = rtsHeld;
		updateHold();
		if (held != rtsHeld) sendOutputs(time); // before signal()
	} else {
		rbrFull = false;
	}
	// let the device deliver its next byte right away (like I8251)
	if (isRecvReady()) interface.signal(time);
	return result;
}

uint8_t NS16550::calcLSR() const
{
	bool dr = fifoEnabled() ? (rxCount != 0) : rbrFull;
	bool thre = (txCount == 0);
	uint8_t result = lsrErrors;
	if (dr) result |= LSR_DR;
	if (thre) result |= LSR_THRE;
	if (thre && !txShifting) result |= LSR_TEMT;
	return result;
}

unsigned NS16550::triggerLevel() const
{
	static constexpr std::array<uint8_t, 4> levels = {1, 4, 8, 14};
	return levels[fcr >> 6];
}

uint8_t NS16550::calcIIR() const
{
	// highest priority enabled source (table 7-5); CTI shares the RDA
	// enable bit and, like MAME, is reported as $0C when both are pending
	uint8_t id = IIR_NONE;
	bool rda = fifoEnabled() ? (rxCount >= triggerLevel()) : rbrFull;
	if ((ier & IER_RLS) && lsrErrors) {
		id = IIR_RLS;
	} else if ((ier & IER_RDA) && fifoEnabled() && ctiFlag) {
		id = IIR_CTI;
	} else if ((ier & IER_RDA) && rda) {
		id = IIR_RDA;
	} else if ((ier & IER_THRE) && threLatch) {
		id = IIR_THRE;
	} else if ((ier & IER_MS) && msrDelta) {
		id = IIR_MS;
	}
	return uint8_t(id | (fifoEnabled() ? 0xC0 : 0x00));
}

void NS16550::updateIRQ(EmuTime time)
{
	bool pending = (calcIIR() & 0x01) == 0;
	if (pending != intState) {
		intState = pending;
		interface.setINT(pending, time);
	}
}

void NS16550::clearRecvFifo()
{
	rxHead = 0;
	rxCount = 0;
}

void NS16550::clearTransFifo()
{
	txHead = 0;
	txCount = 0;
}

template<typename Archive>
void NS16550::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("syncRecv",    syncRecv,
	             "syncTrans",   syncTrans,
	             "syncTimeout", syncTimeout,
	             "syncModem",   syncModem,
	             "divisor",     divisor,
	             "ier",         ier,
	             "fcr",         fcr,
	             "lcr",         lcr,
	             "mcr",         mcr,
	             "lsrErrors",   lsrErrors,
	             "scr",         scr,
	             "rbr",         rbr,
	             "rbrFull",     rbrFull);
	ar.serialize_blob("rxFifo", rxFifo);
	ar.serialize_blob("txFifo", txFifo);
	ar.serialize("rxHead",      rxHead,
	             "rxCount",     rxCount,
	             "txHead",      txHead,
	             "txCount",     txCount,
	             "recvReady",   recvReady,
	             "recvPending", recvPending,
	             "txShift",     txShift,
	             "txShifting",  txShifting,
	             "threLatch",   threLatch,
	             "ctiFlag",     ctiFlag,
	             "intState",    intState,
	             "msrInputs",   msrInputs,
	             "msrDelta",    msrDelta,
	             "syncCTS",     syncCTS,
	             "rtsHeld",     rtsHeld,
	             "txWaitCTS",   txWaitCTS);
}
INSTANTIATE_SERIALIZE_METHODS(NS16550);

} // namespace openmsx
