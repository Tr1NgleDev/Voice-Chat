#pragma once

#include <4dm.h>

#include "Buffer.h"

#ifndef VC_SERVER_ONLY
#include "Config.h"
#include "MicFilter.h"
#endif

inline constexpr uint32_t SAMPLE_RATE = 48000;
inline constexpr uint32_t FRAME_MS = 20;
inline constexpr uint32_t FRAME_SIZE = SAMPLE_RATE * FRAME_MS / 1000;
inline constexpr uint32_t MAX_ENCODED_DATA_SIZE = 1024;

#ifndef VC_SERVER_ONLY
class Voice : public fdm::SoLoud::AudioSource
{
public:
	std::shared_ptr<Buffer<float>> buff;
	double output = 0.0;

	Voice() {}
	Voice(std::shared_ptr<Buffer<float>> buff);

	fdm::SoLoud::AudioSourceInstance* createInstance() override;
};

class VoiceInstance : public fdm::SoLoud::AudioSourceInstance
{
private:
	Voice* mParent;

public:
	VoiceInstance(Voice& aParent);
	
	uint32_t getAudio(float* aBuffer, uint32_t aSamplesToRead, uint32_t aBufferSize) override;
	bool hasEnded() override;
};

extern std::atomic<bool> voiceChatConnected;
extern double inputValue;
extern MicFilter micFilter;
extern Buffer<int16_t>* buff;
extern std::thread encoderThread;
extern int16_t* postProcessingBuffer;
extern double inputEnv;
void inputCallbackSimple(Audijo::Buffer<int16_t>& input, Audijo::Buffer<int16_t>& output, Audijo::CallbackInfo info);
void initVoiceChatInput();
void encoderThreadFunc(Buffer<int16_t>* buff);
void inputCallback(Audijo::Buffer<int16_t>& input, Audijo::Buffer<int16_t>& output, Audijo::CallbackInfo info);
#endif

extern std::unordered_map<fdm::stl::uuid, fdm::stl::uuid> playerToVC;
extern std::unordered_map<fdm::stl::uuid, fdm::stl::uuid> vcToPlayer;
