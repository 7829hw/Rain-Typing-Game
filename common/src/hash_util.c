// common/src/hash_util.c
#include "hash_util.h"

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OpenSSL 초기화 상태 플래그 */
static int crypto_initialized = 0;

/*
 * 바이너리 데이터를 16진수 문자열로 변환
 */
static void bytes_to_hex(const unsigned char* bytes, int len, char* hex_output) {
  for (int i = 0; i < len; i++) {
    sprintf(hex_output + (i * 2), "%02x", bytes[i]);
  }
  hex_output[len * 2] = '\0';
}

int crypto_init(void) {
  if (crypto_initialized) {
    return 1;
  }

  /* OpenSSL 3.0+ 에서는 자동 초기화되므로 별도 초기화 불필요 */
  crypto_initialized = 1;
  return 1;
}

void crypto_cleanup(void) {
  if (!crypto_initialized) {
    return;
  }

  /* OpenSSL 3.0+ 에서는 자동 정리되므로 별도 정리 불필요 */
  crypto_initialized = 0;
}

int hash_password_sha256(const char* password, char* hash_output) {
  if (!crypto_initialized) {
    fprintf(stderr, "[CRYPTO] Crypto system not initialized\n");
    return 0;
  }

  if (!password || !hash_output) {
    return 0;
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) {
    return 0;
  }

  unsigned char hash[SHA256_DIGEST_LENGTH];
  unsigned int hash_len = 0;

  if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 || EVP_DigestUpdate(ctx, password, strlen(password)) != 1 ||
      EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
    EVP_MD_CTX_free(ctx);
    return 0;
  }

  EVP_MD_CTX_free(ctx);

  if (hash_len != SHA256_DIGEST_LENGTH) {
    return 0;
  }

  bytes_to_hex(hash, SHA256_DIGEST_LENGTH, hash_output);
  return 1;
}