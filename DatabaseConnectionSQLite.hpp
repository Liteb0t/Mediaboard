#include "DatabaseConnection.hpp"
#include "db_interface.h"
#include <boost/filesystem/path.hpp>
#include <sqlite3.h>

class DatabaseConnectionSQLite : public DatabaseConnection {
public:
	DatabaseConnectionSQLite(boost::filesystem::path database_directory, std::string filename);
	~DatabaseConnectionSQLite() override;
	// void init() override;
	void init(boost::filesystem::path database_directory);
	int getUniquePermissionObjectId() const override { return -1; } // Not yet implemented
	int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) override;
	PermissionSettingIterator* retrievePermissionSettings(int permission_collection_id) override {
		return new PermissionSettingIteratorSQLite(permission_collection_id);
	}
private:
	class PermissionCollectionIteratorSQLite : public DatabaseConnection::PermissionCollectionIterator {
	public:
		PermissionCollectionIteratorSQLite(int permission_object_id) {
			db_create_cursor_for_permission_collection(permission_object_id);
		}
		~PermissionCollectionIteratorSQLite() override {
			db_free_cursor_for_permission_collection();
		}
		db_permission_collection_struct* getValue() const override { return db_cursor_retrieve_permission_collection(); }
	};
	PermissionCollectionIterator* retrievePermissionCollections(int permission_object_id) override {
		return new PermissionCollectionIteratorSQLite(permission_object_id);
	}
	class PermissionSettingIteratorSQLite : public DatabaseConnection::PermissionSettingIterator {
	public:
		PermissionSettingIteratorSQLite(int permission_collection_id) {
			db_create_cursor_for_permission_setting(permission_collection_id);
		}
		~PermissionSettingIteratorSQLite() override {
			db_free_cursor_for_permission_setting();
		}
		db_permission_setting_struct* getValue() const override { return db_cursor_retrieve_permission_setting(); }
	};
	sqlite3* db;
};
