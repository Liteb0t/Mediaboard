#include "DatabaseConnection.hpp"

class DatabaseConnectionPostgres : public DatabaseConnection {
public:
	DatabaseConnectionPostgres(std::string connection_string, std::string database_name, unsigned short port);
	~DatabaseConnectionPostgres() override {
		db_disconnect();
	}
	int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) override;

	class PermissionCollectionIteratorECPG : public DatabaseConnection::PermissionCollectionIterator {
	public:
		PermissionCollectionIteratorECPG(int permission_object_id) {
			db_create_cursor_for_permission_collection(permission_object_id);
		}
		~PermissionCollectionIteratorECPG() override {
			db_free_cursor_for_permission_collection();
		}
		db_permission_collection_struct* getValue() const override { return db_cursor_retrieve_permission_collection(); }
	};
	PermissionCollectionIterator* retrievePermissionCollections(int permission_object_id) override {
		return new PermissionCollectionIteratorECPG(permission_object_id);
	}
	class PermissionSettingIteratorECPG : public DatabaseConnection::PermissionSettingIterator {
	public:
		PermissionSettingIteratorECPG(int permission_collection_id) {
			db_create_cursor_for_permission_setting(permission_collection_id);
		}
		~PermissionSettingIteratorECPG() override {
			db_free_cursor_for_permission_setting();
		}
		db_permission_setting_struct* getValue() const override { return db_cursor_retrieve_permission_setting(); }
	};
	PermissionSettingIterator* retrievePermissionSettings(int permission_collection_id) override {
		return new PermissionSettingIteratorECPG(permission_collection_id);
	}
};
