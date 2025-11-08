#pragma once

#include <4dm.h>

inline constexpr fdm::Packet::ClientPacket C_VOICE_DATA = (fdm::Packet::ClientPacket)0xFFF0;
inline constexpr fdm::Packet::ServerPacket S_VOICE_DATA = (fdm::Packet::ServerPacket)0xFFF0;
inline constexpr fdm::Packet::ClientPacket C_VOICE_CHAT_MOD_VERIFICATION = (fdm::Packet::ClientPacket)0xFFF1;
inline constexpr fdm::Packet::ServerPacket S_VOICE_CHAT_MOD_VERIFICATION = (fdm::Packet::ServerPacket)0xFFF1;
inline constexpr fdm::Packet::ServerPacket S_VOICE_CHAT_PLAYERS = (fdm::Packet::ServerPacket)0xFFF2;

struct PacketVoiceCS
{
	uint64_t order;
	uint8_t encodedData[];
};

struct PacketVoiceSC
{
	glm::vec4 pos;
	bool crouching;
	fdm::stl::uuid source;
	fdm::stl::uuid sourceVCUUID;
	uint64_t order;
	uint8_t encodedData[];
};
