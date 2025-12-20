// the database must NOT have entries longer than these limits.
#define POST_MAX_NAME 32
// #define POST_MAX_EMAIL 254
#define POST_MAX_CONTENT 5000
// #define MAX_THREADS_PER_BOARD 50
#define KEY_LENGTH 8
#define POST_MAX_FILE_NAME 205 // The Unix file name limit is 255, but some space is required for UUID and other parts.
#define POST_MAX_FILE_NAME_WITH_UUID POST_MAX_FILE_NAME+36
#define GROUP_MAX_NAME 32
#define ACCOUNT_MAX_USERNAME 32
#define ACCOUNT_MIN_PASSWORD 1
