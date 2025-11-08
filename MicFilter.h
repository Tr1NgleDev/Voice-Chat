#pragma once

#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <algorithm>

class MicFilter
{
private:
	uint32_t sampleRate = 48000;
	double env = 0;

	double threshold = -65; // dB
	double attack = 0.05; // s
	double release = 0.1; // s

	double gain = 1.0;

public:

	MicFilter(double threshold = -65, double attack = 0.05, double release = 0.1, double gain = 1.0, uint32_t sampleRate = 48000);
	void setGain(double gain);
	double getGain();
	void setThreshold(double threshold);
	double getThreshold();
	void setRelease(double release);
	double getRelease();
	void setAttack(double attack);
	double getAttack();
	void setSampleRate(uint32_t sampleRate);
	uint32_t getSampleRate();

	double process(int16_t* buff, size_t samples);
};
