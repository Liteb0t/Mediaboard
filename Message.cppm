module;
#include <ctime>
#include <boost/json.hpp>
#include <expected>
#include <iostream>
#include <string>
export module Mediaboard.Message;

import FuzeDBI;
import FuzeHttp.Core;

export namespace Mediaboard {
class File {
public:
	static std::expected<File, std::string> validateInput(const boost::json::object json) {
		File file;
		if (auto filename_it = json.find("filename"); filename_it == json.end())
			return std::unexpected("Missing JSON field: filename");
		else if (!filename_it->value().is_string())
			return std::unexpected("JSON field 'filename' must be an string");
		else {
			file.filename = filename_it->value().as_string();
			if (file.filename.size() < 1 || file.filename.size() > static_cast<size_t>(MAX_FILE_NAME_WITH_UUID))
				return std::unexpected(std::format("Filename length {} is not between 1 and {}", file.filename.length(), MAX_FILE_NAME_WITH_UUID));
		}

		if (json.contains("width") || json.contains("height") || json.contains("thumbnail_file_extension")) {
			if (auto width_it = json.find("width"); width_it == json.end())
				return std::unexpected("Missing JSON field: width");
			else if (!width_it->value().is_int64())
				return std::unexpected("JSON field 'width' must be an int");
			else
				file.width = width_it->value().as_int64();
			if (auto height_it = json.find("height"); height_it == json.end())
				return std::unexpected("Missing JSON field: height");
			else if (!height_it->value().is_int64())
				return std::unexpected("JSON field 'height' must be an int");
			else
				file.height = height_it->value().as_int64();

			if (auto thumbnail_file_extension_it = json.find("thumbnail_file_extension"); thumbnail_file_extension_it == json.end())
				return std::unexpected("Missing JSON field: thumbnail_file_extension");
			else if (!thumbnail_file_extension_it->value().is_string())
				return std::unexpected("JSON field 'thumbnail_file_extension' must be an string");
			else {
				file.thumbnail_file_extension = thumbnail_file_extension_it->value().as_string();
				// TODO check if file extension is in supported formats
				// TODO sanitise filename. Frontend handles this but not if the API is used directly
				if (file.thumbnail_file_extension->length() < 1 || file.thumbnail_file_extension->length() > MAX_FILE_NAME)
					return std::unexpected(std::format("thumbnail_file_extension length {} is not between 1 and {}", file.thumbnail_file_extension->length(), MAX_FILE_NAME));
			}
		}
		return file;
	}
	inline static const size_t MAX_FILE_NAME = 205;
	inline static const size_t MAX_FILE_NAME_WITH_UUID = 205+36;
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

	inline static const size_t MAX_NAME = 32;
	inline static const size_t MAX_CONTENT = 5000;
	inline static const size_t MAX_NUMBER_OF_FILES = 4;
	// Save message when JSON is received
	struct Validated {
		int thread_id;
		// int id_in_thread;
		std::string name;
		std::string content;
		std::vector<File> files;
	};
	static std::expected<Message::Validated, std::string> validateInput(const boost::json::object json) {
		Validated validated;
		if (auto name_it = json.find("name"); name_it == json.end())
			return std::unexpected("Missing JSON field: name");
		else if (!name_it->value().is_string())
			return std::unexpected("JSON field 'name' must be an string");
		else {
			validated.name = name_it->value().as_string();
			if (validated.name.length() > Message::MAX_NAME)
				return std::unexpected(std::format("Name length {} must be less than {}", validated.name.length(), Message::MAX_NAME));
		}

		size_t number_of_files;
		if (auto files_it = json.find("files"); files_it == json.end())
			return std::unexpected("Missing JSON field: files");
		else if (!files_it->value().is_array())
			return std::unexpected("JSON field 'files' must be an array");
		else {
			boost::json::array files_json = files_it->value().as_array();
			number_of_files = files_json.size();
			if (number_of_files > MAX_NUMBER_OF_FILES)
				return std::unexpected(std::format("Cannot attach more than {} files", MAX_NUMBER_OF_FILES));
			for (boost::json::value file_val : files_json) {
				if (!file_val.is_object())
					return std::unexpected("file_val must be an object");
				boost::json::object& file_obj = file_val.as_object();
				auto file_maybe = File::validateInput(file_obj);
				if (!file_maybe)
					return std::unexpected(file_maybe.error());
				validated.files.push_back(file_maybe.value());
			}
		}

		if (auto content_it = json.find("content"); content_it == json.end())
			return std::unexpected("Missing JSON field: content");
		else if (!content_it->value().is_string())
			return std::unexpected("JSON field 'content' must be an string");
		else {
			validated.content = content_it->value().as_string();
			if ((validated.content.length() < 1 && number_of_files == 0) || validated.content.length() > MAX_CONTENT)
				return std::unexpected(std::format("Content length {} is not between 1 and {}", validated.content.length(), MAX_CONTENT));
		}

		return validated;
	}
	Message(FuzeDBI::Connection* db, Validated input, int author_client_id, int thread_id, int id_in_thread)
			: id(db->incrementSequence("message_id")),
			author_client_id(author_client_id),
			author_username(input.name.length() == 0 ? "Anonymous" : input.name),
			content(input.content),
			thread_id(thread_id),
			id_in_thread(id_in_thread),
			files(input.files),
			created_at(std::chrono::system_clock::now()),
			deleted(false) {
		int time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count();
		this->post_as_json = {
			{"type", "post"},
			{"id", id},
			{"thread_id", thread_id},
			{"id_in_thread", id_in_thread},
			{"name", author_username},
			{"created_at", time_since_epoch},
			{"content", content}
		};
		// save shit to database
		db->query<void>("INSERT INTO message(id, thread_id, id_in_thread, author_client_id, author_username, created_at, content) VALUES ($1, $2, $3, $4, $5, $6, $7)", this->id, this->thread_id, this->id_in_thread, author_client_id, this->author_username, time_since_epoch, this->content);
		this->files_i = 0;
		for (const File& file : files) {
			if (file.width && file.height && file.thumbnail_file_extension) {
				db->query<void>("INSERT INTO message_file(message_id, file_name, width, height, thumbnail_file_extension) VALUES ($1, $2, $3, $4, $5)", this->id, file.filename.c_str(), file.width.value(), file.height.value(), file.thumbnail_file_extension.value());
			}
			else
				db->query<void>("INSERT INTO message_file(message_id, file_name) VALUES ($1, $2)", this->id, file.filename.c_str());
		}
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
