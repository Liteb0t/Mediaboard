#include <string>
#include <ctime>
#include <boost/json.hpp>
#include "FuzeDBI.hpp"
#include "FuzeHttp.hpp"

enum class MESSAGE_FIELDS : size_t { MAX_NAME = 32, MAX_CONTENT = 5000, MAX_FILE_NAME = 205, MAX_FILE_NAME_WITH_UUID = 205+36 };

class Message {
public:
	Message(int id, int thread_id, int id_in_thread, std::chrono::time_point<std::chrono::system_clock> created_at, int author_client_id,  std::string author_username, std::string content, std::vector<std::string> files, bool deleted = false);
	Message(boost::json::object post_json, int author_client_id, FuzeDBI::Connection* fuze_dbi);
	std::string dump() const;
	boost::json::object asJson() const { return this->post_as_json; };
	int getId() const { return this->id; };
	int getIdInThread() const { return this->id_in_thread; };
	// std::string getKey() const { return this->key; }
	std::chrono::time_point<std::chrono::system_clock> createdAt() const { return this->created_at; }
	void createFromJSON(boost::json::object post_json);
	void markAsDeleted();
	bool isDeleted() const { return this->deleted; }
	bool clientIsAuthor(const Client& client) const { return client.id == this->author_client_id; }
private:
	int id;
	int thread_id;
	int id_in_thread;
	// char upload_timestamp[20];
	std::vector<std::string> files;
	short files_i;
	int author_client_id;
	std::string author_username;
	std::string content;
	boost::json::object post_as_json;
	std::chrono::time_point<std::chrono::system_clock> created_at;
	// std::string key;
	bool deleted;
};
