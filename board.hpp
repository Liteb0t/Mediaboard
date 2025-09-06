#include "thread.hpp"
#include <ctime>
#include <nlohmann/json.hpp>
#include <set>
#include <tuple>
#include <vector>

using json = nlohmann::json;

class websocket_session; // Forward declaration

class Board {
public:
	Board();
	void createThread(json thread_json);
	int createPost(json post_json);
	// std::string dumpLastThread() const;
	std::string dumpAllThreads() const;
	std::string dumpPostsInThread(int thread_id) const;
	bool threadExists(int thread_id) const { std::map<int, Thread>::const_iterator it = threads.find(thread_id); return it != threads.end(); };
	void addListenerToThread(websocket_session* listener, int thread_id);
	void removeListenerFromThread(websocket_session* listener, int thread_id);
	std::unordered_set<websocket_session*> getListenersFromThread(int thread_id) const { return this->threads.at(thread_id).getListeners(); };
	std::string dumpPost(int thread_id, int post_id) const;
	struct thread_order_comparator {
		bool operator() (std::tuple<std::time_t, int> left, std::tuple<std::time_t, int> right) const;
	};
private:
	void cacheAllThreads();

	std::map<int, Thread> threads;
	// std::vector<int> ordered_threads; // O(N) access time - room for optimisation
	std::set<std::tuple<std::time_t, int>, thread_order_comparator> ordered_threads;
	int thread_limit;
	int post_limit;
};
