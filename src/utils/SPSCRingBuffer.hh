#ifndef SPSC_RING_BUFFER_HH
#define SPSC_RING_BUFFER_HH

#include <atomic>
#include <cstddef>
#include <memory>

namespace openmsx {

/**
 * Lock-free Single-Producer Single-Consumer Ring Buffer.
 *
 * This is a thread-safe queue optimized for the case where exactly one
 * thread produces items and exactly one thread consumes them. No locks
 * are used, making it suitable for high-frequency data transfer like
 * CPU trace streaming.
 *
 * Features:
 * - Lock-free push and pop operations
 * - Cache line aligned head/tail to prevent false sharing
 * - Fixed capacity (no dynamic resizing)
 * - Non-blocking: tryPush/tryPop return false instead of blocking
 *
 * @tparam T Element type (must be movable)
 * @tparam Capacity Maximum number of elements in the buffer
 */
template<typename T, size_t Capacity>
class SPSCRingBuffer {
	static_assert(Capacity > 0, "Capacity must be positive");

public:
	SPSCRingBuffer()
		: buffer(std::make_unique<T[]>(Capacity))
	{
	}

	// Non-copyable, non-movable
	SPSCRingBuffer(const SPSCRingBuffer&) = delete;
	SPSCRingBuffer(SPSCRingBuffer&&) = delete;
	SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
	SPSCRingBuffer& operator=(SPSCRingBuffer&&) = delete;

	/**
	 * Try to push an item to the queue (producer side).
	 * @param item The item to push
	 * @return true if successful, false if queue is full
	 */
	[[nodiscard]] bool tryPush(const T& item) {
		size_t currentTail = tail.load(std::memory_order_relaxed);
		size_t nextTail = increment(currentTail);

		if (nextTail == head.load(std::memory_order_acquire)) {
			return false;  // Buffer full
		}

		buffer[currentTail] = item;
		tail.store(nextTail, std::memory_order_release);
		return true;
	}

	/**
	 * Try to push an item to the queue using move semantics.
	 * @param item The item to move
	 * @return true if successful, false if queue is full
	 */
	[[nodiscard]] bool tryPush(T&& item) {
		size_t currentTail = tail.load(std::memory_order_relaxed);
		size_t nextTail = increment(currentTail);

		if (nextTail == head.load(std::memory_order_acquire)) {
			return false;  // Buffer full
		}

		buffer[currentTail] = std::move(item);
		tail.store(nextTail, std::memory_order_release);
		return true;
	}

	/**
	 * Try to pop an item from the queue (consumer side).
	 * @param item Output parameter for the popped item
	 * @return true if successful, false if queue is empty
	 */
	[[nodiscard]] bool tryPop(T& item) {
		size_t currentHead = head.load(std::memory_order_relaxed);

		if (currentHead == tail.load(std::memory_order_acquire)) {
			return false;  // Buffer empty
		}

		item = std::move(buffer[currentHead]);
		head.store(increment(currentHead), std::memory_order_release);
		return true;
	}

	/**
	 * Check if the queue is empty.
	 * Note: This is a snapshot and may change immediately.
	 */
	[[nodiscard]] bool isEmpty() const {
		return head.load(std::memory_order_acquire) ==
		       tail.load(std::memory_order_acquire);
	}

	/**
	 * Get approximate number of items in queue.
	 * Note: This is a snapshot and may change immediately.
	 */
	[[nodiscard]] size_t size() const {
		size_t h = head.load(std::memory_order_acquire);
		size_t t = tail.load(std::memory_order_acquire);
		return (t >= h) ? (t - h) : (Capacity - h + t);
	}

	/**
	 * Get maximum capacity of the queue.
	 * Actual usable capacity is Capacity - 1 (one slot reserved for empty/full detection).
	 */
	[[nodiscard]] static constexpr size_t capacity() { return Capacity - 1; }

private:
	[[nodiscard]] static size_t increment(size_t idx) {
		return (idx + 1) % Capacity;
	}

	std::unique_ptr<T[]> buffer;

	// Cache line aligned to prevent false sharing between producer and consumer
	alignas(64) std::atomic<size_t> head{0};
	alignas(64) std::atomic<size_t> tail{0};
};

} // namespace openmsx

#endif // SPSC_RING_BUFFER_HH
