#include <stdlib.h>
#include <time.h>
#include "field_lengths.h"
#define DATABASE_PASSWORD_ENVIRONMENT_VARIABLE "FUZE_MEDIABOARD_PASSWORD"

#ifdef __cplusplus
extern "C" {
#endif
	struct db_thread_struct {
		int id;
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

	void initPostArray(struct db_post_array*, size_t);
	void insertToPostArray(struct db_post_array*, struct db_post_struct);
	void freePostArray(struct db_post_array*);
	void initThreadArray(struct db_thread_array*, size_t);
	void insertToThreadArray(struct db_thread_array*, struct db_thread_struct);
	void freeThreadArray(struct db_thread_array*);
	int db_store_post(int thread_id, int id_in_thread, const char name[POST_MAX_NAME+1], time_t upload_timestamp, const char*, char files[4][POST_MAX_FILE_NAME_WITH_UUID+1], int file_count, const char key[KEY_LENGTH+1]);
	int db_store_thread(int number_of_posts);
	// void db_mark_message_as_deleted(int thread_id, int message_id);
	void db_mark_post_as_deleted(int post_id);
	void db_mark_thread_as_deleted(int _thread_id);
	int db_store_account(const char* username, const char* password);
	char db_fetch_key(char* key, const char* username, const char* password);
	int db_key_matches_account(const char* _key, const char* _username);
	char db_change_password(const char* username, const char* old_password, const char* new_password);
	void db_create_administrator(const char* _password);
	struct db_post_struct db_retrieve_last_post();
	struct db_thread_array* db_retrieve_threads();
	struct db_post_array* db_retrieve_history();
	void db_connect(const char* _database_name);
	void db_disconnect(void);
#ifdef __cplusplus
}
#endif
