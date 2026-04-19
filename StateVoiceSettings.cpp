#ifndef VC_SERVER_ONLY
#include "StateVoiceSettings.h"
#include "Config.h"
#include "Voice.h"

using namespace fdm;

StateVoiceSettings StateVoiceSettings::instanceObj{};

void StateVoiceSettings::updateProjection(const glm::ivec2& size)
{
	glm::mat4 projection2D = glm::ortho(0.f, (float)size.x, (float)size.y, 0.f, -1.0f, 1.0f);
	glViewport(0, 0, size.x, size.y);

	// update all 2D shaders
	const Shader* textShader = ShaderManager::get("textShader");
	textShader->use();
	glUniformMatrix4fv(glGetUniformLocation(textShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);

	const Shader* tex2DShader = ShaderManager::get("tex2DShader");
	tex2DShader->use();
	glUniformMatrix4fv(glGetUniformLocation(tex2DShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);

	const Shader* quadShader = ShaderManager::get("quadShader");
	quadShader->use();
	glUniformMatrix4fv(glGetUniformLocation(quadShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);
}

void StateVoiceSettings::viewportCallback(void* user, const glm::ivec4& pos, const glm::ivec2& scroll)
{
	GLFWwindow* window = (GLFWwindow*)user;

	// update the viewport
	int wWidth, wHeight;
	glfwGetWindowSize(window, &wWidth, &wHeight);
	glViewport(pos.x, wHeight - pos.y - pos.w, pos.z, pos.w);

	// create a 2D projection matrix from the specified dimensions and scroll position
	glm::mat4 projection2D = glm::ortho(0.0f, (float)pos.z, (float)pos.w, 0.0f, -1.0f, 1.0f);
	projection2D = glm::translate(projection2D, { scroll.x, scroll.y, 0 });

	// update all 2D shaders
	const Shader* textShader = ShaderManager::get("textShader");
	textShader->use();
	glUniformMatrix4fv(glGetUniformLocation(textShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);

	const Shader* tex2DShader = ShaderManager::get("tex2DShader");
	tex2DShader->use();
	glUniformMatrix4fv(glGetUniformLocation(tex2DShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);

	const Shader* quadShader = ShaderManager::get("quadShader");
	quadShader->use();
	glUniformMatrix4fv(glGetUniformLocation(quadShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);
}

void StateVoiceSettings::init(StateManager& s)
{
	int width, height;
	glfwGetWindowSize(s.window, &width, &height);

	qs = ShaderManager::get("quadShader");

	font = { ResourceManager::get("pixelfont.png"), ShaderManager::get("textShader") };

	qr.shader = qs;
	qr.init();

	ui.window = s.window;
	ui.viewportCallback = viewportCallback;
	ui.viewportUser = s.window;
	ui.font = &font;
	ui.qr = &qr;
	ui.cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
	glfwSetCursor(s.window, ui.cursor);

	box = gui::ContentBox{};
	{
		box.alignX(gui::ALIGN_CENTER_X);
		box.alignY(gui::ALIGN_CENTER_Y);
		box.width = 650;
		box.height = 400 + 55;
		box.parent = &ui;
		ui.addElement(&box);

		title = gui::Text{};
		title.setText("Voice-Chat");
		title.size = 2;
		title.alignX(gui::ALIGN_CENTER_X);
		title.alignY(gui::ALIGN_TOP);
		title.offsetY(10);
		box.addElement(&title);

		closeButton = gui::Button{};
		closeButton.width = 18;
		closeButton.height = 18;
		closeButton.setText("X");
		closeButton.alignX(gui::ALIGN_RIGHT);
		closeButton.alignY(gui::ALIGN_TOP);
		closeButton.offsetX(-8);
		closeButton.offsetY(8);
		closeButton.user = &s;
		closeButton.callback = [](void* user)
			{
				StateManager* s = (StateManager*)user;
				s->popState();
			};
		box.addElement(&closeButton);

		vcVolumeText = gui::Text{};
		vcVolumeText.setText(std::format("Voice Volume: {}%", (int)(config.vcVolume * 100)));
		vcVolumeText.size = 2;
		vcVolumeText.alignX(gui::ALIGN_CENTER_X);
		vcVolumeText.alignY(gui::ALIGN_TOP);
		vcVolumeText.offsetY(40);
		box.addElement(&vcVolumeText);

		vcVolume = gui::Slider{};
		vcVolume.alignX(gui::ALIGN_CENTER_X);
		vcVolume.alignY(gui::ALIGN_TOP);
		vcVolume.offsetY(40);
		vcVolume.range = 200;
		vcVolume.width = 400;
		vcVolume.value = config.vcVolume * 100;
		vcVolume.callback = [](void* user, int value)
			{
				config.vcVolume = (double)value / 100.0;
				StateVoiceSettings::instanceObj.vcVolumeText.setText(std::format("Voice Volume: {}%", (int)(config.vcVolume * 100)));
				updateVoiceVolume();
			};
		box.addElement(&vcVolume);

		micEnabledCheckbox = gui::CheckBox{};
		micEnabledCheckbox.alignX(gui::ALIGN_LEFT);
		micEnabledCheckbox.alignY(gui::ALIGN_TOP);
		micEnabledCheckbox.offsetX(100);
		micEnabledCheckbox.offsetY(105);
		micEnabledCheckbox.setText("Capture Input");
		micEnabledCheckbox.checked = config.micEnabled;
		micEnabledCheckbox.callback = [](void* user, bool checked)
			{
				config.micEnabled = checked;
				StateVoiceSettings::instanceObj.inputDeviceText.color.a = config.micEnabled ? 1.0f : 0.5f;
				StateVoiceSettings::instanceObj.inputDeviceSlider.clickable = config.micEnabled;
				if (!config.micEnabled)
				{
					closeAudioStream();
				}
				else if (audioStream.Information().state == Audijo::StreamState::Closed) // probably always
				{
					if (voiceChatConnected)
					{
						initVoiceChatInput();
					}
					else
					{
						StateVoiceSettings::instanceObj.startedStream = true;

						int device = pickInputDevice(config.inputDevice);
						if (device)
						{
							audioStream.Callback(inputCallbackSimple);

							audioStream.Open({
								.input = device,
								.output = Audijo::NoDevice,
								.bufferSize = FRAME_SIZE,
								.sampleRate = SAMPLE_RATE,
								.resampling = true
								});
							audioStream.Start();
						}
					}
				}
			};
		box.addElement(&micEnabledCheckbox);

		deviceNames.clear();
		deviceNames.emplace_back("Default");
		auto devices = audioStream.Devices();
		for (auto& dev : devices)
		{
			if (dev.inputChannels)
				deviceNames.emplace_back(dev.name);
		}

		int inputDevice = glm::clamp(config.inputDevice, -1, (int)deviceNames.size() - 2);

		inputDeviceText = gui::Text{};
		inputDeviceText.setText(std::format("Input: {}", deviceNames[inputDevice + 1]));
		inputDeviceText.size = 1;
		inputDeviceText.alignX(gui::ALIGN_CENTER_X);
		inputDeviceText.alignY(gui::ALIGN_TOP);
		inputDeviceText.offsetY(105 + 55);
		inputDeviceText.color.a = config.micEnabled ? 1.0f : 0.5f;
		box.addElement(&inputDeviceText);

		inputDeviceSlider = gui::Slider{};
		inputDeviceSlider.alignX(gui::ALIGN_CENTER_X);
		inputDeviceSlider.alignY(gui::ALIGN_TOP);
		inputDeviceSlider.offsetY(105 + 55);
		inputDeviceSlider.range = deviceNames.size() - 1;
		inputDeviceSlider.width = 400;
		inputDeviceSlider.value = inputDevice + 1;
		inputDeviceSlider.clickable = config.micEnabled;
		inputDeviceSlider.callback = [](void* user, int value)
			{
				config.inputDevice = value - 1;
				StateVoiceSettings::instanceObj.inputDeviceText.setText(std::format("Input: {}", StateVoiceSettings::instanceObj.deviceNames[value]));

				if (config.micEnabled)
				{
					if (StateVoiceSettings::instanceObj.startedStream)
					{
						closeAudioStream();

						int device = pickInputDevice(config.inputDevice);
						if (device)
						{
							audioStream.Callback(inputCallbackSimple);

							audioStream.Open({
								.input = device,
								.output = Audijo::NoDevice,
								.bufferSize = FRAME_SIZE,
								.sampleRate = SAMPLE_RATE,
								.resampling = true
								});
							audioStream.Start();
						}
					}
					else
					{
						updateConfigValues();
					}
				}
				else
				{
					closeAudioStream();
				}
			};
		box.addElement(&inputDeviceSlider);

		/*
		inputDeviceDropdown.setName("");
		inputDeviceDropdown.alignX(gui::ALIGN_CENTER_X);
		inputDeviceDropdown.offsetY(100);
		inputDeviceDropdown.callback = [](void* user, int64_t optionIndex, const fdm::stl::string& optionName)
			{
				config.inputDevice = optionIndex - 1;
			};
		inputDeviceDropdown.options.clear();
		inputDeviceDropdown.addOption("Default");

		auto devices = rtaudio.getDeviceIds();
		for (auto& dev : devices)
		{
			auto info = rtaudio.getDeviceInfo(dev);
			if (info.inputChannels)
				inputDeviceDropdown.addOption(info.name);
		}

		inputDeviceDropdown.selectOption(config.inputDevice + 1);
		inputDeviceDropdown.xTextPadding = 5;
		inputDeviceDropdown.fitText(400);
		box.addElement(&inputDeviceDropdown);
		*/

		micVolumeText = gui::Text{};
		micVolumeText.setText(std::format("Mic Volume: {}%", (int)(config.micVolume * 100)));
		micVolumeText.size = 2;
		micVolumeText.alignX(gui::ALIGN_CENTER_X);
		micVolumeText.alignY(gui::ALIGN_TOP);
		micVolumeText.offsetY(160 + 55);
		box.addElement(&micVolumeText);

		micVolume = gui::Slider{};
		micVolume.alignX(gui::ALIGN_CENTER_X);
		micVolume.alignY(gui::ALIGN_TOP);
		micVolume.offsetY(160 + 55);
		micVolume.range = 200;
		micVolume.width = 400;
		micVolume.value = config.micVolume * 100;
		micVolume.callback = [](void* user, int value)
			{
				config.micVolume = (double)value / 100.0;
				StateVoiceSettings::instanceObj.micVolumeText.setText(std::format("Mic Volume: {}%", (int)(config.micVolume * 100)));
				updateMicFilter();
			};
		box.addElement(&micVolume);

		thresholdText = gui::Text{};
		thresholdText.setText(std::format("Threshold: {:.1f}dB", config.micThreshold));
		thresholdText.size = 2;
		thresholdText.alignX(gui::ALIGN_CENTER_X);
		thresholdText.alignY(gui::ALIGN_TOP);
		thresholdText.offsetY(220 + 55);
		box.addElement(&thresholdText);

		volumeMeter = VolumeMeter{};
		volumeMeter.width = 400;
		volumeMeter.height = 10;
		volumeMeter.alignX(gui::ALIGN_CENTER_X);
		volumeMeter.alignY(gui::ALIGN_TOP);
		volumeMeter.offsetY(220 + 25 + 55);
		box.addElement(&volumeMeter);

		threshold = gui::Slider{};
		threshold.alignX(gui::ALIGN_CENTER_X);
		threshold.alignY(gui::ALIGN_TOP);
		threshold.offsetY(220 + 55);
		threshold.range = 1000;
		threshold.width = 400;
		threshold.value = (100 - (-config.micThreshold)) * 10;
		threshold.callback = [](void* user, int value)
			{
				config.micThreshold = -(100.0 - (double)value / 10.0);
				StateVoiceSettings::instanceObj.thresholdText.setText(std::format("Threshold: {:.1f}dB", config.micThreshold));
				updateMicFilter();
			};
		box.addElement(&threshold);

		pushToTalkCheckbox = gui::CheckBox{};
		pushToTalkCheckbox.alignX(gui::ALIGN_LEFT);
		pushToTalkCheckbox.alignY(gui::ALIGN_TOP);
		pushToTalkCheckbox.offsetX(100);
		pushToTalkCheckbox.offsetY(280 + 55);
		pushToTalkCheckbox.setText("Push To Talk");
		pushToTalkCheckbox.checked = config.pushToTalk;
		pushToTalkCheckbox.callback = [](void* user, bool checked)
			{
				config.pushToTalk = checked;
				StateVoiceSettings::instanceObj.pushToTalkToggleCheckbox.clickable = checked;
			};
		box.addElement(&pushToTalkCheckbox);

		pushToTalkToggleCheckbox = gui::CheckBox{};
		pushToTalkToggleCheckbox.alignX(gui::ALIGN_LEFT);
		pushToTalkToggleCheckbox.alignY(gui::ALIGN_TOP);
		pushToTalkToggleCheckbox.offsetX(100);
		pushToTalkToggleCheckbox.offsetY(280 + 30 + 55);
		pushToTalkToggleCheckbox.setText("Toggle Mode");
		pushToTalkToggleCheckbox.checked = config.pushToTalkToggle;
		pushToTalkToggleCheckbox.clickable = config.pushToTalk;
		pushToTalkToggleCheckbox.callback = [](void* user, bool checked)
			{
				config.pushToTalkToggle = checked;
			};
		box.addElement(&pushToTalkToggleCheckbox);

		pushToTalkBind = gui::Button{};
		pushToTalkBind.alignX(gui::ALIGN_RIGHT);
		pushToTalkBind.alignY(gui::ALIGN_TOP);
		pushToTalkBind.offsetX(-100);
		pushToTalkBind.offsetY(280 + 55);
		pushToTalkBind.setText(mbName(config.pushToTalkMouseBind));
		pushToTalkBind.width = 110;
		pushToTalkBind.height = 50;
		pushToTalkBind.callback = [](void* user)
			{
				if (!StateVoiceSettings::instanceObj.binding)
				{
					StateVoiceSettings::instanceObj.pushToTalkBind.setText("?");
					StateVoiceSettings::instanceObj.binding = true;
				}
				else
				{
					StateVoiceSettings::instanceObj.pushToTalkBind.setText(mbName(config.pushToTalkMouseBind));
					StateVoiceSettings::instanceObj.binding = false;
				}
			};
		box.addElement(&pushToTalkBind);

		playerListButton = gui::Button{};
		playerListButton.alignX(gui::ALIGN_CENTER_X);
		playerListButton.alignY(gui::ALIGN_BOTTOM);
		playerListButton.offsetY(-10);
		playerListButton.setText("Players");
		playerListButton.width = 200;
		playerListButton.callback = [](void* user)
			{
				StateVoiceSettings::instanceObj.ui.removeElement(&StateVoiceSettings::instanceObj.box);
				StateVoiceSettings::instanceObj.ui.addElement(&StateVoiceSettings::instanceObj.box2);
			};

		box.addElement(&playerListButton);

		muteButton = ImgButton{ResourceManager::get(config.muted ? "assets/mic_muted.png" : "assets/mic.png", true), 48, 48, 8, 8};
		muteButton.alignX(gui::ALIGN_RIGHT);
		muteButton.alignY(gui::ALIGN_TOP);
		muteButton.offsetX(-32);
		muteButton.offsetY(micVolume.yOffset + 1);
		muteButton.callback = [](void* user)
			{
				mute();
				StateVoiceSettings::instanceObj.muteButton
					.setTexture(ResourceManager::get(config.muted ? "assets/mic_muted.png" : "assets/mic.png", true));
			};
		box.addElement(&muteButton);

		deafenButton = ImgButton{ ResourceManager::get(config.deafened ? "assets/speaker_deafened.png" : "assets/speaker.png", true), 48, 48, 8, 8 };
		deafenButton.alignX(gui::ALIGN_RIGHT);
		deafenButton.alignY(gui::ALIGN_TOP);
		deafenButton.offsetX(-32);
		deafenButton.offsetY(vcVolume.yOffset + 1);
		deafenButton.callback = [](void* user)
			{
				deafen();
				StateVoiceSettings::instanceObj.deafenButton
					.setTexture(ResourceManager::get(config.deafened ? "assets/speaker_deafened.png" : "assets/speaker.png", true));
			};
		box.addElement(&deafenButton);
	}

	box2 = gui::ContentBox{};
	{
		box2.alignX(gui::ALIGN_CENTER_X);
		box2.alignY(gui::ALIGN_CENTER_Y);
		box2.width = 650;
		box2.height = 500;
		box2.parent = &ui;
		//ui.addElement(&box2);

		title2 = gui::Text{};
		title2.setText("Players:");
		title2.size = 2;
		title2.alignX(gui::ALIGN_CENTER_X);
		title2.alignY(gui::ALIGN_TOP);
		title2.offsetY(10);
		box2.addElement(&title2);

		closeButton2 = gui::Button{};
		closeButton2.width = 18;
		closeButton2.height = 18;
		closeButton2.setText("X");
		closeButton2.alignX(gui::ALIGN_RIGHT);
		closeButton2.alignY(gui::ALIGN_TOP);
		closeButton2.offsetX(-8);
		closeButton2.offsetY(8);
		closeButton2.user = &s;
		closeButton2.callback = [](void* user)
			{
				StateVoiceSettings::instanceObj.ui.removeElement(&StateVoiceSettings::instanceObj.box2);
				StateVoiceSettings::instanceObj.ui.addElement(&StateVoiceSettings::instanceObj.box);
			};
		box2.addElement(&closeButton2);

		box3 = gui::ContentBox{};
		box3.alignX(gui::ALIGN_CENTER_X);
		box3.alignY(gui::ALIGN_TOP);
		box3.offsetY(40);
		box3.width = 650;
		box3.height = 500-40;
		box3.parent = &box2;
		box2.addElement(&box3);

		players.clear();
		players.reserve(config.playerVolume.size());

		for (auto& player : config.playerVolume)
		{
			stl::uuid id = stl::uuid{}(player.first.c_str());

			if (voiceChatConnected && !vcToPlayer.contains(id))
				continue;

			Player& p = players.emplace_back();
			p.b.parent = &box3;
			p.b.alignX(gui::ALIGN_LEFT);
			p.b.alignY(gui::ALIGN_TOP);
			p.b.offsetX(10);
			p.b.offsetY(10 + (players.size() - 1) * 70);
			p.b.width = 650 - 10 - 10 - 15;
			p.b.height = 60;
			box3.addElement(&p.b);

			p.name.setText(config.playerNames[player.first]);
			p.name.fancy = true;
			p.name.alignX(gui::ALIGN_LEFT);
			p.name.alignY(gui::ALIGN_CENTER_Y);
			p.name.offsetX(16);
			p.name.size = 2;
			p.b.addElement(&p.name);

			p.volumeSlider.setText(std::format("Volume: {}%", (int)(player.second * 100)));
			p.volumeSlider.alignX(gui::ALIGN_RIGHT);
			p.volumeSlider.alignY(gui::ALIGN_CENTER_Y);
			p.volumeSlider.offsetX(-16);
			p.volumeSlider.offsetY(8);
			p.volumeSlider.range = 200;
			p.volumeSlider.width = 200;
			p.volumeSlider.value = player.second * 100;
			p.volumeSlider.user = new std::pair<stl::uuid, gui::Slider*>(id, &p.volumeSlider);
			p.volumeSlider.callback = [](void* user, int value)
				{
					auto* pair = (std::pair<stl::uuid, gui::Slider*>*)user;
					std::string str = stl::uuid::to_string(pair->first);
					config.playerVolume[str] = (double)value / 100.0;
					pair->second->setText(std::format("Volume: {}%", (int)(config.playerVolume[str] * 100)));
				};
			p.b.addElement(&p.volumeSlider);
		}

		box3.scrollH = glm::max((int)(players.size() * 70 + 10 - box3.height), 0);
	}

	updateProjection({ width, height });

	startedStream = false;
	if (config.micEnabled && audioStream.Information().state == Audijo::StreamState::Closed)
	{
		startedStream = true;

		int device = pickInputDevice(config.inputDevice);
		if (device)
		{
			audioStream.Callback(inputCallbackSimple);

			audioStream.Open({
				.input = device,
				.output = Audijo::NoDevice,
				.bufferSize = FRAME_SIZE,
				.sampleRate = SAMPLE_RATE,
				.resampling = true
				});
			audioStream.Start();
		}
	}
}

void StateVoiceSettings::update(StateManager& s, double dt)
{
	if (s.states[s.states.size() - 2] == &StateGame::instanceObj || s.states[s.states.size() - 3] == &StatePause::instanceObj)
		StateGame::instanceObj.update(s, dt);
	else if (s.states[s.states.size() - 2] == &StateSettings::instanceObj)
		StateTitleScreen::instanceObj.update(s, dt);

	volumeMeter.setValue(inputValue);
	volumeMeter.update(dt);
}

void StateVoiceSettings::render(StateManager& s)
{
	glEnable(GL_DEPTH_TEST);
	if (s.states[s.states.size() - 2] == &StateGame::instanceObj || s.states[s.states.size() - 3] == &StatePause::instanceObj)
		StateGame::instanceObj.render(s);
	else if (s.states[s.states.size() - 2] == &StateSettings::instanceObj)
		StateTitleScreen::instanceObj.render(s);

	int w, h;
	glfwGetWindowSize(s.window, &w, &h);

	glDisable(GL_DEPTH_TEST);

	qr.setColor(0, 0, 0, 0.6);
	qr.setPos(0, 0, w, h);
	qr.setQuadRendererMode(GL_TRIANGLES);
	qr.render();

	// render the ui
	ui.render();

	//Dropdown::handleListRendering(s);

	glEnable(GL_DEPTH_TEST);
}

void StateVoiceSettings::mouseInput(StateManager& s, double xpos, double ypos)
{
	ui.mouseInput(xpos, ypos);
}

void StateVoiceSettings::scrollInput(StateManager& s, double xoff, double yoff)
{
	ui.scrollInput(xoff, yoff);
}

void StateVoiceSettings::mouseButtonInput(StateManager& s, int button, int action, int mods)
{
	if (binding && button >= GLFW_MOUSE_BUTTON_4)
	{
		config.pushToTalkMouseBind = button - GLFW_MOUSE_BUTTON_4;
		binding = false;
		pushToTalkBind.setText(mbName(config.pushToTalkMouseBind));
	}
	else
	{
		ui.mouseButtonInput(button, action, mods);
	}
}

void StateVoiceSettings::keyInput(StateManager& s, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		if (binding)
		{
			config.pushToTalkMouseBind = -1;
			binding = false;
			pushToTalkBind.setText(mbName(config.pushToTalkMouseBind));
		}
		else
		{
			if (ui.elements.back() == &box2)
			{
				ui.removeElement(&box2);
				ui.addElement(&box);
			}
			else
			{
				s.popState();
			}
		}
	}
	else
	{
		ui.keyInput(key, scancode, action, mods);
	}
}

void StateVoiceSettings::windowResize(StateManager& s, int width, int height)
{
	updateProjection({ width, height });
}

void StateVoiceSettings::close(StateManager& s)
{
	for (auto& player : players)
	{
		delete (std::pair<stl::uuid, gui::Slider*>*)player.volumeSlider.user;
	}
	players.clear();

	box.clear();
	box2.clear();
	box3.clear();
	ui.clear();

	if (startedStream)
	{
		if (audioStream.Information().state != Audijo::StreamState::Closed)
		{
			audioStream.Close();
			while (audioStream.Information().state != Audijo::StreamState::Closed);
		}
		startedStream = false;
	}

	binding = false;

	saveConfig();
	updateConfigValues();
}
#endif
