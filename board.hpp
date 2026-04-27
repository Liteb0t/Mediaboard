#include "thread.hpp"
#include <ctime>
#include <nlohmann/json.hpp>
#include <set>
#include <utility>

using json = nlohmann::json;

class websocket_session; // Forward declaration

class Board : public PermissionManagedObject {
public:
	Board(PermissionObjectBase* permission_parent, FuzeDBI::Connection* fuze_dbi);
	int createThread(json thread_json);
	int createPost(json post_json);
	void deleteThread(int thread_id);
	void deleteMessageFromThread(int message_id, int thread_id);
	// std::string dumpLastThread() const;
	// bool keyMatchesMessageInThread(std::string key, int message_id, int thread_id) const;
	std::string dumpAllThreads(const Client& client) const;
	// std::string dumpThread(int thread_id, int client_id, std::string key) const;
	// std::string dumpPermissionsInThread(int thread_id, int client_id) const;
	bool threadExists(int thread_id) const { std::unordered_map<int, Thread>::const_iterator it = threads.find(thread_id); return it != threads.end(); };
	bool messageExistsInThread(int message_id, int thread_id) const { return this->threads.at(thread_id).messageExists(message_id); }
	void addListenerToThread(websocket_session* listener, int thread_id);
	void removeListenerFromThread(websocket_session* listener, int thread_id);
	std::unordered_set<websocket_session*> getListenersFromThread(int thread_id) const { return this->threads.at(thread_id).getListeners(); };
	std::string dumpPost(int thread_id, int post_id, std::string key) const;
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
	// void cacheAllThreads();
	// boost::shared_ptr<Thread> getThread(int thread_id) const { return boost::make_shared<Thread>(this->threads.at(thread_id)); }
	void addGroupPermissionCollectionToThread(int group_id, int thread_id) { this->threads.at(thread_id).addGroupPermissionCollection(group_id); }
	void addAccountPermissionCollectionToThread(int account_id, int thread_id) { this->threads.at(thread_id).addAccountPermissionCollection(account_id); }
	void setGroupPermissionForThread(int group_id, PERMISSION permission, THREE_STATE_SETTING setting, int thread_id) { this->threads.at(thread_id).setGroupPermission(group_id, permission, setting); }
	void setAccountPermissionForThread(int account_id, PERMISSION permission, THREE_STATE_SETTING setting, int thread_id) { this->threads.at(thread_id).setAccountPermission(account_id, permission, setting); }
	void removeGroupPermissionCollectionFromThread(int group_id, int thread_id) { this->threads.at(thread_id).removeGroupPermissionCollection(group_id); }
	void removeAccountPermissionCollectionFromThread(int account_id, int thread_id) { this->threads.at(thread_id).removeAccountPermissionCollection(account_id); }
private:
	std::unordered_map<int, Thread> threads;
	std::set<std::pair<std::time_t, int>, thread_order_comparator> ordered_threads;
};
