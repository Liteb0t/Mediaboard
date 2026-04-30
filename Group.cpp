#include "Group.hpp"
#include <string>
#include <iostream>

// Group::Group(json group_json) {
Group::Group(int id, std::string name)
		: id(id),
		name(name) {
	this->group_as_json["id"] = this->id;
	this->group_as_json["name"] = this->name;
	std::cout << "[Group] Constructed with ID " << this->id << std::endl;
}
