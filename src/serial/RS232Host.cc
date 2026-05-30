#include "RS232Host.hh"

#include "RS232Connector.hh"

#include "EventDistributor.hh"
#include "PlugException.hh"
#include "Scheduler.hh"
#include "serialize.hh"

#include "checked_cast.hh"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace openmsx {

RS232Host::RS232Host(EventDistributor& eventDistributor_,
                     Scheduler& scheduler_,
                     CommandController& commandController)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, rs232HostDeviceSetting(
	        commandController, "rs232-host-device",
	        "Host serial device path for RS232 host pluggable",
	        "")
{
	eventDistributor.registerEventListener(EventType::RS232_NET, *this);
}

RS232Host::~RS232Host()
{
	eventDistributor.unregisterEventListener(EventType::RS232_NET, *this);
}

// Pluggable
void RS232Host::plugHelper(Connector& connector_, EmuTime /*time*/)
{
	auto device = rs232HostDeviceSetting.getString();
	if (device.empty()) {
		throw PlugException("rs232-host-device setting is empty");
	}

	fd = open(std::string(device).c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		throw PlugException("Can't open serial device: ", device,
		                     " (", strerror(errno), ")");
	}

	// Save original termios for restoration on unplug
	tcgetattr(fd, &originalTermios);

	// Configure: raw mode, 9600-8-N-1
	struct termios tty;
	tcgetattr(fd, &tty);
	cfmakeraw(&tty);
	cfsetispeed(&tty, B9600);
	cfsetospeed(&tty, B9600);
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag |= CREAD | CLOCAL;
	tty.c_cflag &= ~CRTSCTS;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;
	tcsetattr(fd, TCSANOW, &tty);

	auto& rs232Connector = checked_cast<RS232Connector&>(connector_);
	rs232Connector.setDataBits(SerialDataInterface::DataBits::D8);
	rs232Connector.setStopBits(SerialDataInterface::StopBits::S1);
	rs232Connector.setParityBit(false, SerialDataInterface::Parity::EVEN);

	DTR = false;
	RTS = true;

	setConnector(&connector_);
	poller.emplace();
	updateModemLinesCache();
	thread = std::thread([this]() { run(); });
}

void RS232Host::updateModemLinesCache()
{
	if (fd < 0) return;
	int bits = 0;
	if (ioctl(fd, TIOCMGET, &bits) == 0) {
		cachedModemBits.store(bits, std::memory_order_relaxed);
	}
	lastModemPoll = std::chrono::steady_clock::now();
}

void RS232Host::unplugHelper(EmuTime /*time*/)
{
	// Stop helper thread
	if (thread.joinable()) {
		poller->abort();
		thread.join();
	}
	poller.reset();

	// Restore termios and close
	if (fd >= 0) {
		tcsetattr(fd, TCSANOW, &originalTermios);
		close(fd);
		fd = -1;
	}
	cachedModemBits.store(0, std::memory_order_relaxed);
}

zstring_view RS232Host::getName() const
{
	return "rs232-host";
}

zstring_view RS232Host::getDescription() const
{
	return "RS232 Host serial pluggable. Connects the RS232 port to a host "
	       "serial device, selected with the 'rs232-host-device' setting.";
}

void RS232Host::run()
{
	static constexpr int kPollTimeoutMs = 200;
	static constexpr auto kModemPollInterval = std::chrono::milliseconds(200);

	while (true) {
		if (fd < 0) break;
		if (poller->aborted()) break;

		struct pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		int pollResult = ::poll(&pfd, 1, kPollTimeoutMs);

		if (poller->aborted()) break;

		// Periodic modem line cache update
		auto now = std::chrono::steady_clock::now();
		if (now - lastModemPoll >= kModemPollInterval) {
			updateModemLinesCache();
		}

		if (pollResult < 0) {
			if (errno == EINTR) continue;
			break; // fatal poll error
		}
		if (pollResult == 0) {
			continue; // timeout, no data
		}

		char b;
		auto n = read(fd, &b, 1);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			}
			close(fd);
			fd = -1;
			break;
		}
		if (n == 0) {
			continue;
		}

		assert(isPluggedIn());
		{
			std::scoped_lock lock(mutex);
			queue.push_back(b);
		}
		eventDistributor.distributeEvent(Rs232NetEvent());
	}
}

// input
void RS232Host::signal(EmuTime time)
{
	auto* conn = checked_cast<RS232Connector*>(getConnector());

	if (!conn->acceptsData()) {
		std::scoped_lock lock(mutex);
		queue.clear();
		return;
	}

	if (!conn->ready()) {
		return;
	}
	if (conn->isRxBusy()) {
		return; // RXRDY still set — defer delivery, byte stays in queue
	}

	std::scoped_lock lock(mutex);
	if (queue.empty()) return;
	char b = queue.pop_front();
	conn->recvByte(b, time);
}

// EventListener
bool RS232Host::signalEvent(const Event& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::scoped_lock lock(mutex);
		queue.clear();
	}
	return false;
}

// output
void RS232Host::recvByte(uint8_t value_, EmuTime /*time*/)
{
	if (fd < 0) return;
	auto value = static_cast<char>(value_);
	// EAGAIN(출력버퍼 일시 full)은 busy-loop 으로 즉시 재시도(D-35). 호스트에서
	// 1바이트씩 + EAGAIN busy-loop 로 보내면 GTM 경유 57B 가 전부 전달됨을 확인.
	// close(reset)/POLLOUT(hang)/blocking(30B) 은 모두 실패, 이 방식만 통과.
	for (;;) {
		ssize_t n = write(fd, &value, 1);
		if (n == 1) return;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			continue;  // drain 될 때까지 즉시 재시도 (상대가 따라잡게)
		}
		close(fd);
		fd = -1;
		return;
	}
}

// Control lines

std::optional<bool> RS232Host::getDSR(EmuTime /*time*/) const
{
	if (fd < 0) return true;
	return (cachedModemBits.load(std::memory_order_relaxed) & TIOCM_DSR) != 0;
}

std::optional<bool> RS232Host::getCTS(EmuTime /*time*/) const
{
	if (fd < 0) return true;
	return (cachedModemBits.load(std::memory_order_relaxed) & TIOCM_CTS) != 0;
}

std::optional<bool> RS232Host::getDCD(EmuTime /*time*/) const
{
	if (fd < 0) return false;
	return (cachedModemBits.load(std::memory_order_relaxed) & TIOCM_CAR) != 0;
}

void RS232Host::setDTR(bool status, EmuTime /*time*/)
{
	DTR = status;
	if (fd < 0) return;
	int bits = 0;
	ioctl(fd, TIOCMGET, &bits);
	if (DTR) bits |= TIOCM_DTR; else bits &= ~TIOCM_DTR;
	ioctl(fd, TIOCMSET, &bits);
	if (ioctl(fd, TIOCMGET, &bits) == 0) {
		cachedModemBits.store(bits, std::memory_order_relaxed);
	}
}

void RS232Host::setRTS(bool status, EmuTime /*time*/)
{
	if (RTS == status) return;
	RTS = status;
	if (fd >= 0) {
		int bits = 0;
		ioctl(fd, TIOCMGET, &bits);
		if (RTS) bits |= TIOCM_RTS; else bits &= ~TIOCM_RTS;
		ioctl(fd, TIOCMSET, &bits);
		if (ioctl(fd, TIOCMGET, &bits) == 0) {
			cachedModemBits.store(bits, std::memory_order_relaxed);
		}
	}
	if (RTS) {
		std::scoped_lock lock(mutex);
		if (!queue.empty()) {
			eventDistributor.distributeEvent(Rs232NetEvent());
		}
	}
}

template<typename Archive>
void RS232Host::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous connection
}
INSTANTIATE_SERIALIZE_METHODS(RS232Host);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, RS232Host, "RS232Host");

} // namespace openmsx
