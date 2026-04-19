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
int16_t* postProcessingBuffer = nullptr;
Buffer<int16_t>* buff = nullptr;
std::thread encoderThread;
double inputEnv = 0.0;
void inputCallbackSimple(Audijo::Buffer<int16_t>& input, Audijo::Buffer<int16_t>& output, Audijo::CallbackInfo info)
{
	double sum = 0.0;
	for (size_t i = 0; i < input.Frames(); ++i)
		sum += input.data()[0][i] * input.data()[0][i];
	double v = std::sqrt(sum / input.Frames());

	double dB = 20.0 * std::log10(v / (double)INT16_MAX + 1e-10);

	inputValue = dB;
}
void initVoiceChatInput()
{
	if (config.micEnabled)
	{
		int device = pickInputDevice(config.inputDevice);
		if (device != -1)
		{
			closeAudioStream();

			buff = new Buffer<int16_t>(SAMPLE_RATE / 5); // 200ms of samples

			uint32_t bufferFrames = FRAME_SIZE;

			if (postProcessingBuffer)
			{
				free(postProcessingBuffer);
			}

			audioStream.Callback(inputCallback);

			audioStream.Open({
				.input = device,
				.output = Audijo::NoDevice,
				.bufferSize = FRAME_SIZE,
				.sampleRate = SAMPLE_RATE,
				.resampling = true
				});

			bufferFrames = audioStream.Information().bufferSize;

			postProcessingBuffer = (int16_t*)malloc(bufferFrames * sizeof(int16_t));

			audioStream.Start();

			encoderThread = std::thread{ encoderThreadFunc, buff };
			encoderThread.detach();
		}
	}
	else
	{
		closeAudioStream();
	}
}
#endif

std::unordered_map<fdm::stl::uuid, fdm::stl::uuid> playerToVC{};
std::unordered_map<fdm::stl::uuid, fdm::stl::uuid> vcToPlayer{};
