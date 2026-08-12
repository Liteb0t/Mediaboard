module;
#include <ctime>
#include <boost/json.hpp>
#include <iostream>
#include <string>
export module Mediaboard.Message;

import FuzeDBI;
import FuzeHttp.Core;

export namespace Mediaboard {
enum class MESSAGE_FIELDS : size_t { MAX_NAME = 32, MAX_CONTENT = 5000, MAX_FILE_NAME = 205, MAX_FILE_NAME_WITH_UUID = 205+36 };

struct File {
	std::string filename;
	std::optional<int> width, height;
	std::optional<std::string> thumbnail_file_extension;
};

class Message {
public:
	// Cache message from database
	Message(int id, int thread_id, int id_in_thread, std::chrono::time_point<std::chrono::system_clock> created_at, int author_client_id,  std::string author_username, std::string content, std::vector<File> files, bool deleted = false)
			: id(id),
			thread_id(thread_id),
			id_in_thread(id_in_thread),
			created_at(created_at),
			author_client_id(author_client_id),
			author_username(author_username),
			content(content),
			files(files),
			deleted(deleted) {
		this->post_as_json = {
			{"id", id},
			{"thread_id", thread_id},
			{"id_in_thread", id_in_thread},
			{"created_at", std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count()},
			{"name", author_username},
			{"content", content}
		};
	}
	// Save message when JSON is received
	Message(boost::json::object post_json, int author_client_id, FuzeDBI::Connection* fuze_dbi) {
		if (!(post_json.contains("files") && post_json.contains("name") && post_json.contains("content"))) {
			throw std::runtime_error("Message JSON is missing one or more of the following entries: files, name, content");
		}
		// if ((message_content.length() == 0 && post_json["files"].size() == 0) || message_content.length() > static_cast<size_t>(MESSAGE_FIELDS::MAX_CONTENT))
		//	return api_response(http::status::bad_request, std::string("The post does not meet the constraints set by the server.\nThis could mean that the message content was empty and no files were uploaded, or the message content is too long."));
		this->post_as_json = post_json;
		this->post_as_json["type"] = "post";

		this->id_in_thread = post_json["id_in_thread"].as_uint64();
		this->thread_id = post_json["thread_id"].as_int64();
		this->author_client_id = author_client_id;
		// User input checking is done on front-end, so it's not high priority to return an http error when username or content is empty/too long.
		this->author_username = post_json["name"].as_string();
		if (this->author_username == "") {
			this->author_username = "Anonymous"; // Blank username becoming Anonymous is intended behaviour
			this->post_as_json["name"] = this->author_username;
		}
		else if (this->author_username.length() > static_cast<size_t>(MESSAGE_FIELDS::MAX_NAME)) {
			this->author_username = "Bad Username";
			this->post_as_json["name"] = this->author_username;
		}
		this->content = post_json["content"].as_string();
		if (this->content.length() > static_cast<size_t>(MESSAGE_FIELDS::MAX_CONTENT)) {
			throw std::runtime_error(std::format("Message content length {} exceeds the limit of {}", this->content.length(), static_cast<size_t>(MESSAGE_FIELDS::MAX_CONTENT)));
		}
		this->created_at = std::chrono::system_clock::now();
		this->post_as_json["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count();

		this->id = fuze_dbi->query<int>("SELECT message_id FROM _sequences");
		fuze_dbi->query<void>("UPDATE _sequences SET message_id = $1", this->id + 1);
		fuze_dbi->query<void>("INSERT INTO message(id, thread_id, id_in_thread, author_client_id, author_username, created_at, content) VALUES ($1, $2, $3, $4, $5, $6, $7)", this->id, this->thread_id, this->id_in_thread, author_client_id, this->author_username, (int)std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count(), this->content);
		boost::json::array files_json = post_json["files"].as_array();
		if (this->content.length() == 0 && files_json.size() == 0) {
			throw std::runtime_error("Message Cannot be empty");
		}
		this->files_i = 0;
		for (boost::json::value file_val : files_json) {
			boost::json::object& file_obj = file_val.as_object();
			const boost::json::string filename = file_val.at("filename").as_string();
			if (filename.size() <= static_cast<size_t>(MESSAGE_FIELDS::MAX_FILE_NAME_WITH_UUID)) {
				File file{.filename = filename.c_str()}; // TODO sanitise filename. Frontend handles this but not if the API is used directly
				if (file_obj.contains("width") && file_obj.contains("height") && file_obj.contains("thumbnail_file_extension")) {
					file.width = file_obj.at("width").as_int64();
					file.height = file_obj.at("height").as_int64();
					file.thumbnail_file_extension = file_obj.at("thumbnail_file_extension").as_string();
					fuze_dbi->query<void>("INSERT INTO message_file(message_id, file_name, width, height, thumbnail_file_extension) VALUES ($1, $2, $3, $4, $5)", this->id, filename.c_str(), file.width.value(), file.height.value(), file.thumbnail_file_extension.value());
				}
				else
					fuze_dbi->query<void>("INSERT INTO message_file(message_id, file_name) VALUES ($1, $2)", this->id, filename.c_str());
				this->files.push_back(file);
			}
			else
				std::cerr << "File name too long to save to database. Length: " << filename.size() << std::endl;
			if (++files_i >= 4)
				break;
		}
		post_as_json.erase("files");
		this->post_as_json["id"] = this->id;

		// New posts are not in a deleted state
		this->deleted = false;
		// Moderators will be able to view deleted messages
		// this->post_as_json["deleted"] = false;
	}

	std::string dump() const {
		return boost::json::serialize(this->post_as_json);
	}
	boost::json::object asJson() const {
		boost::json::object message_as_json = this->post_as_json;
		boost::json::array files_json;
		for (File file : this->files) {
			boost::json::object file_json = {{"filename", file.filename}};
			if (file.width) file_json.emplace("width", file.width.value());
			if (file.height) file_json.emplace("height", file.height.value());
			if (file.thumbnail_file_extension) file_json.emplace("thumbnail_file_extension", file.thumbnail_file_extension.value());
			files_json.emplace_back(file_json);
		}
		message_as_json.emplace("files", files_json);
		return message_as_json;
	}
	int getId() const { return this->id; };
	int getIdInThread() const { return this->id_in_thread; };
	// std::string getKey() const { return this->key; }
	std::chrono::time_point<std::chrono::system_clock> createdAt() const { return this->created_at; }
	void createFromJSON(boost::json::object post_json);
	void markAsDeleted() {
		this->deleted = true;
	}
	bool isDeleted() const { return this->deleted; }
	bool clientIsAuthor(const FuzeHttp::Client& client) const { return client.id == this->author_client_id; }
private:
	int id;
	int thread_id;
	int id_in_thread;
	// char upload_timestamp[20];
	std::vector<File> files;
	short files_i;
	int author_client_id;
	std::string author_username;
	std::string content;
	boost::json::object post_as_json;
	std::chrono::time_point<std::chrono::system_clock> created_at;
	// std::string key;
	bool deleted;
}; // class Message
} // namespace Mediaboard
