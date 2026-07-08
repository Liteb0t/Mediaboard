#include "FuzeHttpState.hpp"
#include "FuzeHttpServer.hpp"

using namespace FuzeHttp;
// FuzeHttp::State::State(FuzeDBI::Connection* fuze_dbi, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates)
// 		: PermissionManager(0, fuze_dbi), fuze_dbi(fuze_dbi), busted_target_to_target(busted_target_to_target), files_generated_from_templates(files_generated_from_templates) {
FuzeHttp::State::State(FuzeHttp::Server* server)
		: PermissionManager(0, server->db), server(server), document_root(server->document_root), db(server->db) {
	this->loadSessions();
	this->loadClients();
	// Link accounts to clients
	for (const auto& client_pair : this->clients) {
		if (client_pair.second.account_id) {
			int account_id = client_pair.second.account_id.value();
			auto it = this->accounts.find(account_id);
			if (it == this->accounts.end())
				throw std::runtime_error(std::format("Client {} refers to account {} which does not exist", client_pair.first, account_id));
			it->second.client_id = client_pair.first;
			std::cout << "Account " <<account_id << " = Client " <<client_pair.first << std::endl;
		}
	}
}

void FuzeHttp::State::loadSessions() {
	for (auto session_tuple : server->db->queryRows<std::tuple<int, std::string, int>>("SELECT client_id, key, created_at FROM session")) {
		int seconds_since_epoch = std::get<2>(session_tuple); // TODO use long instead of int
		std::chrono::seconds sec(seconds_since_epoch);
		std::chrono::time_point<std::chrono::system_clock> created_at(sec);
		FuzeHttp::Session session{
			.client_id = std::get<0>(session_tuple),
			.created_at = created_at
		};
		this->sessions.emplace(std::get<1>(session_tuple), std::move(session));
	}
}

void FuzeHttp::State::loadClients() {
	// TODO clear clients which have expired or dont have an account
	for (auto client_tuple : server->db->queryRows<std::tuple<int, int>>("SELECT id, account_id FROM client")) {
		std::optional<int> account_id;
		if (std::get<1>(client_tuple) != -1)
			account_id = std::get<1>(client_tuple);
		else
			account_id = {};
		Client client{
			.id = std::get<0>(client_tuple),
			.account_id = account_id
		};
		this->clients.emplace(std::get<0>(client_tuple), std::move(client));
	}
}

Client FuzeHttp::State::createClient(std::optional<int> account_id) {
	int new_client_id = server->db->query<int>("SELECT client_id FROM _sequences");
	server->db->query<void>("UPDATE _sequences SET client_id = $1", new_client_id+1);
	std::cout << "[FuzeHttp] Creating new client with ID " << new_client_id << std::endl;
	if (account_id) {
		server->db->query<void>("INSERT INTO client(id, account_id) VALUES ($1, $2)", new_client_id, account_id.value());
		this->accounts.at(account_id.value()).client_id = new_client_id;
	}
	else
		server->db->query<void>("INSERT INTO client(id) VALUES ($1)", new_client_id);
	Client client{.id = new_client_id, .account_id = account_id};
	this->clients.emplace(new_client_id, client);
	return client;
}

std::optional<Client> FuzeHttp::State::getClientIfExists(FuzeHttp::Request req) const {
	auto cookie_header = req.find("Cookie");
	if (cookie_header == req.end())
		return {};
	std::string cookie = cookie_header->value();
	std::string session_id_base64 = cookie.substr(cookie.find("=")+1);
	// TODO trim if multiple cookies found
	std::cout << "[FuzeHttp] Received session ID: '" <<session_id_base64 << "'" << std::endl;
	if (std::unordered_map<std::string, FuzeHttp::Session>::const_iterator it = this->sessions.find(session_id_base64); it != this->sessions.end()) {
		std::cout << "found session";
		auto client_it = this->clients.find(it->second.client_id);
		if (client_it == this->clients.end()) {
			std::print(std::cerr, "[FuzeHttp] Session ID linked to client with id {} which does not exist", it->second.client_id);
			return {};
		}
		else
			return client_it->second;
	}
	else
		return {};
}

std::string FuzeHttp::State::createSession(int client_id) {
	FuzeHttp::Session session{
		.client_id = client_id,
		.created_at = std::chrono::system_clock::now()
	};
	std::string key_base64 = generateKeyBase64(this->sessions);
	server->db->query<void>("INSERT INTO session(client_id, key, created_at) VALUES ($1, $2, $3)", client_id, key_base64, (int)std::chrono::duration_cast<std::chrono::seconds>(session.created_at.time_since_epoch()).count());
	// db->createSession(
	// 	key_base64,
	// 	session.client_id,
	// 	std::chrono::duration_cast<std::chrono::minutes>(session.created_at.time_since_epoch()).count()
	// );
	this->sessions.emplace(key_base64, std::move(session));
	return key_base64;
}

void FuzeHttp::State::clearExpiredSessions() {
	int initial_number_of_sessions = this->sessions.size();
	std::chrono::time_point<std::chrono::system_clock> current_time = std::chrono::system_clock::now();
	std::erase_if(this->sessions, [this, &current_time](const std::pair<std::string, FuzeHttp::Session>& session_pair){
		if (session_pair.second.created_at + this->authorization_token_lifespan < current_time) {
			server->db->query<void>("DELETE FROM session WHERE key = $1", session_pair.first);
			return true;
		}
		else
			return false;
	});
	std::cout << "[shared_state] Cleared " << initial_number_of_sessions - this->sessions.size() << " expired sessions." << std::endl;
}

// For now, only used to create the admin account. therefore granted_group_id will be BUILTIN_GROUPS::ADMINISTRATORS
std::string FuzeHttp::State::createInvite(int granted_group_id) {
	FuzeHttp::Invite invite{
		.granted_group_id = granted_group_id,
		.created_at = std::chrono::system_clock::now()
	};
	std::string key_base64 = FuzeHttp::generateKeyBase64(this->sessions);
	// TODO save invite to database
	std::cout << "[shared_state] Created invite with key " << key_base64 << std::endl;
	this->invites.emplace(key_base64, std::move(invite));
	return key_base64;
}

int FuzeHttp::State::getGrantedGroupIdFromInvite(const std::string& invite_key_base64) const { // returns USERS if none found
	if (std::unordered_map<std::string, FuzeHttp::Invite>::const_iterator invite = this->invites.find(invite_key_base64); invite != this->invites.end())
		return invite->second.granted_group_id;
	else
		return static_cast<int>(BUILTIN_GROUPS::PUBLIC);
}

const std::optional<Client> FuzeHttp::State::getClientFromSession(const std::string& session_id_base64) const {
	if (std::unordered_map<std::string, FuzeHttp::Session>::const_iterator session = this->sessions.find(session_id_base64); session != this->sessions.end())
		return this->clients.at(session->second.client_id);
	else
		return {};
}

void FuzeHttp::State::websocketJoin(WebsocketSession* session) {
	std::lock_guard<std::mutex> lock(mutex_);
	WebsocketSessions.insert(session);
}

void FuzeHttp::State::websocketLeave(WebsocketSession* session) {
	std::lock_guard<std::mutex> lock(mutex_);
	WebsocketSessions.erase(session);
}
