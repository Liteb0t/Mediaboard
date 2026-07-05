#pragma once
#include "FuzeDBI.hpp"
#include "FuzeHttp.hpp"
#include "FuzeHttpServer.hpp"
#include "FuzeHttpUtils.hpp"

namespace FuzeHttp {
class State : public PermissionManager {
public:
	// State(FuzeDBI::Connection* fuze_dbi, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);
	State(FuzeHttp::Server* server);
	std::optional<Client> getClientIfExists(FuzeHttp::Request req) const;
	Client createClient(std::optional<int> account_id = {});
	// std::variant<Client, FuzeHttp::Response> getRequiredClient(FuzeHttp::Request req) const;
	std::string createSession(int client_id);
	std::string createInvite(int granted_group_id);
	int getGrantedGroupIdFromInvite(const std::string& invite_key_base64) const; // returns PUBLIC if none found
	void clearExpiredSessions();
	const std::filesystem::path& getDocumentRoot() const { return document_root; }
	// const std::unordered_map<std::string, std::string> busted_target_to_target;
	// const std::unordered_set<std::string> files_generated_from_templates;
	virtual void start() {};
	FuzeHttp::Server* server;
protected:
	const std::optional<Client> getClientFromSession(const std::string& session_id_base64) const;

	// void createOwnerAccount(DatabaseConnection* db, const std::string& username, const std::string& password);

	// std::variant<http::file_body::value_type, FuzeHttp::Response> openFile(std::filesystem::path path) const;


	std::unordered_map<int, Client> clients;
	std::unordered_map<std::string /*key_base64*/, Session> sessions;
	std::unordered_map<std::string /*key_base64*/, Invite> invites;
	std::filesystem::path document_root;
	// std::unordered_map<std::filesystem::path, std::string> document_etags;
	// FuzeDBI::Connection* fuze_dbi;
	std::vector<FuzeHttp::TemplateMacro*> options;
private:
	void loadSessions();
	void loadClients();
	const std::chrono::duration<unsigned int> authorization_token_lifespan = std::chrono::days(365);
	// friend class FuzeHttp::Server;
}; // class State
} // namespace FuzeHttp
