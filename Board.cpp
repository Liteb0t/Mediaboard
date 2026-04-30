#include "Board.hpp"
// #include "db_interface.h"
#include <boost/json/serialize.hpp>
#include <sstream>
#include <iostream>

Board::Board(PermissionObjectBase* permission_parent, FuzeDBI::Connection* fuze_dbi)
		: PermissionManagedObject(permission_parent, 1, fuze_dbi),
		fuze_dbi(fuze_dbi) {
}

int Board::createThread(boost::json::object thread_json, int author_client_id) {
	Thread thread(this, thread_json.at("thread").as_object(), author_client_id, fuze_dbi);
	this->threads.emplace(thread.getId(), thread);
	this->ordered_threads.insert(std::make_pair(std::chrono::duration_cast<std::chrono::seconds>(thread.getLastMessageTime().time_since_epoch()).count(), thread.getId()));
	return thread.getId();
}

int Board::createMessage(boost::json::object message_json, int author_client_id) {
	int thread_id = message_json["thread_id"].as_int64();
	Thread* thread = &this->threads.at(thread_id);
	std::time_t old_message_time = std::chrono::duration_cast<std::chrono::seconds>(thread->getLastMessageTime().time_since_epoch()).count();
	int new_post_id = thread->createMessageFromJson(message_json, author_client_id);
	std::time_t new_message_time = std::chrono::duration_cast<std::chrono::seconds>(thread->getLastMessageTime().time_since_epoch()).count();
	this->ordered_threads.erase(std::make_pair(old_message_time, thread_id));
	this->ordered_threads.insert(std::make_pair(new_message_time, thread_id));
	return new_post_id;
}

void Board::deleteThread(int thread_id) {
	this->threads.at(thread_id).markAsDeleted();
}

void Board::deleteMessageFromThread(int message_id, int thread_id) {
	if (message_id != 0) {
		std::cout << "[Board] Deleting message " << message_id << " in thread " << thread_id << std::endl;
		this->threads.at(thread_id).deleteMessage(message_id);
	}
}

/*
void Board::cacheAllThreads() {
	std::cout << "[Board] Retrieving threads from database..." << std::endl;
	struct db_thread_array* thread_list = db_retrieve_threads();
	for (int i = 0; i < thread_list->used; i++) {
		// json thread_json;
		// thread_json["id"] = thread_list->array[i].id;
		// thread_json["number_of_posts"] = thread_list->array[i].number_of_posts;
		// Thread thread(thread_json, false);
		Thread thread(this, &thread_list->array[i], db, fuze_dbi);
		std::cout << thread.getId() << ", ";
		this->threads.insert(std::make_pair(thread.getId(), thread));
	}
	freeThreadArray(thread_list);
	std::cout << "done." << std::endl;

	std::cout << "[Board] Retrieving posts from database..." << std::endl;
	struct db_post_array* post_history = db_retrieve_history();
	for (int i = 0; i < post_history->used; i++) {
		std::cout << "#" << post_history->array[i].thread_id << '/' << post_history->array[i].id_in_thread << ", ";
		this->threads.at(post_history->array[i].thread_id).createPostFromStruct(&post_history->array[i]);
	}
	freePostArray(post_history);
	std::cout << "done." << std::endl;
	
	// Sort threads by most recent post date
	for (std::unordered_map<int, Thread>::const_iterator it = this->threads.begin(); it != this->threads.end(); ++it) {
		this->ordered_threads.insert(std::make_pair(it->second.getLastPostTime(), it->first));
	}

	std::cout << "[Board] Finished retreiving threads and posts from the database." << std::endl;
}

std::string Board::dumpThread(int thread_id, int client_id, std::string key) const {
	nlohmann::json thread_json;
	thread_json["messages"] = this->threads.at(thread_id).getMessagesAsJson(key);
	thread_json["client_permissions"] = this->threads.at(thread_id).getPermissionsAsJson(client_id);
	return thread_json.dump();
}

std::string Board::dumpPermissionsInThread(int thread_id, int client_id) const {
	return this->threads.at(thread_id).dumpPermissions(client_id);
}
*/

std::string Board::dumpAllThreads(const std::optional<FuzeHttp::Client>& client) const {
	boost::json::array threads_json = boost::json::array();
	for (std::set<std::pair<std::time_t, int>>::const_iterator it = this->ordered_threads.begin(); it != this->ordered_threads.end(); ++it) {
		// boost::shared_ptr<Thread> thread = this->getThread(it->second);
		if (!this->threads.at(it->second).isDeleted() && this->threads.at(it->second).clientHasPermission(client, PERMISSION::VIEW_THREAD)) {
			boost::json::object thread_json = this->threads.at(it->second).asJson();
			thread_json["client_permissions"] = this->threads.at(it->second).getPermissionsAsJson(client);
			threads_json.emplace_back(thread_json);
		}
	}
	return boost::json::serialize(boost::json::value{
		{"type", "thread_catalog"},
		{"threads", threads_json}
	});
}

void Board::addListenerToThread(websocket_session* listener, int thread_id) {
	if (threadExists(thread_id)) {
		this->threads.at(thread_id).addListener(listener);
		std::cout << "[Board] Listener added to thread " << thread_id << std::endl;
	}
	else
		std::cout << "[Board] Warning: could not add listener to thread " << thread_id << " because the thread does not exist." << std::endl;
}

void Board::removeListenerFromThread(websocket_session* listener, int thread_id) {
	if (threadExists(thread_id)) {
		this->threads.at(thread_id).removeListener(listener);
		std::cout << "[Board] Listener removed from thread " << thread_id << std::endl;
	}
	else
		std::cout << "[Board] Warning: did not remove listener from thread " << thread_id << " because the thread does not exist." << std::endl;
}

std::string Board::dumpMessage(int thread_id, int message_id) const {
	return this->threads.at(thread_id).dumpMessage(message_id);
}

