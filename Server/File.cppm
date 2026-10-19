module;
#include <ctime>
#include <boost/json.hpp>
#include <expected>
#include <filesystem>
#include <print>
#include <string>
export module Mediaboard.File;

import FuzeDBI;
import FuzeHttp.PermissionObject;

export namespace Mediaboard {
class File : public FuzeHttp::PermissionManagedObject {
public:
	enum struct STATUS : int {NORMAL=0, CONDEMNED, DESTROYED, MISSING};
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
			thumbnail_file_extension(thumbnail_file_extension),
			status(STATUS::NORMAL) {
		int time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(this->created_at.time_since_epoch()).count();
		// TODO allow std::optional args to FuzeDBI::query
		if (width && height && thumbnail_file_extension) {
			db->query<void>("INSERT INTO stored_file(id, permission_object_id, filename, created_at, width, height, thumbnail_file_extension) VALUES ($1, $2, $3, $4, $5, $6, $7)", this->id, this->getPermissionObjectId(), filename.c_str(), time_since_epoch, width.value(), height.value(), thumbnail_file_extension.value());
		}
		else {
			db->query<void>("INSERT INTO stored_file(id, permission_object_id, filename, created_at) VALUES ($1, $2, $3, $4)", this->id, this->getPermissionObjectId(), filename.c_str(), time_since_epoch);
		}
	}
	File(PermissionObjectBase* permission_parent, FuzeDBI::Connection* db, int id, int permission_object_id, std::string filename, std::chrono::time_point<std::chrono::system_clock> created_at, std::optional<int> coupled_board_id, std::optional<int> coupled_post_id, std::optional<int> width, std::optional<int> height, std::optional<std::string> thumbnail_file_extension, std::optional<std::chrono::time_point<std::chrono::system_clock>> deleted_at, STATUS status)
			: PermissionManagedObject(permission_parent, permission_object_id, db),
			id(id),
			filename(filename),
			created_at(created_at),
			coupled_board_id(coupled_board_id),
			coupled_post_id(coupled_post_id),
			width(width),
			height(height),
			thumbnail_file_extension(thumbnail_file_extension),
			deleted_at(deleted_at),
			status(status) {
	}
	boost::json::object asJson() const {
		boost::json::object file_json = {{"filename", filename}};
		if (width) file_json.emplace("width", width.value());
		if (height) file_json.emplace("height", height.value());
		if (thumbnail_file_extension) file_json.emplace("thumbnail_file_extension", thumbnail_file_extension.value());
		if (status == STATUS::CONDEMNED || status == STATUS::DESTROYED)
			file_json.emplace("deleted", true);
		// if (status != STATUS::NORMAL)
		//	file_json.emplace("status_text", statusText());
		return file_json;
	}
	bool isCoupledToBoard(int board_id) const { return coupled_board_id && board_id == coupled_board_id.value(); }
	void coupleToPost(int board_id, int post_id) { // called from Message.cppm
		db->query<void>("UPDATE stored_file SET coupled_board_id = $1 WHERE id = $2", board_id, this->id);
		this->coupled_board_id = board_id;
		db->query<void>("UPDATE stored_file SET coupled_post_id = $1 WHERE id = $2", post_id, this->id);
		this->coupled_post_id = post_id;
	}
	void markAsDeleted() {
		this->deleted_at = std::chrono::system_clock::now();
		int time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(deleted_at->time_since_epoch()).count();
		db->query<void>("UPDATE stored_file SET deleted_at = $1 WHERE id = $2", time_since_epoch, id);
		setStatus(STATUS::CONDEMNED);
	}
	void restore() {
		if (deleted_at)
			deleted_at = {};
		db->query<void>("UPDATE stored_file SET deleted_at = NULL WHERE id = $1", id);
		setStatus(STATUS::NORMAL);
	}
	// bool isDeleted() const {
	// 	return deleted_at ? true : false;
	// }
	void setStatus(STATUS new_status) {
		if (new_status == status)
			return;
		status = new_status;
		db->query<void>("UPDATE stored_file SET status = $1 WHERE id = $2", static_cast<int>(new_status), id);
	}
	static std::filesystem::path getThumbnailPath(std::filesystem::path base, std::string filename, std::string thumbnail_file_extension) {
		std::filesystem::path thumbnail_path = base / "thumbnails" / std::format("THUMBNAIL_{}.{}", filename, thumbnail_file_extension);
		return thumbnail_path;
	}
	std::filesystem::path getThumbnailPath(const std::filesystem::path& base) {
		if (!thumbnail_file_extension)
			return {};
		std::filesystem::path thumbnail_path = base / "thumbnails" / std::format("THUMBNAIL_{}.{}", filename, thumbnail_file_extension.value());
		return thumbnail_path;
	}
	STATUS getStatus() const { return status; }
	std::string name() const { return this->filename; }
	inline static const size_t MAX_FILE_NAME = 205;
	inline static const size_t MAX_FILE_NAME_WITH_UUID = 205+36;
	int getId() const { return this->id; }
	std::chrono::time_point<std::chrono::system_clock> createdAt() const { return this->created_at; }
private:
	std::string statusText() const {
		if (status == STATUS::NORMAL)
			return "normal";
		else if (status == STATUS::CONDEMNED)
			return "condemned";
		else if (status == STATUS::DESTROYED)
			return "destroyed";
		else if (status == STATUS::MISSING)
			return "missing";
		else
			return "unknown";
	}
	STATUS status;
	int id;
	std::optional<int> coupled_board_id;
	std::optional<int> coupled_post_id;
	std::optional<int> width, height;
	std::optional<std::string> thumbnail_file_extension;
	std::chrono::time_point<std::chrono::system_clock> created_at;
	std::optional<std::chrono::time_point<std::chrono::system_clock>> deleted_at;
	std::string filename;
	// bool recently_deleted = false;
}; // class File
} // export namespace Mediaboard
