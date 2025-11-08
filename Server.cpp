#include <4dm.h>

using namespace fdm;

#include "Voice.h"
#include "BytePacket.h"
#include "Packets.h"

void sendPlayerList(WorldServer* world)
{
	// playerCount
	// player...
	//   entityID
	//   vcUUID
	//   nameLength
	//   nameChars

	BytePacket packet{};
	packet.push(playerToVC.size());

	for (auto& player : playerToVC)
	{
		if (!world->playerIDs.contains(player.first)) continue;
		auto& p = world->playerIDs[player.first];
		stl::uuid eID = p->player->EntityPlayerID;
		if (!world->entityPlayerIDs.contains(eID)) continue;

		EntityPlayer* entity = (EntityPlayer*)world->getEntity(eID);

		packet.push(eID);
		packet.push(player.second);
		packet.push(entity->displayName.size());
		packet.push(entity->displayName.data(), entity->displayName.size());
	}

	world->server.sendMessage({ S_VOICE_CHAT_PLAYERS, packet.data, packet.size }, Connection::Server::TARGET_ALL_CLIENTS, 0, true);
}

$hook(void, WorldServer, handleMessage, const Connection::InMessage& message, double dt)
{
	if (message.getPacketType() == C_VOICE_DATA)
	{
		if (!self->players.contains(message.getClient()))
			return;

		auto& player = self->players[message.getClient()];
		stl::uuid eID = player.player->EntityPlayerID;
		stl::uuid id = player.id;

		if (!playerToVC.contains(id))
			return;

		stl::uuid vcUUID = playerToVC[id];

		std::vector<SteamNetworkingMessage_t*> messages{};
		for (int x = -4; x <= 4; ++x)
		{
			for (int z = -4; z <= 4; ++z)
			{
				for (int w = -4; w <= 4; ++w)
				{
					Chunk* chunk = self->getChunkFromCoords(player.player->pos.x + 8 * x, player.player->pos.z + 8 * z, player.player->pos.w + 8 * w);

					if (!chunk) continue;

					for (auto& entity : chunk->entities)
					{
						if (entity->getName() != "Player") continue;
						if (!self->entityPlayerIDs.contains(entity->id)) continue;

						uint32_t handle = self->entityPlayerIDs.at(entity->id)->handle;

						if (handle == message.getClient()) continue;

						size_t dataSize = message.getMsgSize() - sizeof(PacketVoiceCS);
						uint8_t packetData[sizeof(PacketVoiceSC) + MAX_ENCODED_DATA_SIZE]{ 0 };
						PacketVoiceSC& packet = *(PacketVoiceSC*)packetData;
						const PacketVoiceCS& inPacket = *(const PacketVoiceCS*)message.getMsgData();
						packet.order = inPacket.order;
						std::memcpy(packet.encodedData, inPacket.encodedData, dataSize);
						packet.source = eID;
						packet.sourceVCUUID = vcUUID;
						packet.pos = player.player->pos;
						packet.crouching = player.player->keys.shift;

						Connection::OutMessage outMessage
						{
							S_VOICE_DATA,
							packetData,
							sizeof(PacketVoiceSC) + dataSize
						};
						SteamNetworkingMessage_t* msg =
							outMessage.createMessage(handle, k_nSteamNetworkingSend_UnreliableNoDelay);
						msg->m_idxLane = 0;

						messages.emplace_back(msg);
					}
				}
			}
		}
		self->server.Interface->SendMessages(messages.size(), messages.data(), nullptr);

		return;
	}
	else if (message.getPacketType() == C_VOICE_CHAT_MOD_VERIFICATION)
	{
		if (!self->players.contains(message.getClient()))
			return;

		auto& player = self->players[message.getClient()];

		stl::uuid vcID = *(stl::uuid*)message.getMsgData();

		if (vcToPlayer.contains(vcID))
			return;

		playerToVC[player.id] = vcID;
		vcToPlayer[vcID] = player.id;
		self->server.sendMessage({ S_VOICE_CHAT_MOD_VERIFICATION }, Connection::Server::TARGET_SPECIFIC_CLIENT, message.getClient(), true);

		sendPlayerList(self);

		return;
	}
	return original(self, message, dt);
}

$hook(void, WorldServer, handlePlayerDeath, Player* player, std::string_view reason)
{
	if (self->entityPlayerIDs.contains(player->EntityPlayerID))
	{
		auto& p = self->entityPlayerIDs[player->EntityPlayerID];
		if (playerToVC.contains(p->id))
		{
			vcToPlayer.erase(playerToVC[p->id]);
			playerToVC.erase(p->id);
			sendPlayerList(self);
		}
	}

	return original(self, player, reason);
}

$hookStatic(void, WorldServer, disconnectCallback, uint32_t connHandle, WorldServer* world)
{
	if (!world->players.contains(connHandle) || !world->players[connHandle].player)
		return original(connHandle, world);

	stl::uuid id = world->players[connHandle].id;

	original(connHandle, world);

	if (playerToVC.contains(id))
	{
		vcToPlayer.erase(playerToVC[id]);
		playerToVC.erase(id);
		sendPlayerList(world);
	}
}
