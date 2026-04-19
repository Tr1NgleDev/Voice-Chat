#pragma once
#include <4dm.h>

#include "VolumeMeter.h"
#include "ImgButton.h"

//#include "Dropdown.h"


class StateVoiceSettings : public fdm::State
{
public:
	static StateVoiceSettings instanceObj;

	bool startedStream = false;

	const fdm::Shader* qs;
	fdm::QuadRenderer qr;
	fdm::FontRenderer font;
	fdm::gui::Interface ui;

	fdm::gui::ContentBox box;
	fdm::gui::Text title;
	fdm::gui::Button closeButton;
	fdm::gui::Text vcVolumeText; // i want the text centered so...
	fdm::gui::Slider vcVolume;
	fdm::gui::CheckBox micEnabledCheckbox;
	std::vector<std::string> deviceNames{};
	fdm::gui::Text inputDeviceText; // i wanted to use my dropdown class but the names are too long :sob:
	fdm::gui::Slider inputDeviceSlider;
	//Dropdown inputDeviceDropdown;
	fdm::gui::Text micVolumeText; // i want the text centered so...
	fdm::gui::Slider micVolume;
	VolumeMeter volumeMeter;
	fdm::gui::Text thresholdText; // i want the text centered so...
	fdm::gui::Slider threshold;
	ImgButton muteButton;
	ImgButton deafenButton;
	fdm::gui::CheckBox pushToTalkCheckbox;
	fdm::gui::CheckBox pushToTalkToggleCheckbox;
	fdm::gui::Button pushToTalkBind;
	fdm::gui::Button playerListButton;

	fdm::gui::ContentBox box2;
	fdm::gui::Text title2;
	fdm::gui::Button closeButton2;
	fdm::gui::ContentBox box3;
	struct Player
	{
		fdm::gui::ContentBox b{};
		fdm::gui::Text name{};
		fdm::gui::Slider volumeSlider{};
	};
	std::vector<Player> players;

	bool binding = false;

	void updateProjection(const glm::ivec2& size);
	static void viewportCallback(void* user, const glm::ivec4& pos, const glm::ivec2& scroll);

	void init(fdm::StateManager& s);
	void close(fdm::StateManager& s);
	void update(fdm::StateManager& s, double dt);
	void render(fdm::StateManager& s);
	void mouseInput(fdm::StateManager& s, double xpos, double ypos);
	void scrollInput(fdm::StateManager& s, double xoff, double yoff);
	void mouseButtonInput(fdm::StateManager& s, int button, int action, int mods);
	void keyInput(fdm::StateManager& s, int key, int scancode, int action, int mods);
	void windowResize(fdm::StateManager& s, int width, int height);
};
