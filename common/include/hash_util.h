// ===============================
// 이 파일은 비밀번호 등을 SHA-256 해시로 변환하는 함수의 선언을 포함합니다.
// OpenSSL 라이브러리를 사용하며, 해시 결과는 64자리 헥사 문자열입니다.
// (주의: 결과 버퍼는 최소 65바이트 이상이어야 합니다)

#ifndef HASH_UTIL_H
#define HASH_UTIL_H

// SHA-256 해시 문자열을 계산합니다.
// input: 해싱할 문자열
// output_hex: 해시 결과를 저장할 버퍼 (최소 65 바이트)
void compute_sha256(const char* input, char* output_hex);

#endif // HASH_UTIL_H