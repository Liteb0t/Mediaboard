#pragma once
#include "db_interface.h"
#include <array>
#include <memory>
#include <string>

enum struct USER_OR_GROUP {USER, GROUP};

class DatabaseConnection {
public:
	virtual ~DatabaseConnection() {
	}
	// virtual void init() = 0;
	virtual int getUniquePermissionObjectId() const = 0;
	virtual int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) = 0;
	virtual void declarePermissionCollectionCursor(int permission_object_id) = 0;
	virtual db_permission_collection_struct* getValueFromPermissionCollectionCursor() = 0;
	virtual void closePermissionCollectionCursor() = 0;
	virtual void declarePermissionSettingCursor(int permission_collection_id) = 0;
	virtual db_permission_setting_struct* getValueFromPermissionSettingCursor() = 0;
	virtual void closePermissionSettingCursor() = 0;
	class TestIterator {
	public:
		virtual void printClassType() const = 0;
	};
	virtual TestIterator* getTestIterator() = 0;
	class TestRange {
	public:
		friend bool operator==(const TestRange& lhs, const TestRange& rhs) {
			return !(lhs.isEnd() || rhs.isEnd()) || lhs.getId() == rhs.getId();
		}
		friend bool operator!=(const TestRange& lhs, const TestRange& rhs) {
			return !(lhs.getId() == rhs.getId());
		}
		virtual int operator*() = 0;
	protected:
		int index;
	private:
		virtual int getId() const = 0;
		virtual bool isEnd() const = 0;
	};
	class TestRangeInitialiser {
	public:
		virtual TestRange* begin() = 0;
	};
	// https://stackoverflow.com/a/79917118/18658154
	class Iterator {
	 std::unique_ptr<TestRange> impl;
	 public:
		 Iterator(std::unique_ptr<TestRange> p) : impl(std::move(p)) {}
		 int operator*() const {
			 return impl->deref();

		}
		Iterator& operator++() {
			impl->inc(); return *this;

		}
		bool operator!=(const Iterator& other) const {
			return !impl->equals(*other.impl);

		}

};
	virtual TestRangeInitialiser& getTestRangeInitialiser() = 0;
	// virtual TestRange* getTestRange() = 0;
protected:
	// virtual const std::string getDatabaseVersion() const = 0;
	// virtual void connectToDatabase(const std::string& connection_target);
};
