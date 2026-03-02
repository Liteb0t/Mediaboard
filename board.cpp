#include "board.hpp"
// #include "db_interface.h"
#include <sstream>
#include <iostream>

Board::Board(boost::shared_ptr<PermissionObjectBase> permission_parent) : PermissionManagedObject(permission_parent, 1 /* temporary ID until multi board update*/) {
	// this->thread_limit=50; // MAX_THREADS_PER_BOARD
	// this->post_limit = 100;
	// this->cacheAllThreads();
}

int Board::createThread(json thread_json) {
	int new_permission_object_id = db_get_unique_permission_object_id();
	Thread thread(shared_from_this(), thread_json, new_permission_object_id);
	this->threads.emplace(thread.getId(), thread);
	this->ordered_threads.insert(std::make_pair(thread.getLastPostTime(), thread.getId()));
	return thread.getId();
}

int Board::createPost(json post_json) {
	int thread_id = post_json["thread_id"].template get<int>();
	Thread* thread = &this->threads.at(thread_id);
	std::time_t old_post_time = thread->getLastPostTime();
	int new_post_id =  thread->createPostFromJson(post_json);
	std::time_t new_post_time = thread->getLastPostTime();
	this->ordered_threads.erase(std::make_pair(old_post_time, thread_id));
	this->ordered_threads.insert(std::make_pair(new_post_time, thread_id));
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

bool Board::keyMatchesMessageInThread(std::string key, int message_id, int thread_id) const {
	return this->threads.at(thread_id).keyMatchesMessage(key, message_id);
}

void Board::cacheAllThreads() {
	std::cout << "[Board] Retrieving threads from database..." << std::endl;
	struct db_thread_array* thread_list = db_retrieve_threads();
	for (int i = 0; i < thread_list->used; i++) {
		// json thread_json;
		// thread_json["id"] = thread_list->array[i].id;
		// thread_json["number_of_posts"] = thread_list->array[i].number_of_posts;
		// Thread thread(thread_json, false);
		Thread thread(shared_from_this(), &thread_list->array[i]);
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

std::string Board::dumpAllThreads(int client_id) const {
	json multiple_thread_json;
	multiple_thread_json["type"] = "thread_catalog";
	multiple_thread_json["threads"] = json::array();
	for (std::set<std::pair<std::time_t, int>>::const_iterator it = this->ordered_threads.begin(); it != this->ordered_threads.end(); ++it) {
		boost::shared_ptr<Thread> thread = this->getThread(it->second);
		if (!this->threads.at(it->second).isDeleted() && thread->userHasPermission(client_id, PERMISSION::VIEW_THREAD)) {
			nlohmann::json thread_json = thread->asJson();
			/*
			if (this->userHasPermission(client_id, PERMISSION::MANAGE_PERMISSIONS)) {
				std::cout << "client with ID " << client_id << "has manage_permissions" << std::endl;
				thread_json["client_permissions"]["manage_permissions"] = true;
			}
			thread_json["client_permissions"]["send_message"] = thread->userHasPermission(client_id, PERMISSION::SEND_MESSAGE);
			*/
			// TODO place in JSON client permission status for DELETE_POST
			multiple_thread_json["threads"].push_back(thread_json);
		}
	}
	return multiple_thread_json.dump();
}

std::string Board::dumpThread(int thread_id, int client_id, std::string key) const {
	nlohmann::json thread_json;
	thread_json["messages"] = this->threads.at(thread_id).getMessagesAsJson(key);
	thread_json["client_permissions"]["send_message"] = this->threads.at(thread_id).userHasPermission(client_id, PERMISSION::SEND_MESSAGE);
	return thread_json.dump();
}

std::string Board::dumpPermissionsInThread(int thread_id, int client_id) const {
	return this->threads.at(thread_id).dumpPermissions(client_id);
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

std::string Board::dumpPost(int thread_id, int post_id, std::string key) const {
	return this->threads.at(thread_id).dumpPost(post_id, key);
}

