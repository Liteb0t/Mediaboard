#include "post.hpp"
#include "db_interface.h"
#include "permission_managed_object.hpp"
#include <ctime>
#include <string>
#include <unordered_set>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class websocket_session;

class Thread : public PermissionManagedObject {
public:
	// Thread();
	Thread(PermissionObjectBase* permission_parent, json thread_json/*, bool save_to_database*/);
	Thread(PermissionObjectBase* permission_parent, struct db_thread_struct* thread_struct);
	std::string dumpThread() const;
	json asJson() const { return this->thread_as_json; };
	// int getNumberOfPosts() const { return this->posts.size(); };
	// void addInitialPost(json post_json, bool save_to_database);
	void createPostFromStruct(struct db_post_struct* post_struct);
	int createPostFromJson(json post_json);
	// int addPost(json post_json, int id_in_thread, bool save_to_database);
	// int addPost(json post_json, bool save_to_database);
	void deleteMessage(int message_id);
	bool keyMatchesMessage(std::string key, int message_id) const;
	bool messageExists(int message_id) const { std::map<int, Post>::const_iterator it = posts.find(message_id); return it != posts.end(); };
	int getId() const { return this->id; };
	void addListener(websocket_session* listener);
	void removeListener(websocket_session* listener);
	std::unordered_set<websocket_session*> getListeners() const { return this->listeners; };
	std::string dumpLastPost() const;
	std::string dumpPosts(std::string key) const;
	std::string dumpPost(int message_id, std::string key) const;
	void markAsDeleted();
	std::time_t getLastPostTime() const { return this->last_post_timestamp; }
	int number_of_posts;
	bool isDeleted() const { return this->deleted; }
private:
	int id;
	std::time_t last_post_timestamp;
	// std::vector<Post> posts;
	std::map<int, Post> posts;
	std::unordered_set<websocket_session*> listeners;
	// char subject[256];
	// char upload_timestamp[20];
	// char files[4][256]; // Max files is 4, maximum URL length is 255
	// short files_i;
	// std::string name;
	// std::string content;
	json thread_as_json;
	bool deleted;
};
