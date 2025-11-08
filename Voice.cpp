#include "Voice.h"

#ifndef VC_SERVER_ONLY

Voice::Voice(std::shared_ptr<Buffer<float>> buff)
	: buff(buff)
{
	mBaseSamplerate = SAMPLE_RATE;
	mChannels = 1;
}

fdm::SoLoud::AudioSourceInstance* Voice::createInstance()
{
	return new VoiceInstance(*this);
}

VoiceInstance::VoiceInstance(Voice& aParent)
	: mParent(&aParent)
{
	mBaseSamplerate = SAMPLE_RATE;
	mChannels = 1;
}

uint32_t VoiceInstance::getAudio(float* aBuffer, uint32_t aSamplesToRead, uint32_t aBufferSize)
{
	size_t samples = mParent->buff->pop(aBuffer, aSamplesToRead);
	if (samples < aSamplesToRead)
	{
		std::memset(aBuffer + samples, 0, (aSamplesToRead - samples) * sizeof(float));
	}

	double sum = 0.0;
	for (size_t i = 0; i < samples; i++)
		sum += aBuffer[i] * aBuffer[i];
	double v = std::sqrt(sum / samples);
	double dB = 20.0 * std::log10(v + 1e-10);
	mParent->output = glm::clamp((100 - (-dB)) / 100.0 + 0.5, 0.0, 1.0);

	return aSamplesToRead;
}

bool VoiceInstance::hasEnded()
{
	return false;
}

std::atomic<bool> voiceChatConnected = false;
double inputValue = -100.0;
MicFilter micFilter{ -50, 0.05, 0.1, 1.0, SAMPLE_RATE };
int rtCallbackSimple(void* outBuff, void* inBuff, uint32_t buffFrames, double streamTime, RtAudioStreamStatus status, void* user)
{
	int16_t* in = (int16_t*)inBuff;

	double sum = 0.0;
	for (size_t i = 0; i < buffFrames; i++)
		sum += in[i] * in[i];
	double v = std::sqrt(sum / buffFrames);

	double dB = 20.0 * std::log10(v / (double)INT16_MAX + 1e-10);

	inputValue = dB;

	return 0;
}
#endif

std::unordered_map<fdm::stl::uuid, fdm::stl::uuid> playerToVC{};
std::unordered_map<fdm::stl::uuid, fdm::stl::uuid> vcToPlayer{};
