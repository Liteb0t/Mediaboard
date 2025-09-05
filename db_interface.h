#include <stdlib.h>
#include <time.h>

// the database must NOT have entries longer than these limits
#define POST_MAX_NAME 100
// #define POST_MAX_EMAIL 254
#define POST_MAX_CONTENT 10000
#define TIMESTAMP_LEN 20
#define MAX_THREADS_PER_BOARD 50

#ifdef __cplusplus
extern "C" {
#endif
	struct db_thread_struct {
		int id;
		// Add char* subject later
		int number_of_posts;
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
		char name[POST_MAX_NAME];
		// char upload_timestamp[TIMESTAMP_LEN];
		time_t upload_timestamp;
		char files[4][256];  // Maximum of 4 files per post
		short number_of_files;
		char* content;
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
	int db_store_post(int /*thread id*/, int /*id_in_thread*/, const char* /*name*/, time_t /*upload_timestamp*/, const char*, char[4][256] /*files*/, int /*file_count*/);
	int db_store_thread(int /*number_of_posts*/);
	struct db_post_struct db_retrieve_last_post();
	struct db_thread_array* db_retrieve_threads();
	struct db_post_array* db_retrieve_history();
	void db_connect();
	void db_disconnect();
#ifdef __cplusplus
}
#endif
