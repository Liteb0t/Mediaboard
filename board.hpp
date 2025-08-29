#include "thread.hpp"
#include <nlohmann/json.hpp>
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
	std::unordered_set<websocket_session*> getListenersFromThread(int thread_id) const { return this->threads.at(thread_id).getListeners(); };
	std::string dumpPost(int thread_id, int post_id) const;
	// std::string dumpReplies(int thread_id) const;
private:
	void cacheAllThreads();

	std::map<int, Thread> threads;
	int thread_limit;
	int post_limit;
};
