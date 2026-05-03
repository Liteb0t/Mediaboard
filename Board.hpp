#include "Thread.hpp"
#include <ctime>
#include <boost/json.hpp>
#include <set>
#include <utility>

class websocket_session; // Forward declaration

class Board : public PermissionManagedObject {
public:
	Board(PermissionObjectBase* permission_parent, FuzeDBI::Connection* fuze_dbi);
	void cacheAllThreads();
	int createThread(boost::json::object thread_json, int author_client_id);
	int createMessage(boost::json::object message_json, int author_client_id);
	void deleteThread(int thread_id);
	void deleteMessageFromThread(int message_id, int thread_id);
	// std::string dumpLastThread() const;
	// bool keyMatchesMessageInThread(std::string key, int message_id, int thread_id) const;
	std::string dumpAllThreads(const std::optional<FuzeHttp::Client>& client) const;
	// std::string dumpThread(int thread_id, int client_id, std::string key) const;
	// std::string dumpPermissionsInThread(int thread_id, int client_id) const;
	boost::json::object getThreadPermissionsAsJson(int thread_id, const std::optional<FuzeHttp::Client>& client) const;
	bool threadExists(int thread_id) const { std::unordered_map<int, Thread>::const_iterator it = threads.find(thread_id); return it != threads.end(); };
	bool messageExistsInThread(int message_id, int thread_id) const { return this->threads.at(thread_id).messageExists(message_id); }
	void addListenerToThread(websocket_session* listener, int thread_id);
	void removeListenerFromThread(websocket_session* listener, int thread_id);
	std::unordered_set<websocket_session*> getListenersFromThread(int thread_id) const { return this->threads.at(thread_id).getListeners(); };
	std::string dumpMessage(int thread_id, int message_id) const;
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
	const Thread* getThread(int thread_id) const { return &this->threads.at(thread_id); }
	void addGroupPermissionCollectionToThread(int group_id, int thread_id) { this->threads.at(thread_id).addGroupPermissionCollection(group_id); }
	void addAccountPermissionCollectionToThread(int account_id, int thread_id) { this->threads.at(thread_id).addAccountPermissionCollection(account_id); }
	void setGroupPermissionForThread(int group_id, PERMISSION permission, THREE_STATE_SETTING setting, int thread_id) { this->threads.at(thread_id).setGroupPermission(group_id, permission, setting); }
	void setAccountPermissionForThread(int account_id, PERMISSION permission, THREE_STATE_SETTING setting, int thread_id) { this->threads.at(thread_id).setAccountPermission(account_id, permission, setting); }
	void removeGroupPermissionCollectionFromThread(int group_id, int thread_id) { this->threads.at(thread_id).removeGroupPermissionCollection(group_id); }
	void removeAccountPermissionCollectionFromThread(int account_id, int thread_id) { this->threads.at(thread_id).removeAccountPermissionCollection(account_id); }
private:
	FuzeDBI::Connection* fuze_dbi;
	std::unordered_map<int, Thread> threads;
	std::set<std::pair<std::time_t, int>, thread_order_comparator> ordered_threads;
};
