module;
#include <ctime>
#include <string>
#include <iostream>
#include <map>
#include <unordered_set>
#include <boost/json.hpp>
export module Mediaboard.Thread;

export import Mediaboard.Message;
import FuzeDBI;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;

export namespace Mediaboard {
class Thread : public FuzeHttp::PermissionManagedObject {
public:
	Thread(PermissionObjectBase* permission_parent, FuzeDBI::Connection* fuze_dbi, int id, int permission_object_id)
			: PermissionManagedObject(permission_parent, permission_object_id, fuze_dbi),
			id(id),
			fuze_dbi(fuze_dbi) {
		this->cacheAllPermissions();
		this->thread_as_json = {
			{"id", id},
			{"reply_count", 0}
		};
	}
	// Save thread when JSON is received
	Thread(PermissionObjectBase* permission_parent, boost::json::object thread_json, int author_client_id, FuzeDBI::Connection* fuze_dbi)
			: PermissionManagedObject(permission_parent, fuze_dbi),
			fuze_dbi(fuze_dbi) {
		boost::json::object post_zero = thread_json.at("post_zero").as_object();
		this->thread_as_json = thread_json;
		this->id = fuze_dbi->query<int>("SELECT thread_id FROM _sequences");
		fuze_dbi->query<void>("UPDATE _sequences SET thread_id = $1", this->id+1);
		fuze_dbi->query<void>("INSERT INTO thread(id, permission_object_id) VALUES ($1, $2)", this->id, this->getPermissionObjectId());
		this->thread_as_json["id"] = this->id;
		post_zero.emplace("thread_id", this->id);
		int new_message_id = this->createMessageFromJson(std::move(post_zero), author_client_id);
		this->thread_as_json["post_zero"] = this->messages.at(new_message_id).asJson();
		this->thread_as_json["reply_count"] = 0;
	}
	// Thread(PermissionObjectBase* permission_parent, struct db_thread_struct* thread_struct, FuzeDBI::Connection* fuze_dbi);
	// std::string dumpThread() const;
	boost::json::object asJson(const std::optional<FuzeHttp::Client>& client) const {
		boost::json::object thread_json = this->thread_as_json;
		thread_json.emplace("client_permissions", this->getPermissionsAsJson(client));
		return thread_json;
	}
	boost::json::object asJsonWithMessages(const std::optional<FuzeHttp::Client>& client) const {
		boost::json::object thread_json = this->asJson(client);
		thread_json.emplace("messages", this->getMessagesAsJson());
		return thread_json;
	}
	// int getNumberOfPosts() const { return this->posts.size(); };
	// void addInitialPost(json post_json, bool save_to_database);
	// void createPostFromStruct(struct db_post_struct* post_struct);
	void cacheMessage(Message&& message) {
		if (message.getIdInThread() == 0)
			this->thread_as_json["post_zero"] = message.asJson();
		else if (!message.isDeleted()) {
			this->reply_count++;
			this->thread_as_json["reply_count"] = this->reply_count;
		}
		this->messages.emplace(message.getIdInThread(), message);
		this->last_message_created_at = message.createdAt();
	}
	int createMessageFromJson(boost::json::object message_json, int author_client_id) {
		int new_message_id_in_thread = fuze_dbi->query<int>("SELECT message_id_seq FROM thread WHERE id = $1", this->id);
		fuze_dbi->query<void>("UPDATE thread SET message_id_seq = $1 WHERE id = $2", new_message_id_in_thread+1, this->id);
		message_json.emplace("id_in_thread", (size_t)new_message_id_in_thread);
		Message message(message_json, author_client_id, fuze_dbi); // Key is deleted from message_json in its constructor
		if (this->messages.empty())
			this->thread_as_json["post_zero"] = message.asJson();
		else {
			this->reply_count++;
			this->thread_as_json["reply_count"] = this->reply_count;
		}
		this->messages.emplace(message.getIdInThread(), message);
		this->last_message_created_at = message.createdAt();
		return message.getIdInThread();
	}
	// int addPost(json post_json, int id_in_thread, bool save_to_database);
	// int addPost(json post_json, bool save_to_database);
	void deleteMessage(int id_in_thread) {
		this->messages.at(id_in_thread).markAsDeleted();
		fuze_dbi->query<void>("UPDATE message SET deleted = TRUE WHERE thread_id = $1 AND id_in_thread = $2", this->id, id_in_thread);
		this->reply_count--;
		this->thread_as_json["reply_count"] = this->reply_count;
		std::cout << "Erased message " << id_in_thread << " from thread " << this->id << std::endl;
	}
	// bool keyMatchesMessage(std::string key, int message_id) const;
	bool messageExists(int message_id_in_thread) const { return this->messages.contains(message_id_in_thread); }
	int getId() const { return this->id; };
	void addListener(FuzeHttp::WebsocketSession* listener) {
		listeners.insert(listener);
	}
	void removeListener(FuzeHttp::WebsocketSession* listener) {
		listeners.erase(listener);
	}
	std::unordered_set<FuzeHttp::WebsocketSession*> getListeners() const {
		return this->listeners;
	}
	void removeUnauthorizedListeners() {
		std::erase_if(this->listeners, [this](const FuzeHttp::WebsocketSession* ws)->bool{
			return !this->clientHasPermission(ws->getClient(), FuzeHttp::PERMISSION::VIEW_THREAD);
		});
	}
	boost::json::array getMessagesAsJson() const {
		boost::json::array multiple_post_json = boost::json::array();
		for (auto it = this->messages.begin(); it != this->messages.end(); ++it) {
			if (!it->second.isDeleted()) {
				// std::cout << "Dumping post " << this->id << "/" << it->second.getIdInThread() << std::endl;
				boost::json::object post_json = it->second.asJson();
				// post_json["is_author"] = keyMatchesMessage(key, it->first);
				multiple_post_json.push_back(post_json);
			}
		}
		return multiple_post_json;
	}
	std::string dumpMessage(int message_id) const  {
		boost::json::object message_json = this->messages.at(message_id).asJson();
		return boost::json::serialize(message_json);
	}
	// std::string dumpPermissions(int client_id) const;
	void markAsDeleted() {
		this->deleted = true;
		fuze_dbi->query<void>("UPDATE thread SET deleted = TRUE WHERE id = $1", this->id);
		fuze_dbi->query<void>("UPDATE message SET deleted = TRUE WHERE thread_id = $1", this->id);
	}
	std::chrono::time_point<std::chrono::system_clock> getLastMessageTime() const { return this->last_message_created_at; }
	bool isDeleted() const { return this->deleted; }
	boost::json::object getPermissionsAsJson(const std::optional<FuzeHttp::Client>& client) const {
		return {
			{"manage_permissions", this->clientHasPermission(client, FuzeHttp::PERMISSION::MANAGE_PERMISSIONS)},
			{"send_message", this->clientHasPermission(client, FuzeHttp::PERMISSION::SEND_MESSAGE)},
			{"delete_post", this->clientHasPermission(client, FuzeHttp::PERMISSION::DELETE_POST)},
			{"upload_file", this->clientHasPermission(client, FuzeHttp::PERMISSION::UPLOAD_FILE)},
		};
	}
	const Message* getMessage(int message_id_in_thread) const { return &this->messages.at(message_id_in_thread); }
private:
	FuzeDBI::Connection* fuze_dbi;
	int id;
	std::chrono::time_point<std::chrono::system_clock> last_message_created_at;
	// std::vector<Post> posts;
	std::map<int, Message> messages;
	int reply_count = 0;
	std::unordered_set<FuzeHttp::WebsocketSession*> listeners;
	// char subject[256];
	boost::json::object thread_as_json;
	bool deleted = false; // It is assumed new Thread object are not marked as deleted, because deleted threads are not retrieved from the database, nor can they be created through the API.
}; // class Thread
} // namespace FuzeHttp
