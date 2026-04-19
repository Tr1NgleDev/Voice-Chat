#ifndef VC_SERVER_ONLY
#include "Config.h"

#include <fstream>

Config config{};
Stream<Audijo::Wasapi> audioStream{};
boost::uuids::random_generator uuidGen{};

using namespace fdm;

void loadConfig()
{
	std::ifstream configFile{ std::filesystem::path(fdm::getModPath(fdm::modID)) / "config.json" };
	if (!configFile.is_open()) return;

	nlohmann::json j = nlohmann::json::parse(configFile);

	if (j.contains("micEnabled") && j["micEnabled"].is_boolean())
		config.micEnabled = j["micEnabled"];
	else
		config.micEnabled = true;

	if (j.contains("vcVolume") && j["vcVolume"].is_number_float())
		config.vcVolume = j["vcVolume"];
	else
		config.vcVolume = 1.0;

	if (j.contains("micVolume") && j["micVolume"].is_number_float())
		config.micVolume = j["micVolume"];
	else
		config.micVolume = 1.0;

	if (j.contains("micThreshold") && j["micThreshold"].is_number_float())
		config.micThreshold = j["micThreshold"];
	else
		config.micThreshold = -50;

	if (j.contains("muted") && j["muted"].is_boolean())
		config.muted = j["muted"];
	else
		config.muted = false;

	if (j.contains("deafened") && j["deafened"].is_boolean())
		config.deafened = j["deafened"];
	else
		config.deafened = false;

	if (j.contains("pushToTalk") && j["pushToTalk"].is_boolean())
		config.pushToTalk = j["pushToTalk"];
	else
		config.pushToTalk = true;

	if (j.contains("inputDevice") && j["inputDevice"].is_number_integer())
		config.inputDevice = j["inputDevice"];
	else
		config.inputDevice = -1;

	if (j.contains("playerVolume") && j["playerVolume"].is_object())
		config.playerVolume = j["playerVolume"];
	else
		config.playerVolume = {};

	if (j.contains("playerNames") && j["playerNames"].is_object())
		config.playerNames = j["playerNames"];
	else
		config.playerNames = {};

	if (j.contains("pushToTalkToggle") && j["pushToTalkToggle"].is_boolean())
		config.pushToTalkToggle = j["pushToTalkToggle"];
	else
		config.pushToTalkToggle = false;

	if (j.contains("pushToTalkMouseBind") && j["pushToTalkMouseBind"].is_number_integer())
		config.pushToTalkMouseBind = j["pushToTalkMouseBind"];
	else
		config.pushToTalkMouseBind = GLFW_MOUSE_BUTTON_5 - GLFW_MOUSE_BUTTON_4;

	if (j.contains("vcUUID") && j["vcUUID"].is_string())
		config.vcUUID = boost::lexical_cast<uuid>(j["vcUUID"].get<std::string>());
	else
		config.vcUUID = uuidGen();

	configFile.close();

	saveConfig();
	updateConfigValues();
}

void saveConfig()
{
	std::ofstream configFile{ std::filesystem::path(fdm::getModPath(fdm::modID)) / "config.json" };
	if (!configFile.is_open()) return;

	if (config.vcUUID.is_nil())
		config.vcUUID = uuidGen();

	nlohmann::json j
	{
		{ "vcUUID", boost::lexical_cast<std::string>(config.vcUUID) },
		{ "vcVolume", config.vcVolume },
		{ "micVolume", config.micVolume },
		{ "micThreshold", config.micThreshold },
		{ "muted", config.muted },
		{ "deafened", config.deafened },
		{ "pushToTalk", config.pushToTalk },
		{ "pushToTalkToggle", config.pushToTalkToggle },
		{ "pushToTalkMouseBind", config.pushToTalkMouseBind },
		{ "inputDevice", config.inputDevice },
		{ "playerVolume", config.playerVolume },
		{ "playerNames", config.playerNames },
		{ "micEnabled", config.micEnabled },
	};
	configFile << j.dump(4);

	configFile.close();
}

std::string mbName(int button)
{
	if (button >= 0)
	{
		return std::format("Mouse{}", button + 4);
	}
	return "None";
}

void updateMicFilter()
{
	config.micVolume = std::clamp(config.micVolume, 0.0, 2.0);
	config.micThreshold = std::clamp(config.micThreshold, -100.0, 0.0);

	micFilter.setGain(config.micVolume * (config.muted ? 0 : 1));
	micFilter.setThreshold(config.micThreshold);
}

void updateVoiceVolume()
{
	config.vcVolume = std::clamp(config.vcVolume, 0.0, 2.0);

	auto* voiceGroup = AudioManager::getVoiceGroupInfo("voice");
	voiceGroup->volume = config.vcVolume * (config.deafened ? 0 : 1);
}
#endif
