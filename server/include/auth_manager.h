// server/include/auth_manager.h
#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H
#include "protocol.h"

/*
 * 인증 시스템 초기화
 * 암호화 시스템도 함께 초기화됨
 */
void init_auth_system();

/*
 * 사용자 등록 구현
 * username: 사용자명
 * hashed_password: 클라이언트에서 이미 해시된 비밀번호
 * response_msg: 응답 메시지 출력 버퍼
 * 반환값: 성공 시 1, 실패 시 0
 */
int register_user_impl(const char* username, const char* hashed_password, char* response_msg);

/*
 * 사용자 로그인 구현
 * username: 사용자명
 * hashed_password: 클라이언트에서 이미 해시된 비밀번호
 * response_msg: 응답 메시지 출력 버퍼
 * logged_in_user: 로그인 성공 시 사용자명 저장 버퍼
 * 반환값: 성공 시 1, 실패 시 0
 */
int login_user_impl(const char* username, const char* hashed_password, char* response_msg, char* logged_in_user);

#endif