// server/include/auth_manager.h
#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H
#include "protocol.h"

void init_auth_system();
int register_user_impl(const char* username, const char* password, char* response_msg);
int login_user_impl(const char* username, const char* password, char* response_msg, char* logged_in_user);

#endif