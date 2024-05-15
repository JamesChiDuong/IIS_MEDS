#ifndef API_H
#define API_H

#define CRYPTO_SECRETKEYBYTES 2416
#define CRYPTO_PUBLICKEYBYTES 13220
#define CRYPTO_BYTES 12976

#define CRYPTO_ALGNAME "MEDS13220"

int crypto_sign_keypair(
    unsigned char *pk,
    unsigned char *sk
  );

int crypto_sign(
    unsigned char *sm, unsigned long *smlen,
    const unsigned char *m, unsigned long mlen,
    const unsigned char *sk
  );

int crypto_sign_open(
    unsigned char *m, unsigned long *mlen,
    const unsigned char *sm, unsigned long smlen,
    const unsigned char *pk
  );
#endif

