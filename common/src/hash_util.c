// ===============================
// �� ������ ���ڿ��� SHA-256 �ؽ÷� ��ȯ�ϴ� �Լ��� ������ �����մϴ�.
// OpenSSL�� SHA256 �Լ��� ����Ͽ� 64�ڸ� 16���� �ؽð��� �����մϴ�.

#include "hash_util.h"
#include <openssl/sha.h>
#include <string.h>
#include <stdio.h>

// compute_sha256
// �Է� ���ڿ��� �޾� SHA-256 �ؽø� ����Ͽ� ��� ���ڿ��� ��ȯ�մϴ�.
// ����� output_hex�� ����Ǹ� �ݵ�� 65����Ʈ �̻��̾�� �մϴ�.
void compute_sha256(const char* input, char* output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];  // 32����Ʈ �ؽ� ��� ����

    // �Է� ���ڿ��� ���� SHA-256 �ؽ� ���
    SHA256((const unsigned char*)input, strlen(input), hash);

    // �ؽ� ����� ��� ���ڿ��� ��ȯ
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    }

    output_hex[64] = '\0';  // �� ���� ���� �߰�
}