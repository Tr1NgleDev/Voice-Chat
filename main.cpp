#include <4dm.h>

using namespace fdm;

// Initialize the DLLMain
initDLL

#ifndef VC_SERVER_ONLY

#include "Packets.h"

#include "Buffer.h"
#include <opus/opus.h>
#include "Voice.h"

#include <fstream>

#include "MicFilter.h"

#include "4DKeyBinds.h"

#include "Config.h"

#include "StateVoiceSettings.h"

#include "BytePacket.h"

static StateManager* stateManager;

static std::string deafenSound;
static std::string undeafenSound;
static std::string muteSound;
static std::string unmuteSound;
static const Tex2D* speakerTex;
static const Tex2D* deafenedTex;
static const Tex2D* micTex;
static const Tex2D* mutedTex;
static const Tex2D* disconnectedTex;

struct PacketVoice
{
	uint64_t order;
	std::vector<uint8_t> encodedData;
};

struct PlayerVoice
{
	std::mutex mutex;
	std::map<uint64_t, PacketVoice> decodingQueue;
	OpusDecoder* decoder = nullptr;

	stl::uuid player;
	stl::uuid vcUUID;
	std::shared_ptr<Buffer<float>> buffer;
	Voice* source = nullptr;
	SoLoud::handle voiceHandle = -1;

	PlayerVoice() {}

	PlayerVoice(PlayerVoice&& other) noexcept
	{
		this->player = other.player;
		this->vcUUID = other.vcUUID;
		this->decoder = other.decoder;
		this->buffer = std::move(other.buffer);
		this->source = other.source;
		this->voiceHandle = other.voiceHandle;

		other.player = {};
		other.vcUUID = {};
		other.decoder = nullptr;
		other.source = nullptr;
		other.voiceHandle = -1;
	}

	~PlayerVoice()
	{
		if (decoder)
		{
			opus_decoder_destroy(decoder);
			decoder = nullptr;
		}

		if (voiceHandle != -1)
		{
			AudioManager::soloud.stop(voiceHandle);
			voiceHandle = -1;
		}

		if (source)
		{
			delete source;
			source = nullptr;
		}
	}
};
static std::mutex playerVoicesMutex;
static std::unordered_map<stl::uuid, std::unique_ptr<PlayerVoice>> playerVoices;
static std::thread decoderThread;
static bool talking = false;

#define OPUS_APPLICATION OPUS_APPLICATION_VOIP

void encoderThreadFunc(Buffer<int16_t>* buff)
{
	if (!voiceChatConnected) return;
	if (!config.micEnabled) return;

	int err;
	OpusEncoder* encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION, &err);

	if (!encoder)
	{
		Console::printLine(Console::RED, "Failed to create the Opus encoder: ", err);
		return;
	}

	std::vector<int16_t> frame{};
	frame.resize(FRAME_SIZE);

	std::vector<uint8_t> packetData{};
	packetData.resize(sizeof(PacketVoiceCS) + MAX_ENCODED_DATA_SIZE);

	PacketVoiceCS& packet = *(PacketVoiceCS*)packetData.data();

	while (voiceChatConnected && config.micEnabled)
	{
		while (buff->available() < FRAME_SIZE)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			if (!voiceChatConnected) break;
		}

		if (!voiceChatConnected) break;
		if (!config.micEnabled) break;

		buff->pop(frame.data(), FRAME_SIZE);

		int size = opus_encode(encoder, frame.data(), FRAME_SIZE, packet.encodedData, MAX_ENCODED_DATA_SIZE);
		if (size < 4)
		{
			continue;
		}

		if (StateGame::instanceObj.world.get())
		{
			auto& client = ((WorldClient*)StateGame::instanceObj.world.get())->client;
			if (client)
			{
				Connection::OutMessage message
				{
					C_VOICE_DATA,
					packetData.data(),
					sizeof(PacketVoiceCS) + size
				};
				SteamNetworkingMessage_t* msg =
					message.createMessage(client->connectionHandle, k_nSteamNetworkingSend_UnreliableNoDelay);
				msg->m_idxLane = 0;
				client->Interface->SendMessages(1, &msg, nullptr);

				++packet.order;
			}
		}
	}

	opus_encoder_destroy(encoder);
}

void decoderThreadFunc()
{
	if (!voiceChatConnected) return;

	float buff[FRAME_SIZE];

	while (voiceChatConnected)
	{
		std::unique_lock lock{ playerVoicesMutex };

		for (auto& voice : playerVoices)
		{
			if (!voice.second) continue;

			std::unique_lock lockVoice{ voice.second->mutex };
			if (voice.second->decodingQueue.empty()) continue;

			std::map<uint64_t, PacketVoice> queue = voice.second->decodingQueue;
			voice.second->decodingQueue.clear();
			lockVoice.unlock();

			uint64_t next = queue.begin()->first;
			for (auto& packet : queue)
			{
				for (; next <= packet.first; ++next)
				{
					int samples = 0;

					if (packet.first == next)
					{
						samples = opus_decode_float(voice.second->decoder, packet.second.encodedData.data(), packet.second.encodedData.size(), buff, FRAME_SIZE, 0);
					}
					else
					{
						samples = opus_decode_float(voice.second->decoder, nullptr, 0, buff, FRAME_SIZE, 0);
					}

					if (samples < 0)
					{
						continue;
					}

					voice.second->buffer->push(buff, samples);
				}
			}
		}
	}

	{
		std::unique_lock lock{ playerVoicesMutex };
		playerVoices.clear();
	}
}

void closeAudioStream()
{
	if (audioStream.Information().state != Audijo::StreamState::Closed)
	{
		audioStream.Close();
		while (audioStream.Information().state != Audijo::StreamState::Closed);
		inputEnv = 0.0;
	}
}

void inputCallback(Audijo::Buffer<int16_t>& input, Audijo::Buffer<int16_t>& output, Audijo::CallbackInfo info)
{
	if (stateManager->states.back() == &StateVoiceSettings::instanceObj)
	{
		inputCallbackSimple(input, output, info);
	}

	if (!voiceChatConnected) return;
	if (!postProcessingBuffer) return;
	if (!talking) return;
	if (config.muted) return;
	if (config.deafened) return;
	if (!config.micEnabled) return;

	std::memcpy(postProcessingBuffer, input.data()[0], input.Frames() * sizeof(int16_t));

	double vOg = 0.0;
	double v = micFilter.process(postProcessingBuffer, input.Frames());

	inputEnv = v;

	buff->push(postProcessingBuffer, input.Frames());
}

void cleanup()
{
	if (voiceChatConnected)
	{
		voiceChatConnected = false;

		closeAudioStream();

		if (encoderThread.joinable())
		{
			encoderThread.join();
		}

		if (decoderThread.joinable())
		{
			decoderThread.join();
		}

		if (buff)
		{
			delete buff;
			buff = nullptr;
		}

		if (postProcessingBuffer)
		{
			free(postProcessingBuffer);
			postProcessingBuffer = nullptr;
		}

		std::lock_guard lock{ playerVoicesMutex };
		playerVoices.clear();

		playerToVC.clear();
		vcToPlayer.clear();
	}
}

$hook(void, Connection::Client, cleanup)
{
	original(self);
	cleanup();
}

$hook(void, WorldClient, localPlayerInit, Player* player)
{
	original(self, player);

	self->client->sendMessage({ C_VOICE_CHAT_MOD_VERIFICATION, (uint8_t*)&config.vcUUID, sizeof(stl::uuid) });
}

int pickInputDevice(int target)
{
	if (target <= -1)
		return Audijo::Default;

	auto devices = audioStream.Devices();
	int j = 0;
	for (int i = 0; i < devices.size(); ++i)
	{
		auto& dev = devices[i];
		if (dev.inputChannels)
		{
			if (j == target)
			{
				return dev.id;
			}
			++j;
		}
	}

	return Audijo::NoDevice;
}

$hook(void, WorldClient, handleMessage, const Connection::InMessage& message, Player* player)
{
	if (message.getPacketType() == S_VOICE_DATA && voiceChatConnected)
	{
		const PacketVoiceSC& inPacket = *(const PacketVoiceSC*)message.getMsgData();

		auto* group = AudioManager::getVoiceGroupInfo("voice");
		if (!playerVoices.contains(inPacket.source))
		{
			glm::vec4 pos = AudioManager::hyperplaneView * inPacket.pos;
			//glm::vec4 vel = AudioManager::hyperplaneView * sourcePlayer->interpVel;
			glm::vec4 vel{ 0 };

			int err;
			auto voice = std::make_unique<PlayerVoice>();
			voice->player = inPacket.source;
			voice->vcUUID = inPacket.sourceVCUUID;
			std::string vcUUIDstr = stl::uuid::to_string(voice->vcUUID);
			voice->decoder = opus_decoder_create(SAMPLE_RATE, 1, &err);
			voice->buffer = std::make_shared<Buffer<float>>(SAMPLE_RATE / 2); // 500ms of samples
			voice->source = new Voice(voice->buffer);
			voice->source->set3dMinMaxDistance(1.0f, 32.0f);
			voice->voiceHandle = AudioManager::soloud.play3d(*voice->source, pos.x, pos.y, pos.z, vel.x, vel.y, vel.z, group->volume, false, 0);
			AudioManager::soloud.set3dSourceAttenuation(voice->voiceHandle, SoLoud::AudioSource::ATTENUATION_MODELS::LINEAR_DISTANCE, 1.0f);
			{
				std::lock_guard lock{ playerVoicesMutex };
				playerVoices.insert({ inPacket.source, std::move(voice) });
			}
		}

		auto& voice = playerVoices[inPacket.source];

		{
			std::string vcUUIDstr = stl::uuid::to_string(voice->vcUUID);

			glm::vec4 pos = AudioManager::hyperplaneView * inPacket.pos;
			//glm::vec4 vel = AudioManager::hyperplaneView * self->interpVel;
			glm::vec4 vel{ 0 };

			if (config.playerVolume.contains(vcUUIDstr))
			{
				AudioManager::soloud.setVolume(voice->voiceHandle, config.playerVolume[vcUUIDstr] * group->volume);
			}
			else
			{
				AudioManager::soloud.setVolume(voice->voiceHandle, 1.0f * group->volume);
			}
			AudioManager::soloud.set3dSourceParameters(voice->voiceHandle, pos.x, pos.y, pos.z, vel.x, vel.y, vel.z);
			//voice->source->set3dMinMaxDistance(1.0f, inPacket.crouching ? 8.0f : 32.0f);
			AudioManager::soloud.m3dData[AudioManager::soloud.getVoiceFromHandle_internal(voice->voiceHandle)].m3dMaxDistance = inPacket.crouching ? 8.0f : 32.0f;
		}

		{
			PacketVoice packetVoice;
			packetVoice.encodedData = std::vector<uint8_t>(inPacket.encodedData, inPacket.encodedData + message.getMsgSize() - sizeof(PacketVoiceSC));
			packetVoice.order = inPacket.order;

			std::lock_guard lock{ voice->mutex };
			voice->decodingQueue[packetVoice.order] = packetVoice;
		}

		return;
	}
	else if (message.getPacketType() == S_VOICE_CHAT_MOD_VERIFICATION && !voiceChatConnected)
	{
		voiceChatConnected = true;

		initVoiceChatInput();
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

		decoderThread = std::thread{ decoderThreadFunc };
		decoderThread.detach();

		return;
	}
	else if (message.getPacketType() == S_VOICE_CHAT_PLAYERS)
	{
		playerToVC.clear();
		vcToPlayer.clear();

		BytePacket packet{ message.getMsgData(), message.getMsgSize() };

		size_t playerCount = packet.next<size_t>();
		playerToVC.reserve(playerCount);
		vcToPlayer.reserve(playerCount);

		for (size_t i = 0; i < playerCount; ++i)
		{
			stl::uuid entityID = packet.next<stl::uuid>();
			stl::uuid vcUUID = packet.next<stl::uuid>();
			size_t nameLength = packet.next<size_t>();
			std::string name = std::string{ packet.data + packet.cursor, packet.data + packet.cursor + nameLength };
			packet.cursor += nameLength;

			stl::uuid configUUID = std::bit_cast<stl::uuid>(config.vcUUID);
			if (vcUUID == configUUID) continue;
			std::string vcUUIDstr = stl::uuid::to_string(vcUUID);

			config.playerNames[vcUUIDstr] = name;

			if (!config.playerVolume.contains(vcUUIDstr))
			{
				config.playerVolume[vcUUIDstr] = 1.0;
			}

			playerToVC.insert({ entityID, vcUUID });
			vcToPlayer.insert({ vcUUID, entityID });
		}

		saveConfig();
	}
	return original(self, message, player);
}

$hook(void, WorldClient, updateLocal, StateManager& s, Player* player, double dt)
{
	if (!config.pushToTalk)
	{
		talking = true;
	}

	static double lastPacketTime = glfwGetTime();
	double time = glfwGetTime();

	if (!voiceChatConnected && time - lastPacketTime >= 10.0)
	{
		self->client->sendMessage({ C_VOICE_CHAT_MOD_VERIFICATION, (uint8_t*)&config.vcUUID, sizeof(stl::uuid) });
		lastPacketTime = time;
	}

	return original(self, s, player, dt);
}

TexRenderer vcIconRenderer{};
float iconScale = 1.0f;
$hook(void, Player, renderHud, GLFWwindow* window)
{
	int w, h;
	glfwGetWindowSize(window, &w, &h);

	original(self, window);

	if (StateGame::instanceObj.world->getType() == World::TYPE_CLIENT)
	{
		glDisable(GL_DEPTH_TEST);

		int sprW = 64;
		int sprH = 64;
		int sprWS = sprW * iconScale;
		int sprHS = sprH * iconScale;

		vcIconRenderer.setPos(w - 8 - sprW - (sprWS - sprW) / 2, h - 8 - sprH - (sprHS - sprH) / 2, sprWS, sprHS);
		if (voiceChatConnected)
		{
			if (config.muted || config.deafened)
			{
				vcIconRenderer.texture = config.deafened ? deafenedTex : mutedTex;
				vcIconRenderer.setColor(1, 1, 1, 1);
				vcIconRenderer.render();
			}
			else if (talking)
			{
				vcIconRenderer.texture = micTex;
				vcIconRenderer.setColor(1, 1, 1, inputEnv < 0.1 ? 0.0 : inputEnv);
				vcIconRenderer.render();
			}
		}
		else
		{
			vcIconRenderer.texture = disconnectedTex;
			vcIconRenderer.setColor(1, 1, 1, 1);
			vcIconRenderer.render();
		}
		glEnable(GL_DEPTH_TEST);
	}
}

$hook(void, StateGame, update, StateManager& s, double dt)
{
	iconScale = glm::mix(iconScale, 1.0f, 0.2f);

	return original(self, s, dt);
}

$hook(void, StateIntro, init, StateManager& s)
{
	original(self, s);

	stateManager = &s;

	// initialize opengl stuff
	glewExperimental = true;
	glewInit();
	glfwInit();

	micTex = ResourceManager::get("assets/mic.png", true);
	mutedTex = ResourceManager::get("assets/mic_muted.png", true);
	speakerTex = ResourceManager::get("assets/speaker.png", true);
	deafenedTex = ResourceManager::get("assets/speaker_deafened.png", true);
	disconnectedTex = ResourceManager::get("assets/disconnected.png", true);

	deafenSound = std::format("../../{}/assets/deafen.ogg", fdm::getModPath(fdm::modID));
	undeafenSound = std::format("../../{}/assets/undeafen.ogg", fdm::getModPath(fdm::modID));
	muteSound = std::format("../../{}/assets/mute.ogg", fdm::getModPath(fdm::modID));
	unmuteSound = std::format("../../{}/assets/unmute.ogg", fdm::getModPath(fdm::modID));
	AudioManager::loadSound(deafenSound);
	AudioManager::loadSound(undeafenSound);
	AudioManager::loadSound(muteSound);
	AudioManager::loadSound(unmuteSound);

	vcIconRenderer.shader = ShaderManager::get("tex2DShader");
	vcIconRenderer.texture = disconnectedTex;
	vcIconRenderer.init();
	vcIconRenderer.setColor(1, 1, 1, 0);
	vcIconRenderer.setClip(0, 0, 32, 32);

	loadConfig();
	saveConfig();
}

$hook(void, StateSettings, updateVolume)
{
	updateVoiceVolume();
	return original(self);
}

inline static int getY(gui::Element* element)
{
	// im comparing typeid name strings instead of using dynamic_cast because typeids of 4dminer and typeids of 4dm.h are different
	if (0 == strcmp(typeid(*element).name(), "class gui::Button"))
		return ((gui::Button*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class gui::CheckBox"))
		return ((gui::CheckBox*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class gui::Image"))
		return ((gui::Image*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class gui::Slider"))
		return ((gui::Slider*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class gui::Text"))
		return ((gui::Text*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class gui::TextInput"))
		return ((gui::TextInput*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class fdm::gui::Button"))
		return ((gui::Button*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class fdm::gui::CheckBox"))
		return ((gui::CheckBox*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class fdm::gui::Image"))
		return ((gui::Image*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class fdm::gui::Slider"))
		return ((gui::Slider*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class fdm::gui::Text"))
		return ((gui::Text*)element)->yOffset;
	else if (0 == strcmp(typeid(*element).name(), "class fdm::gui::TextInput"))
		return ((gui::TextInput*)element)->yOffset;
	return 0;
}

$hookIDLate(settingsInit, void, StateSettings, init, StateManager& s)
{
	original(self, s);
	int lowestY = 0;
	for (auto& e : self->mainContentBox.elements)
	{
		if (e == &self->secretButton) // skip the secret button
			continue;

		lowestY = std::max(getY(e), lowestY);
	}
	int oldLowest = lowestY;

	static gui::Text title{};
	title.setText("Voice-Chat:");
	title.alignX(gui::ALIGN_CENTER_X);
	title.offsetY(lowestY += 100);
	title.size = 2;
	self->mainContentBox.addElement(&title);

	static gui::Button btn{};
	btn.setText("Settings");
	btn.alignX(gui::ALIGN_CENTER_X);
	btn.offsetY(lowestY += 50);
	btn.width = 200;
	btn.height = 50;
	btn.user = &s;
	btn.callback = [](void* user)
		{
			StateManager* s = (StateManager*)user;
			s->pushState(&StateVoiceSettings::instanceObj);
		};
	self->mainContentBox.addElement(&btn);
}

void updateConfigValues(bool updateInputDevice)
{
	config.pushToTalkMouseBind = std::clamp(config.pushToTalkMouseBind, -1, GLFW_MOUSE_BUTTON_LAST - GLFW_MOUSE_BUTTON_3);
	for (auto& v : config.playerVolume)
	{
		v.second = std::clamp(v.second, 0.0, 2.0);
	}

	updateMicFilter();
	updateVoiceVolume();

	if (config.micEnabled)
	{
		int device = pickInputDevice(config.inputDevice);
		if (updateInputDevice && voiceChatConnected && device != -1)
		{
			closeAudioStream();

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
		}
	}
	else
	{
		closeAudioStream();
	}

	if (config.deafened)
		vcIconRenderer.texture = deafenedTex;
	else if (config.muted)
		vcIconRenderer.texture = mutedTex;
	else
		vcIconRenderer.texture = micTex;
}

$hook(void, StateSettings, load, GLFWwindow* window)
{
	loadConfig();

	original(self, window);
}

$hook(void, StateSettings, save)
{
	saveConfig();

	original(self);
}

void pushToTalkCallback(GLFWwindow* window, int action, int mods)
{
	if (!config.pushToTalk)
		return;

	if (!config.muted && !config.deafened)
	{
		vcIconRenderer.texture = micTex;
	}

	if (action == GLFW_PRESS)
	{
		talking = config.pushToTalkToggle ? !talking : true;
		iconScale = 1.25f;
	}
	else if (action == GLFW_RELEASE && !config.pushToTalkToggle)
	{
		talking = false;
		iconScale = 1.25f;
	}
}

void mute()
{
	config.muted = !config.muted;

	AudioManager::soloud.play(
		*AudioManager::getSound(config.muted ? muteSound : unmuteSound),
		0.4f,
		0.0f,
		false,
		0);

	iconScale = config.muted ? 0.75f : 1.25f;

	updateMicFilter();

	if (config.deafened)
		vcIconRenderer.texture = deafenedTex;
	else if (config.muted)
		vcIconRenderer.texture = mutedTex;
	else
		vcIconRenderer.texture = micTex;
}

void deafen()
{
	config.deafened = !config.deafened;

	AudioManager::soloud.play(
		*AudioManager::getSound(config.deafened ? deafenSound : undeafenSound),
		0.4f,
		0.0f,
		false,
		0);

	iconScale = config.deafened ? 0.75f : 1.25f;

	updateVoiceVolume();
	updateMicFilter();

	if (config.deafened)
		vcIconRenderer.texture = deafenedTex;
	else if (config.muted)
		vcIconRenderer.texture = mutedTex;
	else
		vcIconRenderer.texture = micTex;
}

void muteCallback(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS) return;

	mute();
}
void deafenCallback(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS) return;

	deafen();
}
void openSettingsCallback(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS) return;

	stateManager->pushState(&StateVoiceSettings::instanceObj);
}

$hook(bool, Player, keyInput, GLFWwindow* window, World* world, int key, int scancode, int action, char mods)
{
	if (!KeyBinds::isLoaded())
	{
		if (key == GLFW_KEY_H)
		{
			pushToTalkCallback(window, action, mods);
			return true;
		}
		if (key == GLFW_KEY_MINUS)
		{
			muteCallback(window, action, mods);
			return true;
		}
		if (key == GLFW_KEY_EQUAL)
		{
			deafenCallback(window, action, mods);
			return true;
		}
		if (key == GLFW_KEY_HOME)
		{
			openSettingsCallback(window, action, mods);
			return true;
		}
	}

	return original(self, window, world, key, scancode, action, mods);
}

$hook(void, Player, mouseButtonInput, GLFWwindow* window, World* world, int button, int action, int mods)
{
	if (config.pushToTalkMouseBind >= 0 && button == config.pushToTalkMouseBind + GLFW_MOUSE_BUTTON_4)
	{
		pushToTalkCallback(window, action, mods);
	}

	return original(self, window, world, button, action, mods);
}

$exec
{
	KeyBinds::addBind("Voice-Chat", "Push To Talk", glfw::H, KeyBindsScope::PLAYER, pushToTalkCallback);
	KeyBinds::addBind("Voice-Chat", "Mute", glfw::Minus, KeyBindsScope::PLAYER, muteCallback);
	KeyBinds::addBind("Voice-Chat", "Deafen", glfw::Equal, KeyBindsScope::PLAYER, deafenCallback);
	KeyBinds::addBind("Voice-Chat", "Open Settings", glfw::Home, KeyBindsScope::PLAYER, openSettingsCallback);
}

$hook(void, EntityPlayer, render, const World* world, const m4::Mat5& MV, bool glasses)
{
	original(self, world, MV, glasses);

	if (!voiceChatConnected)
		return;

	if (self->player != &StateGame::instanceObj.player && !self->player->keys.shift && StateSettings::instanceObj.nametags)
	{
		static TexRenderer tex{ disconnectedTex, ShaderManager::get("tex2DShader") };
		if (!tex.VAO)
		{
			tex.init();
			tex.setClip(0, 0, 32, 32);
			tex.setColor(1, 1, 1, 1);
		}

		if (!playerToVC.contains(self->id))
		{
			tex.texture = disconnectedTex;
		}
		else
		{
			float output = 0.0f;
			if (playerVoices.contains(self->id))
				output = playerVoices[self->id]->source->output;
			tex.texture = micTex;
			tex.setColor(1, 1, 1, output);
		}

		Player& player = StateGame::instanceObj.player;

		glm::vec4 pos = self->getPos() + glm::vec4{ 0.0f, 2.875f, 0.0f, 0.0f };

		glDisable(GL_DEPTH_TEST);

		glm::vec4 transformed = MV * pos;

		if (transformed.z < 0.0f && transformed.z > -16.0f)
		{
			glm::vec4 projected = self->projection3D * glm::vec4{ transformed.x, transformed.y, transformed.z, 1.0f };

			if (projected.w > 0.0f)
			{
				int w, h;
				glfwGetWindowSize(stateManager->window, &w, &h);

				glm::vec3 ndc = projected;
				ndc /= projected.w;

				glm::vec2 screenCoords = glm::vec2{ ndc.x * 0.5f + 0.5f, 1.0f - (ndc.y * 0.5f + 0.5f) } * glm::vec2{ w, h };

				tex.setPos(screenCoords.x - 16, screenCoords.y - 16, 32, 32);
				tex.render();
			}
		}

		glEnable(GL_DEPTH_TEST);
	}
}

#endif
