// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <ctime>
#include <boost/json.hpp>
#include <expected>
#include <iostream>
#include <print>
#include <regex>
#include <string>
export module Mediaboard.Message;

import FuzeDBI;
import FuzeHttp.Core;
import Mediaboard.File;

export namespace Mediaboard {
class Message {
public:
	// Cache message from database
	Message(int id, int thread_id, int id_in_thread, std::chrono::time_point<std::chrono::system_clock> created_at, int author_client_id,  std::string author_username, std::optional<std::string> highest_ranked_group_name, std::string content, bool deleted = false)
			: id(id),
			thread_id(thread_id),
			id_in_thread(id_in_thread),
			created_at(created_at),
			author_client_id(author_client_id),
			author_username(author_username),
			highest_ranked_group_name(highest_ranked_group_name),
			content(content),
			// files(files),
			deleted(deleted) {
		// cacheInternalJSON();
	}

	inline static const size_t MAX_NAME = 32;
	inline static const size_t MAX_CONTENT = 5000;
	inline static const size_t MAX_NUMBER_OF_FILES = 4;
	// Save message when JSON is received
	struct Validated {
		int thread_id;
		std::optional<std::string> highest_ranked_group_name;
		// int id_in_thread;
		std::string name;
		std::string content;
		std::vector<File*> files;
	};
	Message(FuzeDBI::Connection* db, Validated input, int author_client_id, int thread_id, int id_in_thread, int board_id)
			: id(db->incrementSequence("message_id")),
			author_client_id(author_client_id),
			author_username(input.name.length() == 0 ? "Anonymous" : input.name),
			highest_ranked_group_name(input.highest_ranked_group_name),
			content(input.content),
			thread_id(thread_id),
			id_in_thread(id_in_thread),
			files(input.files),
			created_at(std::chrono::system_clock::now()),
			deleted(false) {
		cacheInternalJSON();
		int time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count();
		// save shit to database
		db->query<void>("INSERT INTO message(id, thread_id, id_in_thread, author_client_id, author_username, created_at, content) VALUES ($1, $2, $3, $4, $5, $6, $7)", this->id, this->thread_id, this->id_in_thread, author_client_id, this->author_username, time_since_epoch, this->content);
		if (highest_ranked_group_name) {// this is like ## JANNY after the name
			db->query<void>("UPDATE message SET highest_ranked_group_name = $1 WHERE id = $2",  highest_ranked_group_name.value(), id);
		}
		this->files_i = 0;
		for (File* file : input.files) {
			file->coupleToPost(board_id, this->id);
		}
	}
	boost::json::object asJson(const std::optional<FuzeHttp::Client>& client) const {
		boost::json::object message_as_json = post_as_json ? post_as_json.value() : createJSON();
		message_as_json["is_author"] = client && client.value().id == author_client_id;
		boost::json::array files_json;
		for (File* file : this->files) {
			files_json.emplace_back(file->asJson());
		}
		message_as_json.emplace("files", files_json);
		return message_as_json;
	}
	void addFile(File* file) { this->files.push_back(file); }
	int getId() const { return this->id; };
	int getIdInThread() const { return this->id_in_thread; };
	// std::string getKey() const { return this->key; }
	std::chrono::time_point<std::chrono::system_clock> createdAt() const { return this->created_at; }
	void markAsDeleted() {
		this->deleted = true;
		for (auto& file : this->files)
			file->markAsDeleted();
		uncacheInternalJSON();
	}
	bool isDeleted() const { return this->deleted; }
	bool clientIsAuthor(const FuzeHttp::Client& client) const { return client.id == this->author_client_id; }
private:
	boost::json::object createJSON() const {
		boost::json::object message_as_json;
		int time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count();
		std::optional<std::string> processed_message_content = createProcessedMessageContent(content);
		message_as_json = {
			{"type", "post"},
			{"id", id},
			{"thread_id", thread_id},
			{"id_in_thread", id_in_thread},
			{"name", author_username},
			{"created_at", time_since_epoch},
			{"content", processed_message_content ? processed_message_content.value() : content},
			{"contains_html", processed_message_content ? true : false}
		};
		if (highest_ranked_group_name) // this is like ## JANNY after the name
			message_as_json.emplace("highest_ranked_group_name", highest_ranked_group_name.value());
		return message_as_json;
	}
	void cacheInternalJSON() {
		this->post_as_json = createJSON();
	}
	void uncacheInternalJSON() {
		this->post_as_json = {};
	}
	// https://stackoverflow.com/a/5665377/18658154
	static std::string escapeHTML(const std::string& data) {
		std::string buffer;
		buffer.reserve(data.size());
		for(size_t pos = 0; pos != data.size(); ++pos) {
			switch(data[pos]) {
				case '&':  buffer += "&amp;";       break;
				case '\"': buffer += "&quot;";      break;
				case '\'': buffer += "&apos;";      break;
				case '<':  buffer += "&lt;";        break;
				case '>':  buffer += "&gt;";        break;
				default:   buffer += data[pos];  break;
			}
		}
		std::println("[escapeHTML] returning {}", buffer);
		return buffer;
	}
	static std::optional<std::string> createProcessedMessageContent(const std::string& raw_content) {
		std::string result;
		size_t number_of_matches = 0;
		std::string content = escapeHTML(raw_content);
		std::regex url_regex(R"(https?://[^\s<]+)");
		auto urls_begin = std::sregex_iterator(content.begin(), content.end(), url_regex);
		auto urls_end = std::sregex_iterator();
		std::size_t last_match_index = 0;
		if (std::distance(urls_begin, urls_end) == 0)
			return {};
		for (std::sregex_iterator i = urls_begin; i != urls_end; ++i, ++number_of_matches) {
			std::smatch match = *i;
			result += content.substr(last_match_index, match.position() - last_match_index);
			if (std::optional<std::string> embed = createEmbedIfMatchExistsForURL(match.str()))
				result += createAnchorElementFromURL(match.str()) + embed.value();
			else
				result += createAnchorElementFromURL(match.str()); // <a href=whatever>
			last_match_index = match.position() + match.length();
		}
		result += content.substr(last_match_index);
		// std::println("Found {} URLs", number_of_matches);
		return result;
	}
	static std::string createAnchorElementFromURL(const std::string& url) {
		return std::format(R"-(<a href="{}" target="_blank" rel="noopener nofollow">{}</a>)-", url, url);
	}
	static std::optional<std::string> createEmbedIfMatchExistsForURL(const std::string& url) {
		std::smatch match;
		for (auto& rule : url_embed_rules) {
			if (std::regex_search(url, match, rule.regex))
				return rule.process(match);
		}
		return {};
	}
	struct RegexProcessorSlot {
		std::regex regex;
		std::function<std::optional<std::string>(const std::smatch&)> process;
	};
	inline static const std::vector<RegexProcessorSlot> url_embed_rules = {
		{std::regex(R"-(\w+:\/\/(?:(?:www\.|old\.)?vocaroo\.com|voca\.ro)\/((?:i\/)?\w+))-"), [](const std::smatch& source)->std::optional<std::string>{
			const std::string id = source[1];
			if (!std::all_of(id.begin(), id.end(), [](char c){ return std::isalnum((unsigned char)c); }))
				return {};
			else {
				return std::format(R"-(<br><figure data-iframe-src="https://vocaroo.com/embed/{}?autoplay=0" class="EmbedFigure"><figcaption class="EmbedIframe">Embed Vocaroo</figcaption><iframe style="display: none" width="300" height="60" frameborder="0" src=""></iframe></figure>)-", id);
			}
		}},
		{std::regex(R"-(\w+:\/\/(?:youtu.be\/|[\w.]*youtube[\w.]*\/.*(?:v=|\bembed\/|\bv\/|live\/|shorts\/))([\w\-]{11})([\w&=?]*)\b)-"), [](const std::smatch& source)->std::optional<std::string>{
			const std::string id = source[1];
			if (!std::all_of(id.begin(), id.end(), [](char c){ return std::isalnum((unsigned char)c) || c == '_' || c == '-'; }))
				return {};
			const std::string parameters = source[2];
			std::smatch start_match;
			if (std::regex_search(parameters, start_match, std::regex(R"-("\b(?:star)?t\=(\d+))-"))) {
				std::println("start_match: {}", start_match.str()); // from which to get video start time
			}
			return std::format(R"-(<br><figure data-iframe-src="//www.youtube.com/embed/{}?rel=0&wmode=opaque{}" class="EmbedFigure"><figcaption class="EmbedIframe">Embed YouTube video</figcaption><iframe style="display: none" allowfullscreen="true" width="360" height="240" src=""></iframe></figure>)-", id, start_match.empty() ? "" : std::format("&start={}", start_match.str()));
		}}
	};
	// inline static const std::vector<RegexProcessorSlot> general_embed_rules = {
	// 	{std::regex(R"-(#\d*\/(\d+)\b)-"}, //TODO add reply link creation
	// };
	int id;
	int thread_id;
	int id_in_thread;
	// char upload_timestamp[20];
	std::vector<File*> files;
	short files_i;
	int author_client_id;
	std::string author_username;
	std::optional<std::string> highest_ranked_group_name;
	std::string content;
	std::optional<boost::json::object> post_as_json;
	std::chrono::time_point<std::chrono::system_clock> created_at;
	// std::string key;
	bool deleted;
}; // class Message
} // namespace Mediaboard
