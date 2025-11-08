#pragma once

#include <atomic>
#include <vector>
#include <cstdint>

// guys is this a homestuck reference
// guys it is just like the FIFO modus from homestuck

template<typename T>
class Buffer
{
private:
	std::vector<T> buffer{};

	size_t capacity = 0;
	size_t mask = 0;
	std::atomic<size_t> head;
	std::atomic<size_t> tail;

public:
	Buffer(size_t capacity)
	{
		// round to a power of two
		--capacity;
		capacity |= capacity >> 1;
		capacity |= capacity >> 2;
		capacity |= capacity >> 4;
		for (std::size_t i = 1; i < sizeof(std::size_t); i <<= 1)
			capacity |= capacity >> (i << 3);
		mask = capacity++;

		this->capacity = capacity;
		buffer.resize(capacity);
		head.store(0);
		tail.store(0);
	}

	// push to the back
	bool push(const T* data, size_t samples)
	{
		size_t h = head.load(std::memory_order_relaxed);
		size_t t = tail.load(std::memory_order_acquire);

		size_t freeSamples = (capacity - h + t - 1) & mask;

		if (samples > freeSamples)
			return false;

		for (size_t i = 0; i < samples; ++i)
		{
			buffer[h] = data[i];
			h = (h + 1) & mask;
		}

		head.store(h, std::memory_order_release);

		return true;
	}

	// pop from the front
	size_t pop(T* data, size_t samples)
	{
		size_t h = head.load(std::memory_order_acquire);
		size_t t = tail.load(std::memory_order_relaxed);

		size_t available = (capacity - t + h) & mask;
		size_t count = std::min(available, samples);

		for (size_t i = 0; i < count; ++i)
		{
			data[i] = buffer[t];
			t = (t + 1) & mask;
		}

		tail.store(t, std::memory_order_release);

		return count;
	}

	size_t available()
	{
		size_t h = head.load(std::memory_order_acquire);
		size_t t = tail.load(std::memory_order_acquire);

		return (capacity - t + h) & mask;
	}

	void clear()
	{
		buffer.clear();
		buffer.resize(capacity);
		size_t h = head.load(std::memory_order_relaxed);
		size_t t = tail.load(std::memory_order_relaxed);
		head.store(0, std::memory_order_release);
		tail.store(0, std::memory_order_release);
	}

	size_t getCapacity() const
	{
		return capacity;
	}

	size_t getHead() const
	{
		return head;
	}

	size_t getTail() const
	{
		return tail;
	}

	Buffer(Buffer&& other) noexcept
	{
		this->buffer = other.buffer;
		this->capacity = other.capacity;

		this->head.load(std::memory_order_relaxed);
		size_t h = other.head.load(std::memory_order_relaxed);
		this->head.store(h, std::memory_order_release);

		this->tail.load(std::memory_order_relaxed);
		size_t t = other.tail.load(std::memory_order_relaxed);
		this->tail.store(h, std::memory_order_release);

		other.buffer.clear();
		other.buffer.shrink_to_fit();
		other.capacity = 0;
		other.head.store(0, std::memory_order_release);
		other.tail.store(0, std::memory_order_release);
	}

	Buffer(const Buffer& other) = delete;

	~Buffer()
	{
		buffer.clear();
		capacity = 0;
		head.store(0, std::memory_order_release);
		tail.store(0, std::memory_order_release);
	}
};
