module;
#ifdef WITH_WEBRTC
#include "rtc/peerconnection.hpp"
#include <rtc/rtc.hpp>
#include "rtc/track.hpp"
#endif
#include <boost/json.hpp>
#include <bits/unique_ptr.h>
#include <ctime>
#include <iostream>
#include <print>
#include <set>
#include <unordered_set>
export module Mediaboard.Board;

export import Mediaboard.Thread;
#ifdef WITH_WEBRTC
import Mediaboard.Room;
#endif
import Mediaboard.Permission;
import FuzeDBI;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;

export namespace Mediaboard {

class Board : public FuzeHttp::PermissionManagedObject {
public:
	Board(PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, int id, int permission_object_id, std::string slug, std::string title)
			: PermissionManagedObject(permission_parent, permission_object_id, db),
			id(id),
			slug(slug),
			title(title) {
	}
	// Save board when JSON is received
	Board(PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, boost::json::object board_json)
			: PermissionManagedObject(permission_parent, db) {
		this->id = db->query<int>("SELECT board_id FROM _sequences");
		this->slug = board_json.at("slug").as_string();
		this->title = board_json.at("title").as_string();
		db->query<void>("UPDATE _sequences SET board_id = $1", this->id+1);
		db->query<void>("INSERT INTO board(id, permission_object_id, slug, title) VALUES ($1, $2, $3, $4)", id, this->getPermissionObjectId(), slug, title);
	}
	void cacheAllThreads() {
		std::cout << "[Board] Retrieving threads from database..." << std::endl;
		for (auto thread_tuple : db->queryRows<std::tuple<int, int>>("SELECT id, permission_object_id FROM thread WHERE deleted = FALSE AND board_id = $1", this->id)) {
			// Thread thread(this, db, std::get<0>(thread_tuple), std::get<1>(thread_tuple), this->id);
			auto thread = std::make_unique<Thread>(this, db, std::get<0>(thread_tuple), std::get<1>(thread_tuple), this->id);
			std::cout << thread->getId() << ", ";
			this->threads.insert(std::make_pair(thread->getId(), std::move(thread)));
		}
		std::cout << "done." << std::endl;

		// Mark invalid threads as deleted. Sometimes when an error occurs whilst creating a thread, it saves the thread to the DB but not its corresponding message.
		for (auto& [id, thread] : this->threads) {
			if (!thread->messageExists(0))
				thread->markAsDeleted();
		}

		// Sort threads by most recent message date
		for (auto& [id, thread] : this->threads) {
			this->ordered_threads.insert(std::make_pair(std::chrono::duration_cast<std::chrono::seconds>(thread->getLastMessageTime().time_since_epoch()).count(), id));
		}

		std::cout << "[Board] Finished retreiving threads and posts from the database." << std::endl;
	}
	boost::json::object asJson(const std::optional<FuzeHttp::Client>& client) const {
		// boost::json::object board_json = this->board_as_json;
		boost::json::object board_json = {
			{"id", id},
			{"slug", slug},
			{"title", title},
			{"client_permissions", {
				{"manage_permissions", this->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS))},
				{"create_board", this->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_BOARD))},
				{"create_thread", this->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_THREAD))},
				{"send_message", this->clientHasPermission(client, static_cast<int>(PERMISSION::SEND_MESSAGE))},
				{"delete_post", this->clientHasPermission(client, static_cast<int>(PERMISSION::DELETE_POST))},
				{"upload_file", this->clientHasPermission(client, static_cast<int>(PERMISSION::UPLOAD_FILE))},
			}}
		};
		return board_json;
	}
#ifdef WITH_WEBRTC
	// int createRoom() {
	// 	int new_room_id = room_id_seq++;
	// 	auto room = std::make_shared<Room>(new_room_id, this->id);
	// 	this->rooms.emplace(new_room_id, std::move(room));
	// 	return new_room_id;
	// }
	boost::json::object getRoomsAsJson() const {
		boost::json::array rooms_json = boost::json::array();
		for (auto& [room_id, weak_room] : this->rooms) {
			if (auto strong_room = weak_room.lock())
				rooms_json.push_back(strong_room->asJson());
		}
		return {{"rooms", rooms_json}};
	}
	std::optional<std::shared_ptr<Room>> getSharedRoomIfExists(int room_id) const {
		auto it = rooms.find(room_id);
		if (it != rooms.end()) {
			if (auto strong_room = it->second.lock())
				return strong_room;
		}
		return {};
	}
	void clearEmptyRooms() { // TODO use this at some point
		std::erase_if(this->rooms, [this](const auto& id_weak_room_pair)->bool{
			return id_weak_room_pair.second.expired();
		});
	}
#endif
	int createThread(boost::json::object thread_json, int author_client_id) {
		// int new_thread_id_in_board = db->query<int>("SELECT thread_id_seq FROM board WHERE id = $1", this->id);
		// db->query<void>("UPDATE board SET thread_id_seq = $1 WHERE id = $2", new_thread_id_in_board+1, this->id);
		int new_thread_id = db->query<int>("SELECT thread_id FROM _sequences");
		db->query<void>("UPDATE _sequences SET thread_id = $1", new_thread_id+1);
		auto thread = std::make_unique<Thread>(this, thread_json, author_client_id, db, this->id);
		// int new_thread_id = thread->getId();
		this->ordered_threads.insert(std::make_pair(std::chrono::duration_cast<std::chrono::seconds>(thread->getLastMessageTime().time_since_epoch()).count(), new_thread_id));
		this->threads.emplace(new_thread_id, std::move(thread));
		return new_thread_id;
	}
	int createMessage(boost::json::object message_json, int author_client_id) {
		// TODO better validate json input
		int thread_id = message_json["thread_id"].as_int64();
		Thread* thread = this->getThread(thread_id);
		std::time_t old_message_time = std::chrono::duration_cast<std::chrono::seconds>(thread->getLastMessageTime().time_since_epoch()).count();
		int new_post_id = thread->createMessageFromJson(message_json, author_client_id);
		std::time_t new_message_time = std::chrono::duration_cast<std::chrono::seconds>(thread->getLastMessageTime().time_since_epoch()).count();
		this->ordered_threads.erase(std::make_pair(old_message_time, thread_id));
		this->ordered_threads.insert(std::make_pair(new_message_time, thread_id));
		return new_post_id;
	}
	void deleteThread(int thread_id) {
		this->threads.at(thread_id)->markAsDeleted();
	}
	void deleteMessageFromThread(int message_id, int thread_id) {
		if (message_id != 0) {
			std::cout << "[Board] Deleting message " << message_id << " in thread " << thread_id << std::endl;
			this->threads.at(thread_id)->deleteMessage(message_id);
		}
		else
			throw std::runtime_error("Can't delete message 0 from thread");
	}
	// std::string dumpLastThread() const;
	// bool keyMatchesMessageInThread(std::string key, int message_id, int thread_id) const;
	boost::json::object getThreadsAsJson(const std::optional<FuzeHttp::Client>& client) const {
		boost::json::array threads_json = boost::json::array();
		for (std::set<std::pair<std::time_t, int>>::const_iterator it = this->ordered_threads.begin(); it != this->ordered_threads.end(); ++it) {
			if (!this->threads.at(it->second)->isDeleted() && this->threads.at(it->second)->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_THREAD))) {
				boost::json::object thread_json = this->threads.at(it->second)->asJson(client);
				threads_json.emplace_back(thread_json);
			}
		}
		std::println("[Board] Finished assembling threads list into JSON");
		return {
			{"type", "thread_catalog"},
			{"threads", threads_json}
		};
	}
	// std::string dumpThread(int thread_id, int client_id, std::string key) const;
	// std::string dumpPermissionsInThread(int thread_id, int client_id) const;
	boost::json::object getThreadPermissionsAsJson(int thread_id, const std::optional<FuzeHttp::Client>& client) const {
		return this->threads.at(thread_id)->getPermissionsAsJson(client);
	}
	bool threadExists(int thread_id) const { auto it = threads.find(thread_id); return it != threads.end(); };
	bool messageExistsInThread(int message_id, int thread_id) const { return this->threads.at(thread_id)->messageExists(message_id); }
	void addListenerToThread(FuzeHttp::WebsocketSession* listener, int thread_id) {
		if (threadExists(thread_id)) {
			this->threads.at(thread_id)->addListener(listener);
			std::cout << "[Board] Listener added to thread " << thread_id << std::endl;
		}
		else
			std::cout << "[Board] Warning: could not add listener to thread " << thread_id << " because the thread does not exist." << std::endl;
	}
	void removeListenerFromThread(FuzeHttp::WebsocketSession* listener, int thread_id) {
		if (threadExists(thread_id)) {
			this->threads.at(thread_id)->removeListener(listener);
			std::cout << "[Board] Listener removed from thread " << thread_id << std::endl;
		}
		else
			std::cout << "[Board] Warning: did not remove listener from thread " << thread_id << " because the thread does not exist." << std::endl;
	}
	void removeUnauthorizedListenersFromThread(int thread_id) {
		this->threads.at(thread_id)->removeUnauthorizedListeners();
	}
	std::unordered_set<FuzeHttp::WebsocketSession*> getListenersFromThread(int thread_id) const { return this->threads.at(thread_id)->getListeners(); };
	std::string dumpMessage(int thread_id, int message_id) const {
		return this->threads.at(thread_id)->dumpMessage(message_id);
	}
	struct thread_order_comparator {
		bool operator() (std::pair<std::time_t, int> left, std::pair<std::time_t, int> right) const {
			if (left.first > right.first)
				return true;
			else if (left.first < right.first)
				return false;
			if (left.second > right.second)
				return true;
			else if (left.second < right.second)
				return false;
			else
				return false;
		}
	};
	Thread* getThread(int thread_id) const { return this->threads.at(thread_id).get(); }
	void addGroupPermissionCollectionToThread(int group_id, int thread_id) { this->threads.at(thread_id)->addGroupPermissionCollection(group_id); }
	void addAccountPermissionCollectionToThread(int account_id, int thread_id) { this->threads.at(thread_id)->addAccountPermissionCollection(account_id); }
	void setGroupPermissionForThread(int group_id, int permission_number, FuzeHttp::THREE_STATE_SETTING setting, int thread_id) { this->threads.at(thread_id)->setGroupPermission(group_id, permission_number, setting); }
	void setAccountPermissionForThread(int account_id, int permission_number, FuzeHttp::THREE_STATE_SETTING setting, int thread_id) { this->threads.at(thread_id)->setAccountPermission(account_id, permission_number, setting); }
	void removeGroupPermissionCollectionFromThread(int group_id, int thread_id) { this->threads.at(thread_id)->removeGroupPermissionCollection(group_id); }
	void removeAccountPermissionCollectionFromThread(int account_id, int thread_id) { this->threads.at(thread_id)->removeAccountPermissionCollection(account_id); }
	void markAsDeleted() {
		this->deleted = true;
		db->query<void>("UPDATE board SET deleted = TRUE WHERE id = $1", this->id);
		// db->query<void>("UPDATE thread SET deleted = TRUE WHERE board_id = $1", this->id);
	}
	int getId() const { return this->id; }
	const std::string& getSlug() const { return this->slug; }
	void setSlug(const std::string& new_slug) {
		if (new_slug != slug) {
			this->slug = new_slug;
			db->query<void>("UPDATE board SET slug = $1 WHERE id = $2", new_slug, id);
		}
	}
	void setTitle(const std::string& new_title) {
		if (new_title != title) {
			this->title = new_title;
			db->query<void>("UPDATE board SET title = $1 WHERE id = $2", new_title, id);
		}
	}
	int room_id_seq = 0;
	std::unordered_map<int, std::weak_ptr<Room>> rooms;
	inline bool isDeleted() const { return this->deleted; }
	// int getTitle() const { return this->title; }
	inline static const size_t MAX_SLUG = 32;
	inline static const size_t MAX_TITLE = 64;
private:
	int id;
	std::string slug;
	std::string title;
	std::unordered_map<int, std::unique_ptr<Thread>> threads;
#ifdef WITH_WEBRTC
#endif
	std::set<std::pair<std::time_t, int>, thread_order_comparator> ordered_threads;
	boost::json::object board_as_json;
	bool deleted = false; // It is assumed new Board objects are not marked as deleted, because deleted threads are not retrieved from the database, nor can they be created through the API.
}; // class Board
} // namespace Mediaboard
