#pragma once

#include <cstdlib>
#include <cstdint>
#include <memory>

class BytePacket
{
private:
	bool allocated = false;

public:
	size_t cursor = 0;
	size_t size = 0;
	uint8_t* data = nullptr;

	BytePacket() {}
	BytePacket(const void* data, size_t size) : data((uint8_t*)data), size(size) {}

	template<typename T>
	T next()
	{
		if (cursor >= size)
			return T{};

		T v = *(T*)&data[cursor];
		cursor += sizeof(T);

		return v;
	}

	void resize(size_t newSize)
	{
		if (newSize <= size) return;

		if (data == nullptr)
			data = (uint8_t*)malloc(newSize);
		else
			data = (uint8_t*)realloc(data, newSize);

		size = newSize;

		allocated = true;
	}

	void push(const void* data, size_t size)
	{
		int newSize = this->size + size;
		resize(newSize);

		std::memcpy(this->data + cursor, data, size);

		cursor += size;
	}

	template<typename T>
	void push(const T& data)
	{
		push((const void*)&data, sizeof(T));
	}

	~BytePacket()
	{
		if (allocated)
		{
			std::free(data);
		}
		data = nullptr;
		size = 0;
		cursor = 0;
	}
};
