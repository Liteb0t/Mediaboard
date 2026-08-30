module;
#include <boost/json.hpp>
#include <ctime>
#include <expected>
#include <iostream>
#include <map>
#include <print>
#include <string>
#include <bits/unique_ptr.h>
#include <unordered_set>
export module Mediaboard.Thread;

export import Mediaboard.Message;
import Mediaboard.Permission;
import FuzeDBI;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;

export namespace Mediaboard {
class Thread : public FuzeHttp::PermissionManagedObject {
public:
	// On startup load from database
	Thread(PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, int id, int permission_object_id, int board_id)
			: PermissionManagedObject(permission_parent, permission_object_id, db),
			id(id),
			board_id(board_id) {
		this->cacheAllPermissions();
		std::print("[Thread] ID: {} \tRetrieving messages from database... ", id);
		for (auto message_tuple : db->queryRows<std::tuple<int, int, int, int, int, std::string, std::string>>("SELECT id, thread_id, id_in_thread, created_at, author_client_id, author_username, content FROM message WHERE thread_id = $1 AND deleted = FALSE", id)) {
			int message_id = std::get<0>(message_tuple);
			int thread_id = std::get<1>(message_tuple);
			int id_in_thread = std::get<2>(message_tuple);
			std::print("#{}/{}", thread_id, id_in_thread);
			int seconds_since_epoch = std::get<3>(message_tuple); // TODO use long instead of int
			std::chrono::seconds sec(seconds_since_epoch);
			std::chrono::time_point<std::chrono::system_clock> created_at(sec);
			std::vector<File> message_files;
			for (auto file_tuple : db->queryRows<std::tuple<std::string, std::optional<int>, std::optional<int>, std::optional<std::string>>>("SELECT file_name, width, height, thumbnail_file_extension FROM message_file WHERE message_id = $1", message_id)) {
				message_files.push_back(File{
					.filename = std::get<0>(file_tuple),
					.width = std::get<1>(file_tuple),
					.height = std::get<2>(file_tuple),
					.thumbnail_file_extension = std::get<3>(file_tuple)
				});
			}
			auto message = std::make_unique<Message>(message_id, thread_id, id_in_thread, created_at, std::get<4>(message_tuple), std::get<5>(message_tuple), std::get<6>(message_tuple), message_files);
			this->cacheMessage(std::move(message));
			std::print(", ");
		}
		std::cout << "done." << std::endl;
	}
	// Save thread when JSON is received
	struct Validated {
		Message::Validated post_zero_validated;
	};
	static std::expected<Thread::Validated, std::string> validateInput(const boost::json::object json) {
		Validated validated;

		if (auto post_zero_it = json.find("post_zero"); post_zero_it == json.end())
			return std::unexpected("Missing JSON field: post_zero");
		else if (!post_zero_it->value().is_object())
			return std::unexpected("JSON field 'post_zero' must be an object");
		else {
			boost::json::object post_zero = post_zero_it->value().as_object();
			if (auto message = Message::validateInput(post_zero))
				validated.post_zero_validated = message.value();
			else
				return std::unexpected(message.error());
		}
		return validated;
	}
	Thread(PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, Validated input, int author_client_id, int board_id, int id)
			: PermissionManagedObject(permission_parent, db),
			board_id(board_id),
			id(id) {
		db->query<void>("INSERT INTO thread(id, permission_object_id, board_id) VALUES ($1, $2, $3)", this->id, this->getPermissionObjectId(), this->board_id);
		this->insertMessage(std::make_unique<Message>(db, input.post_zero_validated, author_client_id, this->id, incrementMessageIdInThread()));
	}
	// Thread(PermissionObjectBase* permission_parent, struct db_thread_struct* thread_struct, FuzeDBI::Connection* db);
	// std::string dumpThread() const;
	boost::json::object asJson(const std::optional<FuzeHttp::Client>& client) const {
		return {
			{"id", this->id},
			{"post_zero", this->messages.at(0)->asJson()},
			{"reply_count", this->reply_count},
			{"client_permissions", this->getPermissionsAsJson(client)}
		};
	}
	boost::json::object asJsonWithMessages(const std::optional<FuzeHttp::Client>& client) const {
		boost::json::object thread_json = this->asJson(client);
		thread_json.emplace("messages", this->getMessagesAsJson());
		return thread_json;
	}
	// int getNumberOfPosts() const { return this->posts.size(); };
	// void addInitialPost(json post_json, bool save_to_database);
	// void createPostFromStruct(struct db_post_struct* post_struct);
	void cacheMessage(std::unique_ptr<Message>&& message) {
		if (!message->isDeleted()) {
			this->reply_count++;
		}
		this->last_message_created_at = message->createdAt();
		this->messages.emplace(message->getIdInThread(), std::move(message));
	}
	int incrementMessageIdInThread() {
		int new_message_id_in_thread = db->query<int>("SELECT message_id_seq FROM thread WHERE id = $1", this->id); // TODO fix possible ID clash from non atomic operation
		db->query<void>("UPDATE thread SET message_id_seq = $1 WHERE id = $2", new_message_id_in_thread+1, this->id);
		return new_message_id_in_thread;
	}
	std::expected<Message*, std::string> createMessageFromJson(boost::json::object message_json, int author_client_id) {
		// message_json.emplace("id_in_thread", incrementMessageIdInThread());
		auto message_maybe = Message::validateInput(message_json);
		if (!message_maybe) {
			return std::unexpected(message_maybe.error());
		}
		auto message = std::make_unique<Message>(db, message_maybe.value(), author_client_id, this->id, incrementMessageIdInThread()); // Key is deleted from message_json in its constructor
		auto message_ptr = insertMessage(std::move(message));
		return message_ptr;
	}
	Message* insertMessage(std::unique_ptr<Message> message) {
		auto message_ptr = message.get();
		this->reply_count++;
		this->last_message_created_at = message->createdAt();
		this->messages.emplace(message->getIdInThread(), std::move(message));
		return message_ptr;
	}
	// int addPost(json post_json, int id_in_thread, bool save_to_database);
	// int addPost(json post_json, bool save_to_database);
	void deleteMessage(int id_in_thread) {
		this->messages.at(id_in_thread)->markAsDeleted();
		db->query<void>("UPDATE message SET deleted = TRUE WHERE thread_id = $1 AND id_in_thread = $2", this->id, id_in_thread);
		this->reply_count--;
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
			return !this->clientHasPermission(ws->getClient(), static_cast<int>(PERMISSION::VIEW_THREAD));
		});
	}
	boost::json::array getMessagesAsJson() const {
		boost::json::array multiple_post_json = boost::json::array();
		for (auto it = this->messages.begin(); it != this->messages.end(); ++it) {
			if (!it->second->isDeleted()) {
				// std::cout << "Dumping post " << this->id << "/" << it->second.getIdInThread() << std::endl;
				boost::json::object post_json = it->second->asJson();
				// post_json["is_author"] = keyMatchesMessage(key, it->first);
				multiple_post_json.push_back(post_json);
			}
		}
		return multiple_post_json;
	}
	std::string dumpMessage(int message_id) const  {
		boost::json::object message_json = this->messages.at(message_id)->asJson();
		return boost::json::serialize(message_json);
	}
	// std::string dumpPermissions(int client_id) const;
	void markAsDeleted() {
		this->deleted = true;
		db->query<void>("UPDATE thread SET deleted = TRUE WHERE id = $1", this->id);
		db->query<void>("UPDATE message SET deleted = TRUE WHERE thread_id = $1", this->id);
	}
	std::chrono::time_point<std::chrono::system_clock> getLastMessageTime() const { return this->last_message_created_at; }
	bool isDeleted() const { return this->deleted; }
	boost::json::object getPermissionsAsJson(const std::optional<FuzeHttp::Client>& client) const {
		return {
			{"manage_permissions", this->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS))},
			{"send_message", this->clientHasPermission(client, static_cast<int>(PERMISSION::SEND_MESSAGE))},
			{"delete_post", this->clientHasPermission(client, static_cast<int>(PERMISSION::DELETE_POST))},
			{"upload_file", this->clientHasPermission(client, static_cast<int>(PERMISSION::UPLOAD_FILE))},
		};
	}
	const Message* getMessage(int message_id_in_thread) const { return this->messages.at(message_id_in_thread).get(); }
	int board_id;
private:
	int id;
	// int id_in_board;
	std::chrono::time_point<std::chrono::system_clock> last_message_created_at;
	// std::vector<Post> posts;
	std::map<int, std::unique_ptr<Message>> messages;
	int reply_count = -1;
	std::unordered_set<FuzeHttp::WebsocketSession*> listeners;
	// char subject[256];
	bool deleted = false; // It is assumed new Thread object are not marked as deleted, because deleted threads are not retrieved from the database, nor can they be created through the API.
}; // class Thread
} // namespace FuzeHttp
