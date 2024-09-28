#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

void sha256(const char *str, unsigned char outputBuffer[SHA256_DIGEST_LENGTH]) {
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str, strlen(str));
    SHA256_Final(outputBuffer, &sha256);
}

void printHash(unsigned char hash[SHA256_DIGEST_LENGTH]) {
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

int main() {
    const char *input = "I love you, Tam An";
    unsigned char hash[SHA256_DIGEST_LENGTH];

    sha256(input, hash);

    printf("SHA-256 of \"%s\" is: ", input);
    printHash(hash);

    return 0;
}
