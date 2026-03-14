#include <string>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include "db_interface.h"

using json = nlohmann::json;

enum GroupLockPosition { NONE, LOCK_TO_TOP, LOCK_TO_BOTTOM };

class Group {
public:
	// Group::Group(int id, std::string name)
	Group(struct db_group_struct* group_struct);
	// Group(json group_json);
	Group(std::string name);
	json asJson() const { return this->group_as_json; };
	int getId() const { return this->id; };
	GroupLockPosition getLockPosition() const { return this->lock_position; };
	void addMember(const int user_id) { this->members.insert(user_id); }
	void removeMember(int user_id) { this->members.erase(user_id); }
	bool containsMember(int user_id) const {
		// In C++20 this can be replaced with this->members.contains(user_id)
		std::unordered_set<int>::const_iterator member_iterator = this->members.find(user_id); 
		return member_iterator != this->members.end();
	}
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
private:
	int id;
	std::string name;
	std::unordered_set<int> members;
	json group_as_json;
	GroupLockPosition lock_position;
};
