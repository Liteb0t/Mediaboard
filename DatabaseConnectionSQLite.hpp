#include "DatabaseConnection.hpp"
#include "db_interface.h"
#include <sqlite3.h>

class DatabaseConnectionSQLite : public DatabaseConnection {
public:
	DatabaseConnectionSQLite(std::string file);
	~DatabaseConnectionSQLite() override;

	int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) override;

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
	PermissionSettingIterator* retrievePermissionSettings(int permission_collection_id) override {
		return new PermissionSettingIteratorSQLite(permission_collection_id);
	}
private:
	sqlite3* db;
};
