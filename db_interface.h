#ifndef DB_INTERFACE_H
#define DB_INTERFACE_H

#include <stdlib.h>
#include <time.h>
#include "field_lengths.h"
#define DATABASE_PASSWORD_ENVIRONMENT_VARIABLE "FUZE_MEDIABOARD_PASSWORD"
#define NUMBER_OF_PERMISSIONS 10
#define GROUP_DENY 0
#define GROUP_INHERIT 1
#define GROUP_ALLOW 2

#define GROUP_ADMINISTRATORS 0
#define GROUP_USERS 1
#define GROUP_PUBLIC 2
#define STR_GROUP_ADMINISTRATORS "0"
#define STR_GROUP_USERS "1"
#define STR_GROUP_PUBLIC "2"

#ifdef __cplusplus
extern "C" {
#endif
	// Dynamic array structs
	struct db_thread_struct {
		int id;
		int permission_object_id;
		// Add char* subject later
		int number_of_posts;
		char deleted;
	};
	struct db_thread_array {
		struct db_thread_struct* array;
		size_t used;
		size_t size;
	};
	struct db_post_struct {
		int id;
		int thread_id;
		int id_in_thread;
		char name[POST_MAX_NAME+1];
		// char upload_timestamp[TIMESTAMP_LEN];
		time_t upload_timestamp;
		char files[4][POST_MAX_FILE_NAME_WITH_UUID+1];  // Maximum of 4 files per post
		short number_of_files;
		char* content;
		char key[KEY_LENGTH+1];
		char deleted;
	};
	struct db_post_array {
		struct db_post_struct* array;
		size_t used;
		size_t size;
	};
	struct db_group_struct {
		int id;
		char name[GROUP_MAX_NAME+1];
		// int rank;
		// char permissions[NUMBER_OF_PERMISSIONS+1];
	};
	struct db_group_array {
		struct db_group_struct* array;
		size_t used;
		size_t size;
	};
	struct db_group_heirarchy_struct {
		int rank;
		int group_id;
	};
	struct db_group_heirarchy_array {
		struct db_group_heirarchy_struct* array;
		size_t used;
		size_t size;
	};
	struct db_account_struct {
		int id;
		char username[ACCOUNT_MAX_USERNAME+1];
		char key[KEY_LENGTH+1];
	};
	struct db_account_array {
		struct db_account_struct* array;
		size_t used;
		size_t size;
	};
	struct db_group_member_struct {
		int group_id;
		int account_id;
	};
	struct db_group_member_array {
		struct db_group_member_struct* array;
		size_t used;
		size_t size;
	};
	struct db_permission_collection_struct {
		int id;
		// int permission_object_id;
		int group_id;
		int account_id;
	};
	struct db_permission_collection_array {
		struct db_permission_collection_struct* array;
		size_t used;
		size_t size;
	};
	struct db_permission_setting_struct {
		int id;
		// int permission_collection_id;
		int permission_number;
		int setting;
	};
	struct db_permission_setting_array {
		struct db_permission_setting_struct* array;
		size_t used;
		size_t size;
	};

	// Dynamic array functions
	void initPostArray(struct db_post_array*, size_t);
	void insertToPostArray(struct db_post_array*, struct db_post_struct);
	void freePostArray(struct db_post_array*);
	void initThreadArray(struct db_thread_array*, size_t);
	void insertToThreadArray(struct db_thread_array*, struct db_thread_struct);
	void freeThreadArray(struct db_thread_array*);
	void initGroupArray(struct db_group_array*, size_t);
	void insertToGroupArray(struct db_group_array*, struct db_group_struct);
	void freeGroupArray(struct db_group_array*);
	void initGroupHeirarchyArray(struct db_group_heirarchy_array*, size_t);
	void insertToGroupHeirarchyArray(struct db_group_heirarchy_array*, struct db_group_heirarchy_struct);
	void freeGroupHeirarchyArray(struct db_group_heirarchy_array*);
	void initAccountArray(struct db_account_array*, size_t);
	void insertToAccountArray(struct db_account_array*, struct db_account_struct);
	void freeAccountArray(struct db_account_array*);
	void initGroupMemberArray(struct db_group_member_array*, size_t);
	void insertToGroupMemberArray(struct db_group_member_array*, struct db_group_member_struct);
	void freeGroupMemberArray(struct db_group_member_array*);
	void initPermissionCollectionArray(struct db_permission_collection_array*, size_t);
	void insertToPermissionCollectionArray(struct db_permission_collection_array*, struct db_permission_collection_struct);
	void freePermissionCollectionArray(struct db_permission_collection_array*);
	void initPermissionSettingArray(struct db_permission_setting_array*, size_t);
	void insertToPermissionSettingArray(struct db_permission_setting_array*, struct db_permission_setting_struct);
	void freePermissionSettingArray(struct db_permission_setting_array*);

	// Database functions
		// Posts
	int db_store_post(int thread_id, int id_in_thread, const char name[POST_MAX_NAME+1], time_t upload_timestamp, const char*, char files[4][POST_MAX_FILE_NAME_WITH_UUID+1], int file_count, const char key[KEY_LENGTH+1]);
	int db_store_thread(int number_of_posts, int _new_permission_object_id);
	void db_mark_post_as_deleted(int post_id);
	void db_mark_thread_as_deleted(int _thread_id);
		// Accounts
	int db_account_username_exists(const char* _username);
	int db_store_account(const char* username, const char* password);
	const char db_fetch_key(char* key, const char* username, const char* password);
	int db_key_matches_account(const char* _key, const char* _username);
	int db_account_matches_password(int _account_id, const char* _password);
	char db_change_password(const char* username, const char* old_password, const char* new_password);
	void db_create_administrator(const char* _password);
		// Permissions
	int db_store_group(const char name_[GROUP_MAX_NAME+1]);
	void db_delete_group(int _group_id);
	void db_add_member_to_group(int _user_id, int _group_id);
	void db_remove_member_from_group(int _user_id, int _group_id);
	void db_update_group_heirarchy(struct db_group_heirarchy_array* group_heirarchy);
	int db_get_unique_permission_object_id();
	int db_store_permission_collection(int _permission_object_id, int _user_id, int _group_id);
	void db_delete_permission_collection(int _permission_collection_id);
	int db_store_permission_setting(int _permission_collection_id, int _permission_number, int _setting);
	void db_update_permission_setting(int _permission_setting_id, int _setting);

	// Read from database into dynamic array
	struct db_post_struct db_retrieve_last_post(void);
	struct db_thread_array* db_retrieve_threads(void);
	struct db_post_array* db_retrieve_history(void);
	struct db_group_array* db_retrieve_groups(void);
	struct db_group_heirarchy_array* db_retrieve_group_heirarchy(void);
	struct db_account_array* db_retrieve_accounts(void);
	struct db_group_member_array* db_retrieve_group_members(void);
	struct db_permission_collection_array* db_retrieve_permission_collections_for_permission_object(int permission_object_id);
	struct db_permission_setting_array* db_retrieve_permission_settings_for_permission_collection(int permission_collection_id);

	void db_make_migrations();
	void db_connect(const char* _database_name);
	void db_disconnect(void);
#ifdef __cplusplus
}
#endif
#endif
