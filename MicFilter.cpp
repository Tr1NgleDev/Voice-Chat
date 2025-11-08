#ifndef VC_SERVER_ONLY

#include "MicFilter.h"

MicFilter::MicFilter(double threshold, double attack, double release, double gain, uint32_t sampleRate)
{
	setSampleRate(sampleRate);
	setThreshold(threshold);
	setAttack(attack);
	setRelease(release);
}

void MicFilter::setGain(double gain)
{
	this->gain = gain;
}
double MicFilter::getGain()
{
	return this->gain;
}

void MicFilter::setThreshold(double threshold)
{
	this->threshold = threshold;
}
double MicFilter::getThreshold()
{
	return this->threshold;
}

void MicFilter::setRelease(double release)
{
	this->release = release;
}
double MicFilter::getRelease()
{
	return this->release;
}

void MicFilter::setAttack(double attack)
{
	this->attack = attack;
}
double MicFilter::getAttack()
{
	return this->attack;
}

void MicFilter::setSampleRate(uint32_t sampleRate)
{
	this->sampleRate = sampleRate;
}
uint32_t MicFilter::getSampleRate()
{
	return sampleRate;
}

double MicFilter::process(int16_t* buff, size_t samples)
{
	double sum = 0.0;
	for (size_t i = 0; i < samples; i++)
		sum += buff[i] * buff[i];
	double v = std::sqrt(sum / samples);

	double dB = 20.0 * std::log10(v / (double)INT16_MAX + 1e-10);

	double releaseC = 1.0 - std::exp(-(double)samples / (sampleRate * release));
	double attackC = 1.0 - std::exp(-(double)samples / (sampleRate * attack));

	if (dB < threshold)
	{
		env -= releaseC;
	}
	else
	{
		env += attackC;
	}

	env = std::clamp(env, 0.0, 1.0);
	if (env <= 0.01)
	{
		env = 0.0;
	}
	else if (env >= 0.99)
	{
		env = 1.0;
	}

	double totalGain = std::clamp(gain * env, 0.0, 2.0);

	for (size_t i = 0; i < samples; ++i)
	{
		buff[i] = static_cast<int16_t>(
			std::clamp(
				buff[i] * totalGain,
				(double)INT16_MIN, (double)INT16_MAX
			)
		);
	}

	return env;
}
#endif
