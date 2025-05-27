// ===============================
// 이 파일은 문자열을 SHA-256 해시로 변환하는 함수의 구현을 포함합니다.
// OpenSSL의 SHA256 함수를 사용하여 64자리 16진수 해시값을 생성합니다.

#include "hash_util.h"
#include <openssl/sha.h>
#include <string.h>
#include <stdio.h>

// compute_sha256
// 입력 문자열을 받아 SHA-256 해시를 계산하여 헥사 문자열로 변환합니다.
// 결과는 output_hex에 저장되며 반드시 65바이트 이상이어야 합니다.
void compute_sha256(const char* input, char* output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];  // 32바이트 해시 결과 버퍼

    // 입력 문자열에 대해 SHA-256 해시 계산
    SHA256((const unsigned char*)input, strlen(input), hash);

    // 해시 결과를 헥사 문자열로 변환
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    }

    output_hex[64] = '\0';  // 널 종료 문자 추가
}