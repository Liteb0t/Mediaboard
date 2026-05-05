#include <string>
#include <unordered_set>
#include <boost/json.hpp>

enum GroupLockPosition { NONE, LOCK_TO_TOP, LOCK_TO_BOTTOM };

class Group {
public:
	Group(int id, std::string name);
	boost::json::object asJson() const { return this->group_as_json; };
	int getId() const { return this->id; };
	GroupLockPosition getLockPosition() const { return this->lock_position; };
	void addMember(const int user_id) { this->members.insert(user_id); }
	void removeMember(int user_id) { this->members.erase(user_id); }
	bool containsMember(int user_id) const { return this->members.contains(user_id); }
	/*
	std::string dumpMembers() const {
		nlohmann::json members_json;
		members_json["members"] = nlohmann::json::array();
		for (int member_id : this->members) {
			members_json.push_back(member_id);
		return members_json.dump();
	}*/
	const std::unordered_set<int> getMembers() const {
		return this->members;
	}
	std::string getName() const { return this->name; }
	inline static const size_t MAX_NAME = 32;
private:
	int id;
	std::string name;
	std::unordered_set<int> members;
	boost::json::object group_as_json;
	GroupLockPosition lock_position;
};
