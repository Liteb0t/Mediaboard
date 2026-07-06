#include "Board.hpp"
// #include "db_interface.h"
#include <boost/json/serialize.hpp>
#include <sstream>
#include <print>
#include <iostream>

Board::Board(PermissionObjectBase* permission_parent, FuzeDBI::Connection* fuze_dbi)
		: PermissionManagedObject(permission_parent, 1, fuze_dbi),
		fuze_dbi(fuze_dbi) {
}

int Board::createThread(boost::json::object thread_json, int author_client_id) {
	Thread thread(this, thread_json, author_client_id, fuze_dbi);
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
	else
		throw std::runtime_error("Can't delete message 0 from thread");
}

void Board::cacheAllThreads() {
	std::cout << "[Board] Retrieving threads from database..." << std::endl;
	for (auto thread_tuple : fuze_dbi->queryRows<std::tuple<int, int>>("SELECT id, permission_object_id FROM thread WHERE deleted = FALSE")) {
		Thread thread(this, fuze_dbi, std::get<0>(thread_tuple), std::get<1>(thread_tuple));
		std::cout << thread.getId() << ", ";
		this->threads.insert(std::make_pair(thread.getId(), thread));
	}
	std::cout << "done." << std::endl;

	std::cout << "[Board] Retrieving messages from database..." << std::endl;
	for (auto message_tuple : fuze_dbi->queryRows<std::tuple<int, int, int, int, int, std::string, std::string>>("SELECT id, thread_id, id_in_thread, created_at, author_client_id, author_username, content FROM message WHERE deleted = FALSE")) {
		int message_id = std::get<0>(message_tuple);
		int thread_id = std::get<1>(message_tuple);
		int id_in_thread = std::get<2>(message_tuple);
		std::print("#{}/{}", thread_id, id_in_thread);
		int seconds_since_epoch = std::get<3>(message_tuple); // TODO use long instead of int
		std::chrono::seconds sec(seconds_since_epoch);
		std::chrono::time_point<std::chrono::system_clock> created_at(sec);
		std::vector<File> message_files;
		for (auto file_tuple : fuze_dbi->queryRows<std::tuple<std::string, std::optional<int>, std::optional<int>, std::optional<std::string>>>("SELECT file_name, width, height, thumbnail_file_extension FROM message_file WHERE message_id = $1", message_id)) {
			message_files.push_back(File{
				.filename = std::get<0>(file_tuple),
				.width = std::get<1>(file_tuple),
				.height = std::get<2>(file_tuple),
				.thumbnail_file_extension = std::get<3>(file_tuple)
			});
		}
		Message message(message_id, thread_id, id_in_thread, created_at, std::get<4>(message_tuple), std::get<5>(message_tuple), std::get<6>(message_tuple), message_files);
		this->threads.at(thread_id).cacheMessage(std::move(message));
		std::print(", ");
	}
	std::cout << "done." << std::endl;

	// Mark invalid threads as deleted. Sometimes when an error occurs whilst creating a thread, it saves the thread to the DB but not its corresponding message.
	for (auto& thread_pair : this->threads) {
		if (!thread_pair.second.messageExists(0))
			thread_pair.second.markAsDeleted();
	}
	
	// Sort threads by most recent message date
	for (std::unordered_map<int, Thread>::const_iterator it = this->threads.begin(); it != this->threads.end(); ++it) {
		this->ordered_threads.insert(std::make_pair(std::chrono::duration_cast<std::chrono::seconds>(it->second.getLastMessageTime().time_since_epoch()).count(), it->first));
	}

	std::cout << "[Board] Finished retreiving threads and posts from the database." << std::endl;
}

boost::json::object Board::getThreadPermissionsAsJson(int thread_id, const std::optional<Client>& client) const {
	return this->threads.at(thread_id).getPermissionsAsJson(client);
}

std::string Board::dumpAllThreads(const std::optional<Client>& client) const {
	boost::json::array threads_json = boost::json::array();
	for (std::set<std::pair<std::time_t, int>>::const_iterator it = this->ordered_threads.begin(); it != this->ordered_threads.end(); ++it) {
		// boost::shared_ptr<Thread> thread = this->getThread(it->second);
		if (!this->threads.at(it->second).isDeleted() && this->threads.at(it->second).clientHasPermission(client, PERMISSION::VIEW_THREAD)) {
			boost::json::object thread_json = this->threads.at(it->second).asJson(client);
			threads_json.emplace_back(thread_json);
		}
	}
	std::println("[Board] Finished assembling threads list into JSON");
	return boost::json::serialize(boost::json::value{
		{"type", "thread_catalog"},
		{"threads", threads_json}
	});
}

void Board::addListenerToThread(WebsocketSession* listener, int thread_id) {
	if (threadExists(thread_id)) {
		this->threads.at(thread_id).addListener(listener);
		std::cout << "[Board] Listener added to thread " << thread_id << std::endl;
	}
	else
		std::cout << "[Board] Warning: could not add listener to thread " << thread_id << " because the thread does not exist." << std::endl;
}

void Board::removeListenerFromThread(WebsocketSession* listener, int thread_id) {
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
