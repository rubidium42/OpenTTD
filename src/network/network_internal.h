/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_internal.h Variables and function used internally. */

#ifndef NETWORK_INTERNAL_H
#define NETWORK_INTERNAL_H

#include "network_func.h"
#include "core/tcp_coordinator.h"
#include "core/tcp_game.h"

#include "../command_type.h"
#include "../command_func.h"
#include "../misc/endian_buffer.hpp"
#include "../strings_type.h"

#ifdef RANDOM_DEBUG
/**
 * If this line is enable, every frame will have a sync test
 *  this is not needed in normal games. Normal is like 1 sync in 100
 *  frames. You can enable this if you have a lot of desyncs on a certain
 *  game.
 * Remember: both client and server have to be compiled with this
 *  option enabled to make it to work. If one of the two has it disabled
 *  nothing will happen.
 */
#define ENABLE_NETWORK_SYNC_EVERY_FRAME

/**
 * In theory sending 1 of the 2 seeds is enough to check for desyncs
 *   so in theory, this next define can be left off.
 */
#define NETWORK_SEND_DOUBLE_SEED
#endif /* RANDOM_DEBUG */

typedef class ServerNetworkGameSocketHandler NetworkClientSocket;

/** Status of the clients during joining. */
enum NetworkJoinStatus : uint8_t {
	NETWORK_JOIN_STATUS_CONNECTING,
	NETWORK_JOIN_STATUS_AUTHORIZING,
	NETWORK_JOIN_STATUS_WAITING,
	NETWORK_JOIN_STATUS_DOWNLOADING,
	NETWORK_JOIN_STATUS_PROCESSING,
	NETWORK_JOIN_STATUS_REGISTERING,

	NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO,
	NETWORK_JOIN_STATUS_END,
};

extern uint32_t _frame_counter_server; // The frame_counter of the server, if in network-mode
extern uint32_t _frame_counter_max; // To where we may go with our clients
extern uint32_t _frame_counter;

extern uint32_t _last_sync_frame; // Used in the server to store the last time a sync packet was sent to clients.

/* networking settings */
extern NetworkAddressList _broadcast_list;

extern uint32_t _sync_seed_1;
#ifdef NETWORK_SEND_DOUBLE_SEED
extern uint32_t _sync_seed_2;
#endif
extern uint32_t _sync_frame;
extern bool _network_first_time;
/* Vars needed for the join-GUI */
extern NetworkJoinStatus _network_join_status;
extern uint8_t _network_join_waiting;
extern uint32_t _network_join_bytes;
extern uint32_t _network_join_bytes_total;
extern ConnectionType _network_server_connection_type;
extern std::string _network_server_invite_code;

/* Variable available for clients. */
extern std::string _network_server_name;

extern uint8_t _network_reconnect;

void NetworkQueryServer(std::string_view connection_string);

void GetBindAddresses(NetworkAddressList *addresses, uint16_t port);
struct NetworkGame *NetworkAddServer(std::string_view connection_string, bool manually = true, bool never_expire = false);
void NetworkRebuildHostList();
void UpdateNetworkGameWindow();

/* From network_command.cpp */
/**
 * Everything we need to know about a command to be able to execute it.
 */
struct CommandPacket {
	CompanyID company = CompanyID::Invalid(); ///< company that is executing the command
	uint32_t frame = 0; ///< the frame in which this packet is executed
	bool my_cmd = false; ///< did the command originate from "me"

	Commands cmd{}; ///< command being executed.
	StringID err_msg{}; ///< string ID of error message to use.
	CommandCallback *callback = nullptr; ///< any callback function executed upon successful completion of the command.
	CommandDataBuffer data{}; ///< command parameters.
};

void NetworkDistributeCommands();
void NetworkExecuteLocalCommandQueue();
void NetworkFreeLocalCommandQueue();
void NetworkSyncCommandQueue(NetworkClientSocket *cs);
void NetworkReplaceCommandClientId(CommandPacket &cp, ClientID client_id);

void ShowNetworkError(StringID error_string);
void NetworkTextMessage(NetworkAction action, TextColour colour, bool self_send, std::string_view name, std::string_view str = {}, StringParameter &&data = {});
uint NetworkCalculateLag(const NetworkClientSocket *cs);
StringID GetNetworkErrorMsg(NetworkErrorCode err);
bool NetworkMakeClientNameUnique(std::string &new_name);

std::string_view ParseCompanyFromConnectionString(std::string_view connection_string, CompanyID *company_id);
NetworkAddress ParseConnectionString(std::string_view connection_string, uint16_t default_port);
std::string NormalizeConnectionString(std::string_view connection_string, uint16_t default_port);

void ClientNetworkEmergencySave();

#endif /* NETWORK_INTERNAL_H */
