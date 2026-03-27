#pragma once
#include "db_interface.h"
#include <string>

enum struct USER_OR_GROUP {USER, GROUP};

class DatabaseConnection {
public:
	virtual ~DatabaseConnection() {
	}
	// virtual void init() = 0;
	virtual int getUniquePermissionObjectId() const = 0;
	class PermissionCollectionIterator {
	public:
		virtual ~PermissionCollectionIterator() {}
		virtual db_permission_collection_struct* getValue() const = 0;
	};
	virtual PermissionCollectionIterator* retrievePermissionCollections(int permission_object_id) = 0;
	virtual int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) = 0;
	class PermissionSettingIterator {
	public:
		virtual ~PermissionSettingIterator() {}
		virtual db_permission_setting_struct* getValue() const = 0;
	};
	virtual PermissionSettingIterator* retrievePermissionSettings(int permission_collection_id) = 0;
protected:
	// virtual const std::string getDatabaseVersion() const = 0;
	// virtual void connectToDatabase(const std::string& connection_target);
};
