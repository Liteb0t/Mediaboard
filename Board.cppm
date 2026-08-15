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
import Mediaboard.Permission;
import FuzeDBI;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;

export namespace Mediaboard {
#ifdef WITH_WEBRTC
struct Receiver {
	std::shared_ptr<rtc::PeerConnection> conn;
	std::shared_ptr<rtc::Track> track;
};
#endif

class Board : public FuzeHttp::PermissionManagedObject {
public:
	Board(PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, int id, int permission_object_id, std::string slug, std::string title)
			: PermissionManagedObject(permission_parent, permission_object_id, db),
			id(id),
			slug(slug),
			title(title) {
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
				{"create_thread", this->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_THREAD))},
				{"send_message", this->clientHasPermission(client, static_cast<int>(PERMISSION::SEND_MESSAGE))},
				{"delete_post", this->clientHasPermission(client, static_cast<int>(PERMISSION::DELETE_POST))},
				{"upload_file", this->clientHasPermission(client, static_cast<int>(PERMISSION::UPLOAD_FILE))},
			}}
		};
		return board_json;
	}
	int createThread(boost::json::object thread_json, int author_client_id) {
		// Thread thread(this, thread_json, author_client_id, db);
		auto thread = std::make_unique<Thread>(this, thread_json, author_client_id, db);
		int new_thread_id = thread->getId();
		this->ordered_threads.insert(std::make_pair(std::chrono::duration_cast<std::chrono::seconds>(thread->getLastMessageTime().time_since_epoch()).count(), new_thread_id));
		this->threads.emplace(new_thread_id, std::move(thread));
		return new_thread_id;
	}
	int createMessage(boost::json::object message_json, int author_client_id) {
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
	std::string dumpAllThreads(const std::optional<FuzeHttp::Client>& client) const {
		boost::json::array threads_json = boost::json::array();
		for (std::set<std::pair<std::time_t, int>>::const_iterator it = this->ordered_threads.begin(); it != this->ordered_threads.end(); ++it) {
			if (!this->threads.at(it->second)->isDeleted() && this->threads.at(it->second)->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_THREAD))) {
				boost::json::object thread_json = this->threads.at(it->second)->asJson(client);
				threads_json.emplace_back(thread_json);
			}
		}
		std::println("[Board] Finished assembling threads list into JSON");
		return boost::json::serialize(boost::json::value{
			{"type", "thread_catalog"},
			{"threads", threads_json}
		});
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
#ifdef WITH_WEBRTC
	struct {
		std::unordered_map<int, std::shared_ptr<Receiver>> receivers;
		std::shared_ptr<rtc::PeerConnection> peer_connection;
		// rtc::Description::Video media;
		std::shared_ptr<rtc::Track> track; // Advice from Claude: 'if you ever support multiple concurrent sharers, webrtc_room.track as a single shared field won't scale — you'd want to look up the specific sharer's track associated with whatever stream the watcher is requesting, but for your current single-sharer/multi-watcher model this is fine as-is.'
		int connection_id_counter = 0;
	} webrtc_room;
#endif
	int getId() const { return this->id; }
	// int getTitle() const { return this->title; }
private:
	int id;
	std::string slug;
	std::string title;
	std::unordered_map<int, std::unique_ptr<Thread>> threads;
	std::set<std::pair<std::time_t, int>, thread_order_comparator> ordered_threads;
	boost::json::object board_as_json;
}; // class Board
} // namespace Mediaboard
