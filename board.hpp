#include "thread.hpp"
#include <ctime>
#include <nlohmann/json.hpp>
#include <set>
#include <utility>
#include <vector>

using json = nlohmann::json;

class websocket_session; // Forward declaration

class Board {
public:
	Board();
	int createThread(json thread_json);
	int createPost(json post_json);
	void deleteThread(int thread_id);
	void deleteMessageFromThread(int message_id, int thread_id);
	// std::string dumpLastThread() const;
	bool keyMatchesMessageInThread(std::string key, int message_id, int thread_id) const;
	std::string dumpAllThreads() const;
	std::string dumpPostsInThread(int thread_id, std::string key) const;
	bool threadExists(int thread_id) const { std::map<int, Thread>::const_iterator it = threads.find(thread_id); return it != threads.end(); };
	bool messageExistsInThread(int message_id, int thread_id) const { return this->threads.at(thread_id).messageExists(message_id); }
	void addListenerToThread(websocket_session* listener, int thread_id);
	void removeListenerFromThread(websocket_session* listener, int thread_id);
	std::unordered_set<websocket_session*> getListenersFromThread(int thread_id) const { return this->threads.at(thread_id).getListeners(); };
	std::string dumpPost(int thread_id, int post_id, std::string key) const;
	struct thread_order_comparator {
		bool operator() (std::pair<std::time_t, int> left, std::pair<std::time_t, int> right) const {
			if (left.first > right.first) {
				return true;
			}
			else if (left.first < right.first) {
				return false;
			}
			if (left.second > right.second) {
				return true;
			}
			else if (left.second < right.second) {
				return false;
			}
			else {
				return false;
			}
		}
	};
private:
	void cacheAllThreads();

	std::map<int, Thread> threads;
	// std::vector<int> ordered_threads; // O(N) access time - room for optimisation
	std::set<std::pair<std::time_t, int>, thread_order_comparator> ordered_threads;
	int thread_limit;
	int post_limit;
};
