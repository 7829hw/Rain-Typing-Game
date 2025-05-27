// common/include/hash_util.h
#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <stdint.h>

/* SHA-256 해시 길이 상수 */
#define SHA256_DIGEST_LENGTH 32
#define SHA256_HEX_LENGTH (SHA256_DIGEST_LENGTH * 2 + 1)

/*
 * 비밀번호를 SHA-256으로 해싱하여 16진수 문자열로 변환
 * password: 입력 비밀번호 (평문)
 * hash_output: 출력 버퍼 (최소 SHA256_HEX_LENGTH 크기)
 * 반환값: 성공 시 1, 실패 시 0
 */
int hash_password_sha256(const char* password, char* hash_output);

/*
 * 암호화 시스템 초기화 (OpenSSL 초기화)
 * 반환값: 성공 시 1, 실패 시 0
 */
int crypto_init(void);

/*
 * 암호화 시스템 정리 (OpenSSL 정리)
 */
void crypto_cleanup(void);

#endif /* CRYPTO_UTILS_H */