#include "permission_managed_object.hpp"
#include "Message.hpp"
#include <ctime>
#include <string>
#include <map>
#include <unordered_set>
#include <boost/json.hpp>

class websocket_session;

class Thread : public PermissionManagedObject {
public:
	Thread(PermissionObjectBase* permission_parent, FuzeDBI::Connection* fuze_dbi, int id, int permission_object_id);
	Thread(PermissionObjectBase* permission_parent, boost::json::object thread_json, int author_client_id, FuzeDBI::Connection* fuze_dbi);
	// Thread(PermissionObjectBase* permission_parent, struct db_thread_struct* thread_struct, FuzeDBI::Connection* fuze_dbi);
	// std::string dumpThread() const;
	boost::json::object asJson(const std::optional<FuzeHttp::Client>& client) const;
	boost::json::object asJsonWithMessages(const std::optional<FuzeHttp::Client>& client) const;
	// int getNumberOfPosts() const { return this->posts.size(); };
	// void addInitialPost(json post_json, bool save_to_database);
	// void createPostFromStruct(struct db_post_struct* post_struct);
	void cacheMessage(Message&& message);
	int createMessageFromJson(boost::json::object message_json, int author_client_id);
	// int addPost(json post_json, int id_in_thread, bool save_to_database);
	// int addPost(json post_json, bool save_to_database);
	void deleteMessage(int message_id);
	// bool keyMatchesMessage(std::string key, int message_id) const;
	bool messageExists(int message_id_in_thread) const { return this->messages.contains(message_id_in_thread); }
	int getId() const { return this->id; };
	void addListener(websocket_session* listener);
	void removeListener(websocket_session* listener);
	std::unordered_set<websocket_session*> getListeners() const { return this->listeners; };
	boost::json::array getMessagesAsJson() const;
	std::string dumpMessage(int message_id) const;
	// std::string dumpPermissions(int client_id) const;
	void markAsDeleted();
	std::chrono::time_point<std::chrono::system_clock> getLastMessageTime() const { return this->last_message_created_at; }
	bool isDeleted() const { return this->deleted; }
	boost::json::object getPermissionsAsJson(const std::optional<FuzeHttp::Client>& client) const;
	const Message* getMessage(int message_id_in_thread) const { return &this->messages.at(message_id_in_thread); }
private:
	FuzeDBI::Connection* fuze_dbi;
	int id;
	std::chrono::time_point<std::chrono::system_clock> last_message_created_at;
	// std::vector<Post> posts;
	std::map<int, Message> messages;
	int reply_count = 0;
	std::unordered_set<websocket_session*> listeners;
	// char subject[256];
	boost::json::object thread_as_json;
	bool deleted = false; // It is assumed new Thread object are not marked as deleted, because deleted threads are not retrieved from the database, nor can they be created through the API.
};
