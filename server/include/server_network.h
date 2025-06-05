// server/include/server_network.h
#ifndef SERVER_NETWORK_H
#define SERVER_NETWORK_H

void* handle_client(void* arg);

/* Thread-safe login user management */
void init_logged_in_users(void);
void cleanup_logged_in_users_mutex(void);

#endif  // SERVER_NETWORK_H