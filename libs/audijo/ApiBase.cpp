#ifndef VC_SERVER_ONLY
#include "include/Audijo/ApiBase.hpp"

namespace Audijo
{
	template<typename T>
	concept isInt = std::integral<T> || std::same_as<T, int24_t>;

	template<typename In, typename Out>
	void ConvertBufferTyped(char* outBuffer, char* inBuffer, size_t bufferSize)
	{
		Out* out = (Out*)outBuffer;
		In* in = (In*)inBuffer;
		for (size_t i = 0; i < bufferSize; ++i)
		{
			if constexpr (isInt<In> && isInt<Out>)
			{
				constexpr int _inBits = sizeof(In) * 8;
				constexpr int _outBits = sizeof(Out) * 8;

				int64_t _v = (int64_t)in[i];
				if constexpr (_outBits > _inBits)
					_v <<= (_outBits - _inBits);
				else if constexpr (_outBits < _inBits)
					_v >>= (_inBits - _outBits);

				out[i] = (Out)(int32_t)_v;
			}
			else if constexpr (std::floating_point<In> && std::floating_point<Out>)
				out[i] = (Out)in[i];

			else if constexpr (std::floating_point<Out>)
				out[i] = (Out)in[i] / (Out)std::numeric_limits<In>::max();

			else
			{
				double _v = std::clamp((double)in[i], -1.0, 1.0);
				out[i] = (Out)(_v * (double)std::numeric_limits<Out>::max());
			}
		}
	}

	void ApiBase::AllocateBuffers()
	{
		int _nInChannels = m_Information.inputChannels;
		int _nOutChannels = m_Information.outputChannels;
		int _bufferSize = m_Information.bufferSize;
		auto _inFormat = m_Information.inFormat;
		auto _outFormat = m_Information.outFormat;

		m_InputBuffers = new char* [_nInChannels];
		m_OutputBuffers = new char* [_nOutChannels];

		for (int i = 0; i < _nInChannels; i++)
			m_InputBuffers[i] = new char[_bufferSize * (_inFormat & Bytes)];

		for (int i = 0; i < _nOutChannels; i++)
			m_OutputBuffers[i] = new char[_bufferSize * (_outFormat & Bytes)];
	}

	void ApiBase::FreeBuffers()
	{
		if (m_InputBuffers != nullptr)
		{
			int _nInChannels = m_Information.inputChannels;
			for (int i = 0; i < _nInChannels; i++)
				delete[] m_InputBuffers[i];
			delete[] m_InputBuffers;
			m_InputBuffers = nullptr;
		}

		if (m_OutputBuffers != nullptr)
		{
			int _nOutChannels = m_Information.outputChannels;
			for (int i = 0; i < _nOutChannels; i++)
				delete[] m_OutputBuffers[i];
			delete[] m_OutputBuffers;
			m_OutputBuffers = nullptr;
		}
	}

	void ApiBase::ConvertBuffer(char* outBuffer, char* inBuffer, size_t bufferSize, SampleFormat outFormat, SampleFormat inFormat)
	{
		switch (inFormat)
		{
		case Int8:
			switch (outFormat)
			{
			case Int16:   ConvertBufferTyped<int8_t, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Int24:   ConvertBufferTyped<int8_t, int24_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<int8_t, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<int8_t, float>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<int8_t, double>(outBuffer, inBuffer, bufferSize); break;
			default: std::copy(inBuffer, inBuffer + bufferSize, outBuffer);
			} break;
		case Int16:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<int16_t, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int24:   ConvertBufferTyped<int16_t, int24_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<int16_t, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<int16_t, float>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<int16_t, double>(outBuffer, inBuffer, bufferSize); break;
			default: std::copy(inBuffer, inBuffer + 2 * bufferSize, outBuffer);
			} break;
		case Int24:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<int24_t, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int16:   ConvertBufferTyped<int24_t, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<int24_t, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<int24_t, float>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<int24_t, double>(outBuffer, inBuffer, bufferSize); break;
			default: std::copy(inBuffer, inBuffer + 3 * bufferSize, outBuffer);
			} break;
		case Int32:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<int32_t, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int16:   ConvertBufferTyped<int32_t, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Int24:   ConvertBufferTyped<int32_t, int24_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<int32_t, float>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<int32_t, double>(outBuffer, inBuffer, bufferSize); break;
			default: std::copy(inBuffer, inBuffer + 4 * bufferSize, outBuffer);
			} break;
		case Float32:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<float, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int16:   ConvertBufferTyped<float, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Int24:   ConvertBufferTyped<float, int24_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<float, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<float, double>(outBuffer, inBuffer, bufferSize); break;
			default: std::copy(inBuffer, inBuffer + 4 * bufferSize, outBuffer);
			} break;
		case Float64:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<double, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int16:   ConvertBufferTyped<double, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Int24:   ConvertBufferTyped<double, int24_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<double, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<double, float>(outBuffer, inBuffer, bufferSize); break;
			default: std::copy(inBuffer, inBuffer + 8 * bufferSize, outBuffer);
			} break;
		}
	}

	void ApiBase::ByteSwapBuffer(char* buffer, unsigned int bufferSize, SampleFormat format)
	{
		char val;
		char* ptr;

		ptr = buffer;
		if (format == Int16)
		{
			for (unsigned int i = 0; i < bufferSize; i++)
			{
				// Swap 1st and 2nd bytes.
				val = *(ptr);
				*(ptr) = *(ptr + 1);
				*(ptr + 1) = val;

				// Increment 2 bytes.
				ptr += 2;
			}
		}
		else if (format == Int32 || format == Float32)
		{
			for (unsigned int i = 0; i < bufferSize; i++)
			{
				// Swap 1st and 4th bytes.
				val = *(ptr);
				*(ptr) = *(ptr + 3);
				*(ptr + 3) = val;

				// Swap 2nd and 3rd bytes.
				ptr += 1;
				val = *(ptr);
				*(ptr) = *(ptr + 1);
				*(ptr + 1) = val;

				// Increment 3 more bytes.
				ptr += 3;
			}
		}
		else if (format == Float64)
		{
			for (unsigned int i = 0; i < bufferSize; i++)
			{
				// Swap 1st and 8th bytes
				val = *(ptr);
				*(ptr) = *(ptr + 7);
				*(ptr + 7) = val;

				// Swap 2nd and 7th bytes
				ptr += 1;
				val = *(ptr);
				*(ptr) = *(ptr + 5);
				*(ptr + 5) = val;

				// Swap 3rd and 6th bytes
				ptr += 1;
				val = *(ptr);
				*(ptr) = *(ptr + 3);
				*(ptr + 3) = val;

				// Swap 4th and 5th bytes
				ptr += 1;
				val = *(ptr);
				*(ptr) = *(ptr + 1);
				*(ptr + 1) = val;

				// Increment 5 more bytes.
				ptr += 5;
			}
		}
	}
}
#endif
