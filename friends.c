#include "friends.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/*
 * create a new user with the given name
 * insert it at the tail of the list of users whose head is pointed to by *user_ptr_add
 *
 * return:
 *   - 0 on success.
 *   - 1 if a user by this name already exists in this list.
 *   - 2 if the given name cannot fit in the 'name' array
 *       (don't forget about the null terminator).
 */
int create_user(const char *name, User **user_ptr_add) {
    if (strlen(name) >= MAX_NAME) {
        return 2;
    }

    User *new_user = malloc(sizeof(User));
    if (new_user == NULL) {
        perror("malloc");
        exit(1);
    }
    strncpy(new_user->name, name, MAX_NAME); // name has max length MAX_NAME - 1

    for (int i = 0; i < MAX_NAME; i++) {
        new_user->profile_pic[i] = '\0';
    }

    new_user->first_post = NULL;
    new_user->next = NULL;
    for (int i = 0; i < MAX_FRIENDS; i++) {
        new_user->friends[i] = NULL;
    }

    // Add user to list
    User *prev = NULL;
    User *curr = *user_ptr_add;
    while (curr != NULL && strcmp(curr->name, name) != 0) {
        prev = curr;
        curr = curr->next;
    }

    if (*user_ptr_add == NULL) {
        *user_ptr_add = new_user;
        return 0;
    } else if (curr != NULL) {
        free(new_user);
        return 1;
    } else {
        prev->next = new_user;
        return 0;
    }
}


/*
 * return a pointer to the user with this name in the list starting with head 
 * return NULL if no such user exists
 */
User *find_user(const char *name, const User *head) {
    while (head != NULL && strcmp(name, head->name) != 0) {
        head = head->next;
    }

    return (User *)head;
}


/*
 * print the usernames of all users in the list starting at curr
 */
char *list_users(const User *curr) {
    int num_chars = 11;
    const User *clone = curr;
    while (clone != NULL) {
        int user_chars = strlen(clone->name) + 2;
        num_chars = num_chars + user_chars;
        clone = clone->next;
    }

    char *user_string = malloc(sizeof(char) * num_chars);
    if (user_string == NULL) {
        perror("malloc");
        exit(-1);
    }

    strcpy(user_string, "User List\n");
    while (curr != NULL) {
        strcat(user_string, "\t");
        strcat(user_string, curr->name);
        strcat(user_string, "\n");
        curr = curr->next;
    }
    return user_string;
}


/*
 * make two users friends with each other (symmetric - a pointer to
 * each user must be stored in the 'friends' array of the other)
 *
 * return:
 *   - 0 on success.
 *   - 1 if the two users are already friends.
 *   - 2 if the users are not already friends, but at least one already has
 *     MAX_FRIENDS friends.
 *   - 3 if the same user is passed in twice.
 *   - 4 if at least one user does not exist.
 */
int make_friends(const char *name1, const char *name2, User *head) {
    User *user1 = find_user(name1, head);
    User *user2 = find_user(name2, head);

    if (user1 == NULL || user2 == NULL) {
        return 4;
    } else if (user1 == user2) { // Same user
        return 3;
    }

    int i, j;
    for (i = 0; i < MAX_FRIENDS; i++) {
        if (user1->friends[i] == NULL) { // Empty spot
            break;
        } else if (user1->friends[i] == user2) { // Already friends.
            return 1;
        }
    }

    for (j = 0; j < MAX_FRIENDS; j++) {
        if (user2->friends[j] == NULL) { // Empty spot
            break;
        }
    }

    if (i == MAX_FRIENDS || j == MAX_FRIENDS) { // Too many friends.
        return 2;
    }

    user1->friends[i] = user2;
    user2->friends[j] = user1;
    return 0;
}


// print a post
char *print_post(const Post *post) {
    if (post == NULL) {
        return NULL;
    }

    int post_chars = 1; // 1 = \0
    post_chars = post_chars + strlen(post->author) + 7; // 7 is "From: " and \n
    post_chars = post_chars + strlen(asctime(localtime(post->date))) + 7; // 7 is "Date: " and \n
    post_chars = post_chars + strlen(post->contents) + 1; // 1 is \n

    char *post_string = malloc(sizeof(char) * post_chars);
    if (post_string == NULL) {
        perror("malloc");
        exit(-1);
    }

    strcpy(post_string, "From: ");
    strcat(post_string, post->author);
    strcat(post_string, "\nDate: ");
    strcat(post_string, asctime(localtime(post->date)));
    strcat(post_string, "\n");
    strcat(post_string, post->contents);
    strcat(post_string, "\n");

    return post_string;
}


/*
 * print a user profile
 * return:
 *   - 0 on success.
 *   - 1 if the user is NULL.
 */
char *print_user(const User *user) {
    if (user == NULL) {
        char *profile_string = malloc(sizeof(char) * 16);
        strcpy(profile_string, "User not found\n");
        return profile_string;
    }

    int profile_chars = 130; // 130 = characters in each separator bar + one null terminator
    profile_chars = profile_chars + strlen(user->name) + 8; // 8 is "Name: " and two \n"
    profile_chars = profile_chars + 9; // 9 is "Friends:\n"
    for (int i = 0; i < MAX_FRIENDS && user->friends[i] != NULL; i++) {
        profile_chars = profile_chars + strlen(user->friends[i]->name) + 1; // 1 is \n
    }
    profile_chars = profile_chars + 7; // 7 is "Posts:\n"
    const Post *curr = user->first_post;
    while (curr != NULL) {
        profile_chars = profile_chars + strlen(print_post(curr));
        curr = curr->next;
        if (curr != NULL) {
            profile_chars = profile_chars + 6;
        }
    }

    char *profile_string = malloc(sizeof(char) * profile_chars);
    if (profile_string == NULL) {
        perror("malloc");
        exit(-1);
    }

    strcpy(profile_string, "Name: ");
    strcat(profile_string, user->name);
    strcat(profile_string, "\n\n------------------------------------------\nFriends:\n");
    for (int i = 0; i < MAX_FRIENDS && user->friends[i] != NULL; i++) {
        strcat(profile_string, user->friends[i]->name);
        strcat(profile_string, "\n");
    }
    strcat(profile_string, "------------------------------------------\nPosts:\n");
    const Post *curr2 = user->first_post;
    while (curr2 != NULL) {
        strcat(profile_string, print_post(curr2));
        curr2 = curr2->next;
        if (curr != NULL) {
            strcat(profile_string, "\n===\n\n");
        }
    }
    strcat(profile_string, "------------------------------------------\n");

    return profile_string;
}


/*
 * make a new post from 'author' to the 'target' user,
 * containing the given contents, IF the users are friends
 *
 * return:
 *   - 0 on success
 *   - 1 if users exist but are not friends
 *   - 2 if either User pointer is NULL
 */
int make_post(const User *author, User *target, char *contents) {
    if (target == NULL || author == NULL) {
        return 2;
    }

    int friends = 0;
    for (int i = 0; i < MAX_FRIENDS && target->friends[i] != NULL; i++) {
        if (strcmp(target->friends[i]->name, author->name) == 0) {
            friends = 1;
            break;
        }
    }

    if (friends == 0) {
        return 1;
    }

    // Create post
    Post *new_post = malloc(sizeof(Post));
    if (new_post == NULL) {
        perror("malloc");
        exit(1);
    }
    strncpy(new_post->author, author->name, MAX_NAME);
    new_post->contents = contents;
    new_post->date = malloc(sizeof(time_t));
    if (new_post->date == NULL) {
        perror("malloc");
        exit(1);
    }
    time(new_post->date);
    new_post->next = target->first_post;
    target->first_post = new_post;

    return 0;
}

