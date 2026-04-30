#include "permission_managed_object.hpp"
#include "Message.hpp"
#include <ctime>
#include <string>
#include <unordered_set>
#include <boost/json.hpp>

class websocket_session;

class Thread : public PermissionManagedObject {
public:
	// Thread();
	Thread(PermissionObjectBase* permission_parent, boost::json::object thread_json, int author_client_id, FuzeDBI::Connection* fuze_dbi);
	// Thread(PermissionObjectBase* permission_parent, struct db_thread_struct* thread_struct, FuzeDBI::Connection* fuze_dbi);
	std::string dumpThread() const;
	boost::json::object asJson() const { return this->thread_as_json; }
	// int getNumberOfPosts() const { return this->posts.size(); };
	// void addInitialPost(json post_json, bool save_to_database);
	// void createPostFromStruct(struct db_post_struct* post_struct);
	int createMessageFromJson(boost::json::object message_json, int author_client_id);
	// int addPost(json post_json, int id_in_thread, bool save_to_database);
	// int addPost(json post_json, bool save_to_database);
	void deleteMessage(int message_id);
	// bool keyMatchesMessage(std::string key, int message_id) const;
	bool messageExists(int message_id) const { std::map<int, Message>::const_iterator it = messages.find(message_id); return it != messages.end(); };
	int getId() const { return this->id; };
	void addListener(websocket_session* listener);
	void removeListener(websocket_session* listener);
	std::unordered_set<websocket_session*> getListeners() const { return this->listeners; };
	boost::json::array getMessagesAsJson(std::string key) const;
	std::string dumpMessage(int message_id) const;
	// std::string dumpPermissions(int client_id) const;
	void markAsDeleted();
	std::chrono::time_point<std::chrono::system_clock> getLastMessageTime() const { return this->last_message_created_at; }
	bool isDeleted() const { return this->deleted; }
	boost::json::object getPermissionsAsJson(const std::optional<FuzeHttp::Client>& client) const;
private:
	FuzeDBI::Connection* fuze_dbi;
	int id;
	std::chrono::time_point<std::chrono::system_clock> last_message_created_at;
	// std::vector<Post> posts;
	std::map<int, Message> messages;
	int reply_count = 0;
	std::unordered_set<websocket_session*> listeners;
	// char subject[256];
	// char upload_timestamp[20];
	// char files[4][256]; // Max files is 4, maximum URL length is 255
	// short files_i;
	// std::string name;
	// std::string content;
	boost::json::object thread_as_json;
	bool deleted;
};
