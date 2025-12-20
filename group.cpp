#include "group.hpp"
#include <string>
#include <iostream>

Group::Group(struct db_group_struct* group_struct) {
	this->id = group_struct->id;
	this->name = group_struct->name;
	// this->rank = group_struct->rank;
	// strcpy(this->permissions, group_struct->permissions);
	
	this->group_as_json["id"] = this->id;
	this->group_as_json["name"] = this->name;
	// this->group_as_json["rank"] = this->rank;
	// this->group_as_json["permissions"] = this->permissions;
	
	// if (this->id == 0) {
	// 	this->lock_position = LOCK_TO_TOP;
	// 	this->group_as_json["lock_position"] = "top";
	// }
	// else if (this->id == 1 || this->id == 2) {
	// 	this->lock_position = LOCK_TO_BOTTOM;
	// 	this->group_as_json["lock_position"] = "bottom";
	// }
	// else {
	// 	this->lock_position = NONE;
	// 	this->group_as_json["lock_position"] = "none";
	// }
}

// Group::Group(json group_json) {
Group::Group(std::string name) : name(name) {
	nlohmann::json group_json;
	this->group_as_json = group_json;
	// this->name = group_json["name"].template get<std::string>();
	// this->rank = group_json["rank"].template get<int>();

	// strcpy(this->permissions, (group_json["permissions"].template get<std::string>()).c_str());
	this->group_as_json["name"] = this->name;
	this->id = db_store_group(this->name.c_str());
	this->group_as_json["id"] = this->id;
	std::cout << "[Group] Constructed with ID " << this->id << std::endl;
}
