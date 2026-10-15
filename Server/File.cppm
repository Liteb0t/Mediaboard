module;
#include <ctime>
#include <boost/json.hpp>
#include <expected>
#include <print>
#include <string>
export module Mediaboard.File;

import FuzeDBI;
import FuzeHttp.PermissionObject;

export namespace Mediaboard {
class File : public FuzeHttp::PermissionManagedObject {
public:
	struct Validated {
		int id;
		std::string filename;
		std::optional<int> width, height;
		std::optional<std::string> thumbnail_file_extension;
	};
	File(PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, /*std::optional<int> coupled_post_id,*/ std::string filename, std::optional<int> width, std::optional<int> height, std::optional<std::string> thumbnail_file_extension)
			: PermissionManagedObject(permission_parent, db),
			filename(filename),
			id(db->incrementSequence("stored_file_id")),
			created_at(std::chrono::system_clock::now()),
			// coupled_post_id(coupled_post_id),
			width(width),
			height(height),
			thumbnail_file_extension(thumbnail_file_extension) {
		int time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count();
		// TODO allow std::optional args to FuzeDBI::query
		if (width && height && thumbnail_file_extension) {
			db->query<void>("INSERT INTO stored_file(id, permission_object_id, filename, created_at, width, height, thumbnail_file_extension) VALUES ($1, $2, $3, $4, $5, $6, $7)", this->id, this->getPermissionObjectId(), filename.c_str(), time_since_epoch, width.value(), height.value(), thumbnail_file_extension.value());
		}
		else {
			db->query<void>("INSERT INTO stored_file(id, permission_object_id, filename, created_at) VALUES ($1, $2, $3, $4)", this->id, this->getPermissionObjectId(), filename.c_str(), time_since_epoch);
		}
	}
	File(PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, int id, int permission_object_id, std::string filename, std::chrono::time_point<std::chrono::system_clock> created_at, std::optional<int> coupled_post_id, std::optional<int> width, std::optional<int> height, std::optional<std::string> thumbnail_file_extension)
			: PermissionManagedObject(permission_parent, permission_object_id, db),
			id(id),
			filename(filename),
			created_at(created_at),
			coupled_post_id(coupled_post_id),
			width(width),
			height(height),
			thumbnail_file_extension(thumbnail_file_extension) {
	}
	boost::json::object asJson() const {
		boost::json::object file_json = {{"filename", filename}};
		if (width) file_json.emplace("width", width.value());
		if (height) file_json.emplace("height", height.value());
		if (thumbnail_file_extension) file_json.emplace("thumbnail_file_extension", thumbnail_file_extension.value());
		return file_json;
	}
	void coupleToPost(int post_id) { // called from Message.cppm
		db->query<void>("UPDATE stored_file SET coupled_post_id = $1 WHERE id = $2", post_id, this->id);
		this->coupled_post_id = post_id;
	}
	std::string name() const { return this->filename; }
	inline static const size_t MAX_FILE_NAME = 205;
	inline static const size_t MAX_FILE_NAME_WITH_UUID = 205+36;
	int getId() const { return this->id; }
	std::chrono::time_point<std::chrono::system_clock> createdAt() const { return this->created_at; }
private:
	std::optional<int> coupled_post_id;
	int id;
	std::optional<int> width, height;
	std::optional<std::string> thumbnail_file_extension;
	std::chrono::time_point<std::chrono::system_clock> created_at;
	std::string filename;
}; // class File
} // export namespace Mediaboard
