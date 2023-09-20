#include <time.h>

#define MAX_NAME 32 // max username AND profile_pic filename lengths
#define MAX_FRIENDS 10 

typedef struct user {
    char name[MAX_NAME];
    char profile_pic[MAX_NAME];
    struct post *first_post;
    struct user *friends[MAX_FRIENDS];
    struct user *next;
} User;

typedef struct post {
    char author[MAX_NAME];
    char *contents;
    time_t *date;
    struct post *next;
} Post;

int create_user(const char *name, User **user_ptr_add);

User *find_user(const char *name, const User *head);

char *list_users(const User *curr);

int make_friends(const char *name1, const char *name2, User *head);

char *print_user(const User *user);

int make_post(const User *author, User *target, char *contents);


