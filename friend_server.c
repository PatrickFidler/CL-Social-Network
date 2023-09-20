#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "friends.h"

#ifndef PORT
  #define PORT 53232
#endif
#define BUFFER_SIZE 128
#define MAX_BACKLOG 5
#define INPUT_ARG_MAX_NUM 12
#define DELIM " \n"

// my data structure, storing (for each client):
// - file descriptor
// - username (used to locate user in the users data structure)
// - file buffer, for partial reads
// - pointer to next client
typedef struct sockname {
    int sock_fd;
    char *username;
    char *buf;
    struct sockname *next;
} Client;

// all helper function signatures, commented where they appear
int accept_connection(int fd, Client *clients);
int read_from(Client *clients, User **user_list_ptr);
int find_network_newline(const char *buf, int n);
int tokenize(char *cmd, char **cmd_argv);
char *process_args(int cmd_argc, char **cmd_argv, User **user_list_ptr, char *username);

int main(void) {
    // initializing the first client in a "null" state so we have a data structure to reference
    Client client;
    client.sock_fd = -1;
    client.username = NULL;
    client.next = NULL;
    // our clients data structure
    Client *clients = &client;

    // create socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("server: socket");
        exit(1);
    }

    // initialize server
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    // for port convienience
    int on = 1;
    int status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
                            (const char *) &on, sizeof(on));
    if (status == -1) {
        perror("setsockopt -- REUSEADDR");
    }

    // reset for safety
    memset(&server.sin_zero, 0, 8);

    // bind socket to server
    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("server: bind");
        close(sock_fd);
        exit(1);
    }

    // listen on socket
    if (listen(sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }
    
    // initialize set of file descriptors
    int max_fd = sock_fd;
    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_SET(sock_fd, &all_fds);

    // initialize user data structure
    User *user_list = NULL;

    // server loop
    while (1) {
        // clone set for select call
        fd_set listen_fds = all_fds;
        if ((select(max_fd + 1, &listen_fds, NULL, NULL, NULL)) == -1) {
            perror("server: select");
            exit(1);
        }

        // used to check if this is a new connection, updates all atributes accordingly
        if (FD_ISSET(sock_fd, &listen_fds)) {
            int client_fd = accept_connection(sock_fd, clients);
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            // send a message to the newly connected client so they know to send a username
            write(client_fd, "What is your user name?\n", 24);
        }

        // clone clients data structure so we can iterate through it without mutating
        Client *clients_clone = clients;
        while (clients_clone != NULL) {
            // if the client exists and is ready to be read from, read from it
            if (clients_clone->sock_fd > -1 && FD_ISSET(clients_clone->sock_fd, &listen_fds)) {
                int client_closed = read_from(clients_clone, &user_list);
                // if read_from returns an fd number, the client disconnected,
                // so act accordingly
                if (client_closed > 0) {
                    FD_CLR(client_closed, &all_fds);
                    close(clients->sock_fd);
                    clients->sock_fd = -1;
                }
            }
            // iterate through to next client
            clients_clone = clients_clone->next;
        }
    }
    return 1;
}

// accepts the new client's (specified by fd) connection
// returns the file descriptor
int accept_connection(int fd, Client *clients) {
    // clone clients data structure and iterate until we find next availiable client spot
    Client *clients_clone = clients;
    while (clients_clone->sock_fd != -1) {
        if (clients_clone->next != NULL) {
            clients_clone = clients_clone->next;
        } else {
            // init new client
            Client *new_client = malloc(sizeof(Client));
            new_client->sock_fd = -1;
            clients_clone->next = new_client;
        }
    }

    // accept the new client
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    // finish setting up new client
    clients_clone->sock_fd = client_fd;
    clients_clone->username = NULL;
    clients_clone->buf = malloc(sizeof(char) * BUFFER_SIZE);
    clients_clone->next = NULL;
    return client_fd;
}

// reads from the specified client (will always be the front of the provided clients structure)
int read_from(Client *clients, User **user_list_ptr) {
    // initialize variables for partial read buffer
    int fd = clients->sock_fd;
    char buf[BUFFER_SIZE];
    int inbuf;
    int room;
    char *after;

    // proceed depending on if this client has been partialy read from or not
    if (clients->buf == NULL) {
        buf[BUFFER_SIZE] = '\0';
        inbuf = 0;           
        room = sizeof(buf); 
        after = buf; 
    } else {
        strcpy(buf, clients->buf);
        inbuf = strlen(buf);
        room = sizeof(buf) - inbuf;
        after = buf + inbuf;
    }
	
    // read from the client
    if (read(fd, after, room) > 0) {
        int where;
        // this while loop will only trigger if a full read has finished
        while ((where = find_network_newline(buf, strlen(buf))) > 0) {
            buf[where - 2] = '\0';
            // if no username was declared, this read was the client giving a username
            if (clients->username == NULL) {
                // allocating space based on if they gave a name longer than 32 chars
                if (strlen(buf) < 32) {
                    clients->username = malloc(sizeof(char) * (strlen(buf) + 1));
                    strncpy(clients->username, buf, strlen(buf));
                    clients->username[strlen(buf)] = '\0';
                } else {
                    clients->username = malloc(sizeof(char) * 32);
                    strncpy(clients->username, buf, 31);
                    clients->username[31] = '\0';
                }
                // create the new user in our user structure, or welcome them back if they already existed
                if (create_user(clients->username, user_list_ptr) == 1) {
                    write(clients->sock_fd, "Welcome back.\nGo ahead and enter user commands>\n", 48);
                } else {
                    write(clients->sock_fd, "Welcome.\nGo ahead and enter user commands>\n", 43);
                }
            // this client already gave a username, so this read was a command
            } else {
                // initialize cmd_argv for processing arguments
                char *cmd_argv[INPUT_ARG_MAX_NUM];
                // clone buffer for tokenization
                char buf_clone[BUFFER_SIZE];
                strncpy(buf_clone, buf, where - 1);
                int cmd_argc = tokenize(buf_clone, cmd_argv);
                // process the given arguments, to_write contains desired server output
                char *to_write = process_args(cmd_argc, cmd_argv, user_list_ptr, clients->username);
                // the user disconnected if to_write is null and they didn't just hit enter
                if (cmd_argc > 0 && (to_write == NULL)) {
                    // reset all client attributes we can reset now accordingly (rest done outside helper)
                    memset(clients->username, '\0', strlen(clients->username));
                    clients->username = NULL;
                    free(clients->username);
                    return clients->sock_fd;
                // otherwise, this was another command and we just want to give the output
                } else {
                    write(clients->sock_fd, to_write, strlen(to_write));
                }
            }
            // full read finished, so reset buffer
            memset(buf, '\0', 128);
        }
        // pass on the full/partial read to buf attribute for next read
        strcpy(clients->buf, buf);
    }
    return 0;
}

// locates and returns the position of the network newline (if it exists)
int find_network_newline(const char *buf, int n) {
    for (int i = 0; i < n - 1; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            return 1 + (i + 1);
        }
    }
    return -1;
}

// tokenizes the given command into different arguments
int tokenize(char *cmd, char **cmd_argv) {
    int cmd_argc = 0;
    char *next_token = strtok(cmd, DELIM);    
    while (next_token != NULL) {
        if (cmd_argc >= INPUT_ARG_MAX_NUM - 1) {
            perror("Too many arguments!");
            cmd_argc = 0;
            break;
        }
        cmd_argv[cmd_argc] = next_token;
        cmd_argc++;
        next_token = strtok(NULL, DELIM);
    }

    return cmd_argc;
}

// processes the array of arguments given, potentially using the other info passed to it
char *process_args(int cmd_argc, char **cmd_argv, User **user_list_ptr, char *username) {
    User *user_list = *user_list_ptr;
    // nothing was typed when client hit enter
    if (cmd_argc <= 0) {
        return "";
    // user is quitting, return null
    } else if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
        return NULL;
    // user wants a user list, use modified list_users function to get correct output
    } else if (strcmp(cmd_argv[0], "list_users") == 0 && cmd_argc == 1) {
        char *buf = list_users(user_list);
        return buf;
    // user wants to make friends with another, use modified make_friends function to get correct output
    // (and make the nessecary changes in the user structure)
    } else if (strcmp(cmd_argv[0], "make_friends") == 0 && cmd_argc == 2) {
        switch (make_friends(username, cmd_argv[1], user_list)) {
            case 1:
                return "You are already friends\n";
            case 2:
                return "At least one of you has entered the max number of friends\n";
            case 3:
                return "You can't friend yourself\n";
            case 4:
                return "The user you entered does not exist\n";
            default:
                return "";
        }
    // user wants to post to another user, use modified make_post function to get correct output
    // (and make the nessecary changes in the user structure)
    } else if (strcmp(cmd_argv[0], "post") == 0 && cmd_argc >= 3) {
        // first determine how long a string we need
        int space_needed = 0;
        for (int i = 3; i < cmd_argc; i++) {
            space_needed += strlen(cmd_argv[i]) + 1;
        }

        // allocate the space
        char *contents = malloc(space_needed);
        if (contents == NULL) {
            perror("malloc");
            exit(1);
        }

        // copy in the bits to make a single string
        strcpy(contents, cmd_argv[2]);
        for (int i = 3; i < cmd_argc; i++) {
            strcat(contents, " ");
            strcat(contents, cmd_argv[i]);
        }

        User *author = find_user(username, user_list);
        User *target = find_user(cmd_argv[1], user_list);
        switch (make_post(author, target, contents)) {
            case 1:
                return "You can only post to your friends\n";
            case 2:
                return "The user you want to post to does not exist\n";
			default:
				return "";
        }
    // user wants to see a profile, use print_user function to get correct output
    } else if (strcmp(cmd_argv[0], "profile") == 0 && cmd_argc == 2) {
        User *user = find_user(cmd_argv[1], user_list);
        if (print_user(user) == NULL) {
            return "User not found\n";
        } else {
            char *buf = print_user(user);
            return buf;
        }
    // nothing was valid, return message accordingly
    } else {
        return "Incorrect syntax\n";
    }
    return 0;
}
