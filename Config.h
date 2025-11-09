#pragma once

#include <4dm.h>
#include <Audijo/Audijo.hpp>
#include <boost/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/lexical_cast.hpp>
#include "Voice.h"

using uuid = boost::uuids::uuid;

using Audijo::Stream;

extern Stream<Audijo::Wasapi> audioStream;
extern boost::uuids::random_generator uuidGen;

struct Config
{
	uuid vcUUID{};
	double vcVolume = 1.0;
	double micVolume = 1.0;
	double micThreshold = -50;
	bool muted = false;
	bool deafened = false;
	bool pushToTalk = true;
	bool pushToTalkToggle = false;
	int pushToTalkMouseBind = GLFW_MOUSE_BUTTON_5 - GLFW_MOUSE_BUTTON_4;
	int inputDevice = -1;
	std::map<std::string, double> playerVolume{};
	std::map<std::string, std::string> playerNames{};
};

extern Config config;

void updateMicFilter();
void updateVoiceVolume();
void updateConfigValues(bool updateInputDevice = true);
void loadConfig();
void saveConfig();
int pickInputDevice(int target);

std::string mbName(int button);

void deafen();
void mute();
