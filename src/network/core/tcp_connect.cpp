/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_connect.cpp Basic functions to create connections without blocking.
 */

#include "../../stdafx.h"
#include "../../thread.h"

#include "tcp.h"
#include "../network_coordinator.h"
#include "../network_internal.h"

#include "../../safeguards.h"

/* static */ std::vector<std::shared_ptr<TCPConnecter>> TCPConnecter::connecters;

/**
 * Create a new connecter for the given address.
 * @param connection_string The address to connect to.
 * @param default_port If not indicated in connection_string, what port to use.
 * @param bind_address The local bind address to use. Defaults to letting the OS find one.
 */
TCPConnecter::TCPConnecter(std::string_view connection_string, uint16_t default_port, const NetworkAddress &bind_address, int family) :
	bind_address(bind_address),
	family(family)
{
	this->connection_string = NormalizeConnectionString(connection_string, default_port);
}

/**
 * Create a new connecter for the server.
 * @param connection_string The address to connect to.
 * @param default_port If not indicated in connection_string, what port to use.
 */
TCPServerConnecter::TCPServerConnecter(std::string_view connection_string, uint16_t default_port) :
	server_address(ServerAddress::Parse(connection_string, default_port))
{
	switch (this->server_address.type) {
		case SERVER_ADDRESS_DIRECT:
			this->connection_string = this->server_address.connection_string;
			break;

		case SERVER_ADDRESS_INVITE_CODE:
			this->status = Status::Connecting;
			_network_coordinator_client.ConnectToServer(this->server_address.connection_string, this);
			break;

		default:
			NOT_REACHED();
	}
}

TCPConnecter::~TCPConnecter()
{
	if (this->resolve_thread.joinable()) {
		this->resolve_thread.join();
	}

	for (const auto &socket : this->sockets) {
		closesocket(socket);
	}
	this->sockets.clear();
	this->sock_to_address.clear();

	if (this->ai != nullptr) freeaddrinfo(this->ai);
}

/**
 * Kill this connecter.
 * It will abort as soon as it can and not call any of the callbacks.
 */
void TCPConnecter::Kill()
{
	/* Delay the removing of the socket till the next CheckActivity(). */
	this->killed = true;
}

/**
 * Start a connection to the indicated address.
 * @param address The address to connection to.
 */
void TCPConnecter::Connect(addrinfo *address)
{
	SOCKET sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
	if (sock == INVALID_SOCKET) {
		Debug(net, 0, "Could not create {} {} socket: {}", NetworkAddress::SocketTypeAsString(address->ai_socktype), NetworkAddress::AddressFamilyAsString(address->ai_family), NetworkError::GetLast().AsString());
		return;
	}

	if (!SetReusePort(sock)) {
		Debug(net, 0, "Setting reuse-port mode failed: {}", NetworkError::GetLast().AsString());
	}

	if (this->bind_address.GetPort() > 0) {
		if (bind(sock, (const sockaddr *)this->bind_address.GetAddress(), this->bind_address.GetAddressLength()) != 0) {
			Debug(net, 1, "Could not bind socket on {}: {}", this->bind_address.GetAddressAsString(), NetworkError::GetLast().AsString());
			closesocket(sock);
			return;
		}
	}

	if (!SetNoDelay(sock)) {
		Debug(net, 1, "Setting TCP_NODELAY failed: {}", NetworkError::GetLast().AsString());
	}
	if (!SetNonBlocking(sock)) {
		Debug(net, 0, "Setting non-blocking mode failed: {}", NetworkError::GetLast().AsString());
	}

	NetworkAddress network_address = NetworkAddress(address->ai_addr, (int)address->ai_addrlen);
	Debug(net, 5, "Attempting to connect to {}", network_address.GetAddressAsString());

	int err = connect(sock, address->ai_addr, (int)address->ai_addrlen);
	if (err != 0 && !NetworkError::GetLast().IsConnectInProgress()) {
		closesocket(sock);

		Debug(net, 1, "Could not connect to {}: {}", network_address.GetAddressAsString(), NetworkError::GetLast().AsString());
		return;
	}

	this->sock_to_address[sock] = std::move(network_address);
	this->sockets.push_back(sock);
}

/**
 * Start the connect() for the next address in the list.
 * @return True iff a new connect() is attempted.
 */
bool TCPConnecter::TryNextAddress()
{
	if (this->current_address >= this->addresses.size()) return false;

	this->last_attempt = std::chrono::steady_clock::now();
	this->Connect(this->addresses[this->current_address++]);

	return true;
}

/**
 * Callback when resolving is done.
 * @param ai A linked-list of address information.
 */
void TCPConnecter::OnResolved(addrinfo *ai)
{
	std::deque<addrinfo *> addresses_ipv4, addresses_ipv6;

	/* Apply "Happy Eyeballs" if it is likely IPv6 is functional. */

	/* Detect if IPv6 is likely to succeed or not. */
	bool seen_ipv6 = false;
	bool resort = true;
	for (addrinfo *runp = ai; runp != nullptr; runp = runp->ai_next) {
		if (runp->ai_family == AF_INET6) {
			seen_ipv6 = true;
		} else if (!seen_ipv6) {
			/* We see an IPv4 before an IPv6; this most likely means there is
			 * no IPv6 available on the system, so keep the order of this
			 * list. */
			resort = false;
			break;
		}
	}

	/* Convert the addrinfo into NetworkAddresses. */
	for (addrinfo *runp = ai; runp != nullptr; runp = runp->ai_next) {
		/* Skip entries if the family is set and it is not matching. */
		if (this->family != AF_UNSPEC && this->family != runp->ai_family) continue;

		if (resort) {
			if (runp->ai_family == AF_INET6) {
				addresses_ipv6.emplace_back(runp);
			} else {
				addresses_ipv4.emplace_back(runp);
			}
		} else {
			this->addresses.emplace_back(runp);
		}
	}

	/* If we want to resort, make the list like IPv6 / IPv4 / IPv6 / IPv4 / ..
	 * for how ever many (round-robin) DNS entries we have. */
	if (resort) {
		while (!addresses_ipv4.empty() || !addresses_ipv6.empty()) {
			if (!addresses_ipv6.empty()) {
				this->addresses.push_back(addresses_ipv6.front());
				addresses_ipv6.pop_front();
			}
			if (!addresses_ipv4.empty()) {
				this->addresses.push_back(addresses_ipv4.front());
				addresses_ipv4.pop_front();
			}
		}
	}

	if (_debug_net_level >= 6) {
		if (this->addresses.empty()) {
			Debug(net, 6, "{} did not resolve", this->connection_string);
		} else {
			Debug(net, 6, "{} resolved in:", this->connection_string);
			for (const auto &address : this->addresses) {
				Debug(net, 6, "- {}", NetworkAddress(address->ai_addr, (int)address->ai_addrlen).GetAddressAsString());
			}
		}
	}

	this->current_address = 0;
}

/**
 * Start resolving the hostname.
 *
 * This function must change "status" to either Status::FAILURE
 * or Status::CONNECTING before returning.
 */
void TCPConnecter::Resolve()
{
	/* Port is already guaranteed part of the connection_string. */
	NetworkAddress address = ParseConnectionString(this->connection_string, 0);

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	std::string port_name = fmt::format("{}", address.GetPort());

	static bool getaddrinfo_timeout_error_shown = false;
	auto start = std::chrono::steady_clock::now();

	addrinfo *ai;
	int error = getaddrinfo(address.GetHostname().c_str(), port_name.c_str(), &hints, &ai);

	auto end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
	if (!getaddrinfo_timeout_error_shown && duration >= std::chrono::seconds(5)) {
		Debug(net, 0, "getaddrinfo() for address \"{}\" took {} seconds", this->connection_string, duration.count());
		Debug(net, 0, "  This is likely an issue in the DNS name resolver's configuration causing it to time out");
		getaddrinfo_timeout_error_shown = true;
	}

	if (error != 0) {
		Debug(net, 0, "Failed to resolve DNS for {}", this->connection_string);
		this->status = Status::Failure;
		return;
	}

	this->ai = ai;
	this->OnResolved(ai);

	this->status = Status::Connecting;
}

/**
 * Thunk to start Resolve() on the right instance.
 */
/* static */ void TCPConnecter::ResolveThunk(TCPConnecter *connecter)
{
	connecter->Resolve();
}

/**
 * Check if there was activity for this connecter.
 * @return True iff the TCPConnecter is done and can be cleaned up.
 */
bool TCPConnecter::CheckActivity()
{
	if (this->killed) return true;

	switch (this->status) {
		case Status::Init:
			/* Start the thread delayed, so the vtable is loaded. This allows classes
			 * to overload functions used by Resolve() (in case threading is disabled). */
			if (StartNewThread(&this->resolve_thread, "ottd:resolve", &TCPConnecter::ResolveThunk, this)) {
				this->status = Status::Resolving;
				return false;
			}

			/* No threads, do a blocking resolve. */
			this->Resolve();

			/* Continue as we are either failed or can start the first
			 * connection. The rest of this function handles exactly that. */
			break;

		case Status::Resolving:
			/* Wait till Resolve() comes back with an answer (in case it runs threaded). */
			return false;

		case Status::Failure:
			/* Ensure the OnFailure() is called from the game-thread instead of the
			 * resolve-thread, as otherwise we can get into some threading issues. */
			this->OnFailure();
			return true;

		case Status::Connecting:
		case Status::Connected:
			break;
	}

	/* If there are no attempts pending, connect to the next. */
	if (this->sockets.empty()) {
		if (!this->TryNextAddress()) {
			/* There were no more addresses to try, so we failed. */
			this->OnFailure();
			return true;
		}
		return false;
	}

	fd_set write_fd;
	FD_ZERO(&write_fd);
	for (const auto &socket : this->sockets) {
		FD_SET(socket, &write_fd);
	}

	timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 0;
	int n = select(FD_SETSIZE, nullptr, &write_fd, nullptr, &tv);
	/* select() failed; hopefully next try it doesn't. */
	if (n < 0) {
		/* select() normally never fails; so hopefully it works next try! */
		Debug(net, 1, "select() failed: {}", NetworkError::GetLast().AsString());
		return false;
	}

	/* No socket updates. */
	if (n == 0) {
		/* Wait 250ms between attempting another address. */
		if (std::chrono::steady_clock::now() < this->last_attempt + std::chrono::milliseconds(250)) return false;

		/* Try the next address in the list. */
		if (this->TryNextAddress()) return false;

		/* Wait up to 3 seconds since the last connection we started. */
		if (std::chrono::steady_clock::now() < this->last_attempt + std::chrono::milliseconds(3000)) return false;

		/* More than 3 seconds no socket reported activity, and there are no
		 * more address to try. Timeout the attempt. */
		Debug(net, 0, "Timeout while connecting to {}", this->connection_string);

		for (const auto &socket : this->sockets) {
			closesocket(socket);
		}
		this->sockets.clear();
		this->sock_to_address.clear();

		this->OnFailure();
		return true;
	}

	/* If a socket is writeable, it is either in error-state or connected.
	 * Remove all sockets that are in error-state and mark the first that is
	 * not in error-state as the socket we will use for our connection. */
	SOCKET connected_socket = INVALID_SOCKET;
	for (auto it = this->sockets.begin(); it != this->sockets.end(); /* nothing */) {
		NetworkError socket_error = GetSocketError(*it);
		if (socket_error.HasError()) {
			Debug(net, 1, "Could not connect to {}: {}", this->sock_to_address[*it].GetAddressAsString(), socket_error.AsString());
			closesocket(*it);
			this->sock_to_address.erase(*it);
			it = this->sockets.erase(it);
			continue;
		}

		/* No error but writeable means connected. */
		if (connected_socket == INVALID_SOCKET && FD_ISSET(*it, &write_fd)) {
			connected_socket = *it;
		}

		it++;
	}

	/* All the writable sockets were in error state. So nothing is connected yet. */
	if (connected_socket == INVALID_SOCKET) return false;

	/* Close all sockets except the one we picked for our connection. */
	for (auto it = this->sockets.begin(); it != this->sockets.end(); /* nothing */) {
		if (connected_socket != *it) {
			closesocket(*it);
		}
		this->sock_to_address.erase(*it);
		it = this->sockets.erase(it);
	}

	Debug(net, 3, "Connected to {}", this->connection_string);
	if (_debug_net_level >= 5) {
		Debug(net, 5, "- using {}", NetworkAddress::GetPeerName(connected_socket));
	}

	this->OnConnect(connected_socket);
	this->status = Status::Connected;
	return true;
}

/**
 * Check if there was activity for this connecter.
 * @return True iff the TCPConnecter is done and can be cleaned up.
 */
bool TCPServerConnecter::CheckActivity()
{
	if (this->killed) return true;

	switch (this->server_address.type) {
		case SERVER_ADDRESS_DIRECT:
			return TCPConnecter::CheckActivity();

		case SERVER_ADDRESS_INVITE_CODE:
			/* Check if a result has come in. */
			switch (this->status) {
				case Status::Failure:
					this->OnFailure();
					return true;

				case Status::Connected:
					this->OnConnect(this->socket);
					return true;

				default:
					break;
			}

			return false;

		default:
			NOT_REACHED();
	}
}

/**
 * The connection was successfully established.
 * This socket is fully setup and ready to send/recv game protocol packets.
 * @param sock The socket of the established connection.
 */
void TCPServerConnecter::SetConnected(SOCKET sock)
{
	assert(sock != INVALID_SOCKET);

	this->socket = sock;
	this->status = Status::Connected;
}

/**
 * The connection couldn't be established.
 */
void TCPServerConnecter::SetFailure()
{
	this->status = Status::Failure;
}

/**
 * Check whether we need to call the callback, i.e. whether we
 * have connected or aborted and call the appropriate callback
 * for that. It's done this way to ease on the locking that
 * would otherwise be needed everywhere.
 */
/* static */ void TCPConnecter::CheckCallbacks()
{
	TCPConnecter::connecters.erase(
		std::remove_if(TCPConnecter::connecters.begin(), TCPConnecter::connecters.end(),
				   [](auto &connecter) { return connecter->CheckActivity(); }),
		TCPConnecter::connecters.end());
}

/** Kill all connection attempts. */
/* static */ void TCPConnecter::KillAll()
{
	TCPConnecter::connecters.clear();
}
