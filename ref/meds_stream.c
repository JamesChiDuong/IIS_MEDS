#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#include "fips202.h"

#include "params.h"

#include "api.h"
#include "randombytes.h"

#include "meds.h"

#include "seed.h"
#include "util.h"
#include "bitstream.h"

#include "matrixmod.h"

#define CEILING(x, y) (((x) + (y) - 1) / (y))
/*Key generation*/
#ifdef UNIX
unsigned char DEBUG_pk[CRYPTO_PUBLICKEYBYTES], DEBUG_sk[CRYPTO_SECRETKEYBYTES];
pmod_mat_t *A_inv[MEDS_s];
pmod_mat_t *B_inv[MEDS_s];
pmod_mat_t *G[MEDS_s];
int crypto_sign_keypair_DEBUG(uint8_t *delta, pmod_mat_t *A_inv[MEDS_s], pmod_mat_t *B_inv[MEDS_s], pmod_mat_t *G[MEDS_s]);
int crypto_sign_keypair_DEBUG(uint8_t *delta, pmod_mat_t *A_inv[MEDS_s], pmod_mat_t *B_inv[MEDS_s], pmod_mat_t *G[MEDS_s])
{

  pmod_mat_t G_data[MEDS_k * MEDS_m * MEDS_n * MEDS_s];

  for (int i = 0; i < MEDS_s; i++)
    G[i] = &G_data[i * MEDS_k * MEDS_m * MEDS_n];

  uint8_t sigma_G0[MEDS_pub_seed_bytes];
  uint8_t sigma[MEDS_sec_seed_bytes];

  XOF((uint8_t *[]){sigma_G0, sigma},
      (size_t[]){MEDS_pub_seed_bytes, MEDS_sec_seed_bytes},
      delta, MEDS_sec_seed_bytes,
      2);

  LOG_VEC(sigma, MEDS_sec_seed_bytes);
  LOG_VEC_FMT(sigma_G0, MEDS_pub_seed_bytes, "sigma_G0");

  rnd_sys_mat(G[0], MEDS_k, MEDS_m * MEDS_n, sigma_G0, MEDS_pub_seed_bytes);

  LOG_MAT(G[0], MEDS_k, MEDS_m * MEDS_n);

  pmod_mat_t A_inv_data[MEDS_s * MEDS_m * MEDS_m];
  pmod_mat_t B_inv_data[MEDS_s * MEDS_m * MEDS_m];

  for (int i = 0; i < MEDS_s; i++)
  {
    A_inv[i] = &A_inv_data[i * MEDS_m * MEDS_m];
    B_inv[i] = &B_inv_data[i * MEDS_n * MEDS_n];
  }

  for (int i = 1; i < MEDS_s; i++)
  {
    pmod_mat_t A[MEDS_m * MEDS_m] = {0};
    pmod_mat_t B[MEDS_n * MEDS_n] = {0};

    while (1 == 1) // redo generation for this index until success
    {
      uint8_t sigma_Ti[MEDS_sec_seed_bytes];
      uint8_t sigma_a[MEDS_sec_seed_bytes];

      XOF((uint8_t *[]){sigma_a, sigma_Ti, sigma},
          (size_t[]){MEDS_sec_seed_bytes, MEDS_sec_seed_bytes, MEDS_sec_seed_bytes},
          sigma, MEDS_sec_seed_bytes,
          3);

      pmod_mat_t Ti[MEDS_k * MEDS_k];

      rnd_inv_matrix(Ti, MEDS_k, MEDS_k, sigma_Ti, MEDS_sec_seed_bytes);

      GFq_t Amm;

      {
        keccak_state Amm_shake;
        shake256_absorb_once(&Amm_shake, sigma_a, MEDS_sec_seed_bytes);

        Amm = rnd_GF(&Amm_shake);
      }

      LOG_MAT(Ti, MEDS_k, MEDS_k);
      LOG_VAL(Amm);

      pmod_mat_t G0prime[MEDS_k * MEDS_m * MEDS_n];

      pmod_mat_mul(G0prime, MEDS_k, MEDS_m * MEDS_n,
                   Ti, MEDS_k, MEDS_k,
                   G[0], MEDS_k, MEDS_m * MEDS_n);

      LOG_MAT(G0prime, MEDS_k, MEDS_m * MEDS_n);

      if (solve(A, B_inv[i], G0prime, Amm) < 0)
      {
        LOG("no sol");
        continue;
      }

      if (pmod_mat_inv(B, B_inv[i], MEDS_n, MEDS_n) < 0)
      {
        LOG("no inv B");
        continue;
      }

      if (pmod_mat_inv(A_inv[i], A, MEDS_m, MEDS_m) < 0)
      {
        LOG("no inv A_inv");
        continue;
      }

      LOG_MAT_FMT(A, MEDS_m, MEDS_m, "A[%i]", i);
      LOG_MAT_FMT(A_inv[i], MEDS_m, MEDS_m, "A_inv[%i]", i);
      LOG_MAT_FMT(B, MEDS_n, MEDS_n, "B[%i]", i);
      LOG_MAT_FMT(B_inv[i], MEDS_n, MEDS_n, "B_inv[%i]", i);

      pi(G[i], A, B, G[0]);

      if (pmod_mat_syst_ct(G[i], MEDS_k, MEDS_m * MEDS_n) != 0)
      {
        LOG("redo G[%i]", i);
        continue; // Not systematic; try again for index i.
      }

      LOG_MAT_FMT(G[i], MEDS_k, MEDS_m * MEDS_n, "G[%i]", i);

      // successfull generated G[s]; break out of while loop
      break;
    }
  }

  // copy pk data
  {
    uint8_t *tmp_pk = DEBUG_pk;

    memcpy(tmp_pk, sigma_G0, MEDS_pub_seed_bytes);
    LOG_VEC(tmp_pk, MEDS_pub_seed_bytes, "sigma_G0 (pk)");
    tmp_pk += MEDS_pub_seed_bytes;

    bitstream_t bs;

    bs_init(&bs, tmp_pk, MEDS_PK_BYTES - MEDS_pub_seed_bytes);

    for (int si = 1; si < MEDS_s; si++)
    {
      for (int j = (MEDS_m - 1) * MEDS_n; j < MEDS_m * MEDS_n; j++)
        bs_write(&bs, G[si][MEDS_m * MEDS_n + j], GFq_bits);

      for (int r = 2; r < MEDS_k; r++)
        for (int j = MEDS_k; j < MEDS_m * MEDS_n; j++)
          bs_write(&bs, G[si][r * MEDS_m * MEDS_n + j], GFq_bits);

      bs_finalize(&bs);
    }

    LOG_VEC(tmp_pk, MEDS_PK_BYTES - MEDS_pub_seed_bytes, "G[1:] (pk)");
    tmp_pk += MEDS_PK_BYTES - MEDS_pub_seed_bytes;

    LOG_HEX(DEBUG_pk, MEDS_PK_BYTES);

    if (MEDS_PK_BYTES != MEDS_pub_seed_bytes + bs.byte_pos + (bs.bit_pos > 0 ? 1 : 0))
    {
      fprintf(stderr, "ERROR: MEDS_PK_BYTES and actual pk size do not match! %i vs %i\n", MEDS_PK_BYTES, MEDS_pub_seed_bytes + bs.byte_pos + (bs.bit_pos > 0 ? 1 : 0));
      fprintf(stderr, "%i %i\n", MEDS_pub_seed_bytes + bs.byte_pos, MEDS_pub_seed_bytes + bs.byte_pos + (bs.bit_pos > 0 ? 1 : 0));
      return -1;
    }
  }

  // copy sk data
  {
    memcpy(DEBUG_sk, delta, MEDS_sec_seed_bytes);
    memcpy(DEBUG_sk + MEDS_sec_seed_bytes, sigma_G0, MEDS_pub_seed_bytes);

    bitstream_t bs;

    bs_init(&bs, DEBUG_sk + MEDS_sec_seed_bytes + MEDS_pub_seed_bytes, MEDS_SK_BYTES - MEDS_sec_seed_bytes - MEDS_pub_seed_bytes);

    for (int si = 1; si < MEDS_s; si++)
    {
      for (int j = 0; j < MEDS_m * MEDS_m; j++)
        bs_write(&bs, A_inv[si][j], GFq_bits);

      bs_finalize(&bs);
    }

    for (int si = 1; si < MEDS_s; si++)
    {
      for (int j = 0; j < MEDS_n * MEDS_n; j++)
        bs_write(&bs, B_inv[si][j], GFq_bits);

      bs_finalize(&bs);
    }

    LOG_HEX(DEBUG_sk, MEDS_SK_BYTES);
  }

  return 0;
}
#endif

int crypto_sign_keypair_streaming(unsigned char *pk, unsigned char *sk)
{
  // crypto_sign_keypair_DEBUG();
  uint8_t delta[MEDS_sec_seed_bytes];
  /****Create delta******/
  randombytes(delta, MEDS_sec_seed_bytes);
  // crypto_sign_keypair_DEBUG(delta,A_inv,B_inv,G);
  uint8_t sigma_G0[MEDS_pub_seed_bytes];
  uint8_t sigma[MEDS_sec_seed_bytes];

  // create sigma
  XOF((uint8_t *[]){sigma_G0, sigma},
      (size_t[]){MEDS_pub_seed_bytes, MEDS_sec_seed_bytes},
      delta, MEDS_sec_seed_bytes, 2);

  LOG_VEC(sigma, MEDS_sec_seed_bytes);
  LOG_VEC_FMT(sigma_G0, MEDS_pub_seed_bytes, "sigma_G0");

  /****Create G0******/
#if defined(MEDS167717) || defined(MEDS134180) // MEDS167717 need to send G0 back to reduce memory

  // store into G_data and G0
  pmod_mat_t G_data[MEDS_k * MEDS_m * MEDS_n];
  rnd_sys_mat(G_data, MEDS_k, MEDS_m * MEDS_n, sigma_G0, MEDS_pub_seed_bytes);
  LOG_MAT(G_data, MEDS_k, MEDS_m * MEDS_n);

  // Send Go out
  hal_putchar(0);
  write_stream(G_data, MEDS_k * MEDS_m * MEDS_n); // Send G0
  hal_getchar();

#else // no need to send G0
  pmod_mat_t G0[MEDS_k * MEDS_m * MEDS_n];     // Store G0
  pmod_mat_t G_data[MEDS_k * MEDS_m * MEDS_n]; // Store Gi
  // Create G0
  rnd_sys_mat(G0, MEDS_k, MEDS_m * MEDS_n, sigma_G0, MEDS_pub_seed_bytes);
  LOG_MAT(G0, MEDS_k, MEDS_m * MEDS_n);
#endif
  // Initialze A_in and B_inv data
  pmod_mat_t A_inv_data[MEDS_m * MEDS_m];
  pmod_mat_t B_inv_data[MEDS_m * MEDS_m];

  /****Forall******/
  for (int i = 1; i < MEDS_s; i++)
  {
    pmod_mat_t A[MEDS_m * MEDS_m] = {0};
    pmod_mat_t B[MEDS_n * MEDS_n] = {0};

    while (1 == 1)
    {
      uint8_t sigma_Ti[MEDS_sec_seed_bytes];
      uint8_t sigma_a[MEDS_sec_seed_bytes];

      /****create signma_a, sigma_Ti,sigma******/
      XOF((uint8_t *[]){sigma_a, sigma_Ti, sigma},
          (size_t[]){MEDS_sec_seed_bytes, MEDS_sec_seed_bytes, MEDS_sec_seed_bytes},
          sigma, MEDS_sec_seed_bytes,
          3);

      /****create Ti******/
      pmod_mat_t Ti[MEDS_k * MEDS_k];
      rnd_inv_matrix(Ti, MEDS_k, MEDS_k, sigma_Ti, MEDS_sec_seed_bytes);

      /****create Amm******/
      GFq_t Amm;
      {
        keccak_state Amm_shake;
        shake256_absorb_once(&Amm_shake, sigma_a, MEDS_sec_seed_bytes);

        Amm = rnd_GF(&Amm_shake);
      }
      LOG_MAT(Ti, MEDS_k, MEDS_k);
      LOG_VAL(Amm);

#if defined(MEDS167717) || defined(MEDS134180)
      /****create Gprime from G0 and store into G_data******/
      GFq_t *tmp = (GFq_t *)calloc(MEDS_k * MEDS_m * MEDS_n, sizeof(GFq_t)); // create tmp buffer
      pmod_mat_mul_revise(tmp, MEDS_k, MEDS_m * MEDS_n,
                          Ti, MEDS_k, MEDS_k,
                          G_data, MEDS_k, MEDS_m * MEDS_n); // Gprime here only G0 when i = 1, with another i, Gprime is G, Read G0?
      for (int c = 0; c < MEDS_m * MEDS_n; c++)
      {
        for (int r = 0; r < MEDS_k; r++)
        {
          pmod_mat_set_entry(G_data, MEDS_k, MEDS_m * MEDS_n, r, c, tmp[r * MEDS_m * MEDS_n + c]);
        }
      }
      LOG_MAT(G_data, MEDS_k, MEDS_m * MEDS_n); // create Gprime into G_data
      free(tmp);                                // deinit tmp buffer

      /****create A, B_inv_data from Gprime store into G_data******/
      if (solve(A, B_inv_data, G_data, Amm) < 0)
      {
        LOG("no sol");
        continue;
      }
      if (pmod_mat_inv(B, B_inv_data, MEDS_n, MEDS_n) < 0)
      {
        LOG("no inv B");
        continue;
      }

      if (pmod_mat_inv(A_inv_data, A, MEDS_m, MEDS_m) < 0)
      {
        LOG("no inv A_inv");
        continue;
      }
      LOG_MAT_FMT(A, MEDS_m, MEDS_m, "A[%i]", i);
      LOG_MAT_FMT(A_inv_data[i], MEDS_m, MEDS_m, "A_inv[%i]", i);
      LOG_MAT_FMT(B, MEDS_n, MEDS_n, "B[%i]", i);
      LOG_MAT_FMT(B_inv_data[i], MEDS_n, MEDS_n, "B_inv[%i]", i);

      /****create G_data from G0******/
      hal_putchar(0); // send start to reading

      // Read G0 back
      read_stream(G_data, MEDS_k * MEDS_m * MEDS_n);

      pi(G_data, A, B, G_data);                                   // Create G_Data from G0
      if (pmod_mat_syst_ct(G_data, MEDS_k, MEDS_m * MEDS_n) != 0) // Do with G_data
      {
        LOG("redo G[%i]", i);
        continue; // Not systematic; try again for index i.
      }
      LOG_MAT_FMT(G_data, MEDS_k, MEDS_m * MEDS_n, "G[%i]", i);

      // Send A,B matrix
      write_stream(A_inv_data, MEDS_m * MEDS_m);

      write_stream(B_inv_data, MEDS_m * MEDS_m);
      // Send G_data
      write_stream(G_data, MEDS_k * MEDS_m * MEDS_n);

      // Read G0 back for recalculating Gprime
      read_stream(G_data, MEDS_k * MEDS_m * MEDS_n);

      hal_getchar(); // receive data for done
      // successfull generated G[s] and send out to python scripts; break out of while loop
      break;
#else

      // Create G0prime
      pmod_mat_t Gprime[MEDS_k * MEDS_m * MEDS_n];

      pmod_mat_mul(Gprime, MEDS_k, MEDS_m * MEDS_n,
                   Ti, MEDS_k, MEDS_k,
                   G0, MEDS_k, MEDS_m * MEDS_n); // Create Gprime from G0

      LOG_MAT(Gprime, MEDS_k, MEDS_m * MEDS_n); // store into Gprime

      /****create A, B_inv_data from Gpime******/
      if (solve(A, B_inv_data, Gprime, Amm) < 0)
      {
        LOG("no sol");
        continue;
      }
      if (pmod_mat_inv(B, B_inv_data, MEDS_n, MEDS_n) < 0)
      {
        LOG("no inv B");
        continue;
      }

      if (pmod_mat_inv(A_inv_data, A, MEDS_m, MEDS_m) < 0)
      {
        LOG("no inv A_inv");
        continue;
      }
      LOG_MAT_FMT(A, MEDS_m, MEDS_m, "A[%i]", i);
      LOG_MAT_FMT(A_inv_data[i], MEDS_m, MEDS_m, "A_inv[%i]", i);
      LOG_MAT_FMT(B, MEDS_n, MEDS_n, "B[%i]", i);
      LOG_MAT_FMT(B_inv_data[i], MEDS_n, MEDS_n, "B_inv[%i]", i);

      /****create G_data from G0******/
      hal_putchar(0); // send start to reading

      pi(G_data, A, B, G0);
      if (pmod_mat_syst_ct(G_data, MEDS_k, MEDS_m * MEDS_n) != 0)
      {
        LOG("redo G[%i]", i);
        continue; // Not systematic; try again for index i.
      }
      LOG_MAT_FMT(Gprime, MEDS_k, MEDS_m * MEDS_n, "G[%i]", i);

      // Send A,B matrix
      write_stream(A_inv_data, MEDS_m * MEDS_m);

      write_stream(B_inv_data, MEDS_m * MEDS_m);
      // Send G_data
      write_stream(G_data, MEDS_k * MEDS_m * MEDS_n);

      hal_getchar(); // receive data for done
      // successfull generated G[s] and send out to python scripts; break out of while loop
      break;
#endif
    }
  }

  /*******Create sk data****/
  sk = (unsigned char *)calloc(MEDS_SK_BYTES, sizeof(unsigned char));
  memcpy(sk, delta, MEDS_sec_seed_bytes);
  memcpy(sk + MEDS_sec_seed_bytes, sigma_G0, MEDS_pub_seed_bytes);

  bitstream_t bs;

  bs_init(&bs, sk + MEDS_sec_seed_bytes + MEDS_pub_seed_bytes, MEDS_SK_BYTES - MEDS_sec_seed_bytes - MEDS_pub_seed_bytes);

  for (int si = 1; si < MEDS_s; si++)
  {
    // Send start to read
    hal_putchar(0);
    // Read A_in_data
    read_stream(A_inv_data, MEDS_m * MEDS_m);
    for (int j = 0; j < MEDS_m * MEDS_m; j++)
    {
      bs_write(&bs, A_inv_data[j], GFq_bits);
    }
    bs_finalize(&bs);
    // Finish reading
    hal_getchar();
  }

  for (int si = 1; si < MEDS_s; si++)
  {
    // Send start to read
    hal_putchar(0);
    // Read B_in_data
    read_stream(B_inv_data, MEDS_m * MEDS_m);
    for (int j = 0; j < MEDS_n * MEDS_n; j++)
    {
      bs_write(&bs, B_inv_data[j], GFq_bits);
    }
    bs_finalize(&bs);
    // Finish read data
    hal_getchar();
  }
  LOG_HEX(sk, MEDS_SK_BYTES);

  // Send start to read
  hal_putchar(0);
  write_stream_str("sk = ", sk, MEDS_SK_BYTES);
  // Finish read data
  hal_getchar();

  // Deinit memory
  free(sk);
  // Send PK
  pk = (uint8_t *)calloc(2 * (MEDS_k * MEDS_m * MEDS_n), sizeof(uint8_t)); // 167717 byte => declare 27000 byte -16bit and 54000 - 8 bit
  uint32_t byte_pos_check = 0;
  uint8_t *tmp_pk = pk;

  memcpy(tmp_pk, sigma_G0, MEDS_pub_seed_bytes); // copy pub_seed at first 32 bytes

  LOG_VEC(tmp_pk, MEDS_pub_seed_bytes, "sigma_G0 (pk)");

  tmp_pk += MEDS_pub_seed_bytes; // Increase next pub_seed byte

  for (int si = 1; si < MEDS_s; si++)
  {
    bs_init(&bs, tmp_pk, (2 * (MEDS_k * MEDS_m * MEDS_n) - MEDS_pub_seed_bytes)); // Init with 167717-32 byte first => Declare (27000 - 32)*6
    // Send start to read
    hal_putchar(0);
    // Read G_data
    read_stream(G_data, MEDS_k * MEDS_m * MEDS_n);

    // Total (28 iterations × 870 iterations × 11 bits + 30 iterations × 11 bits)/8 = 33537 bytes
    for (int j = (MEDS_m - 1) * MEDS_n; j < MEDS_m * MEDS_n; j++)
    {
      bs_write(&bs, G_data[MEDS_m * MEDS_n + j], GFq_bits);
    }
    for (int r = 2; r < MEDS_k; r++)
    {
      for (int j = MEDS_k; j < MEDS_m * MEDS_n; j++)
      {
        bs_write(&bs, G_data[r * MEDS_m * MEDS_n + j], GFq_bits);
      }
    }
    bs_finalize(&bs);
    byte_pos_check += bs.byte_pos;
    if (si == 1)
    {
      write_stream_str("", pk, (bs.byte_pos + MEDS_pub_seed_bytes)); // print with MEDS_pub_seed_bytes data
    }
    else
    {
      write_stream_str("", pk, (bs.byte_pos));
    }
    memset(pk, 0x00, (2 * (MEDS_k * MEDS_m * MEDS_n)));
    tmp_pk = pk; // Start at begining of Pk buffer
    // Finish read data
    hal_getchar();
  }
  if (MEDS_PK_BYTES != MEDS_pub_seed_bytes + byte_pos_check + (bs.bit_pos > 0 ? 1 : 0))
  {
    printf("ERROR: MEDS_PK_BYTES and actual pk size do not match! %i vs %i\n", MEDS_PK_BYTES, MEDS_pub_seed_bytes + bs.byte_pos + (bs.bit_pos > 0 ? 1 : 0));
    printf("%i %i\n", MEDS_pub_seed_bytes + bs.byte_pos, MEDS_pub_seed_bytes + bs.byte_pos + (bs.bit_pos > 0 ? 1 : 0));
    return -1;
  }
  free(pk);

  return 0;
}

int crypto_sign(
    unsigned char *sm, unsigned long *smlen,
    const unsigned char *m, unsigned long mlen,
    const unsigned char *sk)
{
  uint8_t delta[MEDS_sec_seed_bytes];

  randombytes(delta, MEDS_sec_seed_bytes);

  // Initialze and Read sk data
  sk = (unsigned char *)calloc(MEDS_SK_BYTES, sizeof(unsigned char));
  uint8_t *tmp_sk = sk;
  // Send start to read
  hal_putchar(0);
  read_stream_str(sk, MEDS_SK_BYTES);
  hal_getchar();
  // Finish reading

  // skip secret seed
  tmp_sk += MEDS_sec_seed_bytes;

#if defined(MEDS167717) || defined(MEDS134180)
  pmod_mat_t G_tilde_ti[MEDS_k * MEDS_m * MEDS_n]; // create buffer to store G0 and G_tilde_ti
  // Store G_0
  rnd_sys_mat(G_tilde_ti, MEDS_k, MEDS_m * MEDS_n, tmp_sk, MEDS_pub_seed_bytes); // Store G0 into G_ti
#else
  pmod_mat_t G0_tilde[MEDS_k * MEDS_m * MEDS_n];   // create G_0
  pmod_mat_t G_tilde_ti[MEDS_k * MEDS_m * MEDS_n]; // create G_tilde_ti
  // Store G_0
  rnd_sys_mat(G0_tilde, MEDS_k, MEDS_m * MEDS_n, tmp_sk, MEDS_pub_seed_bytes);
#endif

  tmp_sk += MEDS_pub_seed_bytes;

  pmod_mat_t A_inv_data[MEDS_s * MEDS_m * MEDS_m];
  pmod_mat_t B_inv_data[MEDS_s * MEDS_n * MEDS_n];

  pmod_mat_t *A_inv[MEDS_s];
  pmod_mat_t *B_inv[MEDS_s];

  for (int i = 0; i < MEDS_s; i++)
  {
    A_inv[i] = &A_inv_data[i * MEDS_m * MEDS_m];
    B_inv[i] = &B_inv_data[i * MEDS_n * MEDS_n];
  }

  // Load secret key matrices.
  {
    bitstream_t bs;

    bs_init(&bs, (uint8_t *)tmp_sk, MEDS_SK_BYTES - MEDS_sec_seed_bytes - MEDS_pub_seed_bytes);

    for (int si = 1; si < MEDS_s; si++)
    {
      for (int j = 0; j < MEDS_m * MEDS_m; j++)
        A_inv[si][j] = bs_read(&bs, GFq_bits);

      bs_finalize(&bs);
    }

    for (int si = 1; si < MEDS_s; si++)
    {
      for (int j = 0; j < MEDS_n * MEDS_n; j++)
        B_inv[si][j] = bs_read(&bs, GFq_bits);

      bs_finalize(&bs);
    }

    bs_finalize(&bs);
  }

  for (int i = 1; i < MEDS_s; i++)
    LOG_MAT(A_inv[i], MEDS_m, MEDS_m);

  for (int i = 1; i < MEDS_s; i++)
    LOG_MAT(B_inv[i], MEDS_n, MEDS_n);

#if defined(MEDS167717) || defined(MEDS134180)
  LOG_MAT(G_tilde_ti, MEDS_k, MEDS_m * MEDS_m); /// Log G0
#else
  LOG_MAT(G0_tilde, MEDS_k, MEDS_m * MEDS_m); /// Log G0
#endif
  LOG_VEC(delta, MEDS_sec_seed_bytes);

  uint8_t stree[MEDS_st_seed_bytes * SEED_TREE_size] = {0};
  uint8_t alpha[MEDS_st_salt_bytes];

  uint8_t *rho = &stree[MEDS_st_seed_bytes * SEED_TREE_ADDR(0, 0)];

  XOF((uint8_t *[]){rho, alpha},
      (size_t[]){MEDS_st_seed_bytes, MEDS_st_salt_bytes},
      delta, MEDS_sec_seed_bytes,
      2);

  t_hash(stree, alpha, 0, 0);

  uint8_t *sigma = &stree[MEDS_st_seed_bytes * SEED_TREE_ADDR(MEDS_seed_tree_height, 0)];

  for (int i = 0; i < MEDS_t; i++)
  {
    LOG_HEX_FMT((&sigma[i * MEDS_st_seed_bytes]), MEDS_st_seed_bytes, "sigma[%i]", i);
  }

  pmod_mat_t A_tilde_data[MEDS_m * MEDS_m];
  pmod_mat_t B_tilde_data[MEDS_m * MEDS_m];

  // pmod_mat_t G_tilde_ti[MEDS_k * MEDS_m * MEDS_m];
  uint8_t seed_buf[MEDS_st_salt_bytes + MEDS_st_seed_bytes + sizeof(uint32_t)] = {0};
  memcpy(seed_buf, alpha, MEDS_st_salt_bytes);

  uint8_t *addr_pos = seed_buf + MEDS_st_salt_bytes + MEDS_st_seed_bytes;

  keccak_state h_shake;
  shake256_init(&h_shake);

#if defined(MEDS167717) || defined(MEDS134180) // Only MEDS167717 send G0
  // Send Go out
  hal_putchar(0);

  write_stream(G_tilde_ti, MEDS_k * MEDS_m * MEDS_n); // G_ti store G0 data

  hal_getchar();
#endif
  for (int i = 0; i < MEDS_t; i++)
  {

    while (1 == 1)
    {
      uint8_t sigma_A_tilde_i[MEDS_pub_seed_bytes];
      uint8_t sigma_B_tilde_i[MEDS_pub_seed_bytes];

      for (int j = 0; j < 4; j++)
        addr_pos[j] = (i >> (j * 8)) & 0xff;

      memcpy(seed_buf + MEDS_st_salt_bytes, &sigma[i * MEDS_st_seed_bytes], MEDS_st_seed_bytes);

      LOG_HEX_FMT(seed_buf, MEDS_st_salt_bytes + MEDS_st_seed_bytes + 4, "sigma_prime[%i]", i);

      XOF((uint8_t *[]){sigma_A_tilde_i, sigma_B_tilde_i, &sigma[i * MEDS_st_seed_bytes]},
          (size_t[]){MEDS_pub_seed_bytes, MEDS_pub_seed_bytes, MEDS_st_seed_bytes},
          seed_buf, MEDS_st_salt_bytes + MEDS_st_seed_bytes + 4,
          3);

      LOG_HEX_FMT(sigma_A_tilde_i, MEDS_pub_seed_bytes, "sigma_A_tilde[%i]", i);
      rnd_inv_matrix(A_tilde_data, MEDS_m, MEDS_m, sigma_A_tilde_i, MEDS_pub_seed_bytes);

      LOG_HEX_FMT(sigma_B_tilde_i, MEDS_pub_seed_bytes, "sigma_B_tilde[%i]", i);
      rnd_inv_matrix(B_tilde_data, MEDS_n, MEDS_n, sigma_B_tilde_i, MEDS_pub_seed_bytes);

      LOG_MAT_FMT(A_tilde_data, MEDS_m, MEDS_m, "A_tilde[%i]", i);
      LOG_MAT_FMT(B_tilde_data, MEDS_n, MEDS_n, "B_tilde[%i]", i);
#if defined(MEDS167717) || defined(MEDS134180)
      // Store G to G_tilde_ti buffer with G_0 is input.
      pi(G_tilde_ti, A_tilde_data, B_tilde_data, G_tilde_ti);
#else
      pi(G_tilde_ti, A_tilde_data, B_tilde_data, G0_tilde); // Create G_tilde from G0
#endif
      LOG_MAT_FMT(G_tilde_ti, MEDS_k, MEDS_m * MEDS_n, "G_tilde[%i]", i);
      if (pmod_mat_syst_ct(G_tilde_ti, MEDS_k, MEDS_m * MEDS_n) == 0) // doing with G_tilde_ti data
      {
        break;
      }
    }

    // Caculating G_tile data
    LOG_MAT_FMT(G_tilde_ti, MEDS_k, MEDS_m * MEDS_n, "G_tilde[%i]", i);

    bitstream_t bs;
    uint8_t *bs_buf = (uint8_t *)calloc(CEILING((MEDS_k * (MEDS_m * MEDS_n - MEDS_k)) * GFq_bits, 8), sizeof(uint8_t));
    bs_init(&bs, bs_buf, CEILING((MEDS_k * (MEDS_m * MEDS_n - MEDS_k)) * GFq_bits, 8));

    for (int r = 0; r < MEDS_k; r++)
      for (int j = MEDS_k; j < MEDS_m * MEDS_n; j++)
        bs_write(&bs, G_tilde_ti[r * MEDS_m * MEDS_n + j], GFq_bits);

    shake256_absorb(&h_shake, bs_buf, CEILING((MEDS_k * (MEDS_m * MEDS_n - MEDS_k)) * GFq_bits, 8));

    // Read G0 back for recalculating Gprime
    hal_putchar(0);
    // Send A,B matrix
    write_stream(A_tilde_data, MEDS_m * MEDS_m);

    write_stream(B_tilde_data, MEDS_m * MEDS_m);

#if defined(MEDS167717) || defined(MEDS134180) // only MEDS167717 and MEDS134180 read G0 back
    read_stream(G_tilde_ti, MEDS_k * MEDS_m * MEDS_n);
#endif
    hal_getchar();
    free(bs_buf);
  }

  shake256_absorb(&h_shake, (uint8_t *)m, mlen);

  shake256_finalize(&h_shake);

  uint8_t digest[MEDS_digest_bytes];

  shake256_squeeze(digest, MEDS_digest_bytes, &h_shake);

  LOG_VEC(digest, MEDS_digest_bytes);

  uint8_t h[MEDS_t];

  parse_hash(digest, MEDS_digest_bytes, h, MEDS_t);

  LOG_VEC(h, MEDS_t);

  bitstream_t bs;

  // Initialze the SM memory

  // Data_length for path data
  uint32_t data_length = MEDS_SIG_BYTES - MEDS_w * (CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8));
  sm = (unsigned char *)calloc(((CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8)) + data_length + mlen), sizeof(unsigned char));

  for (int i = 0; i < MEDS_t; i++)
  {
    hal_putchar(0);
    // Read A_tilde_data and B_tilde_data
    read_stream(A_tilde_data, MEDS_m * MEDS_m);

    read_stream(B_tilde_data, MEDS_m * MEDS_m);

    bs_init(&bs, sm, ((CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8)))); // Total

    if (h[i] > 0)
    {
      {
        pmod_mat_t mu[MEDS_m * MEDS_m];

        pmod_mat_mul(mu, MEDS_m, MEDS_m, A_tilde_data, MEDS_m, MEDS_m, A_inv[h[i]], MEDS_m, MEDS_m);
        LOG_MAT(mu, MEDS_m, MEDS_m);

        for (int j = 0; j < MEDS_m * MEDS_m; j++)
          bs_write(&bs, mu[j], GFq_bits);
      }

      bs_finalize(&bs);

      {
        pmod_mat_t nu[MEDS_n * MEDS_n];

        pmod_mat_mul(nu, MEDS_n, MEDS_n, B_inv[h[i]], MEDS_n, MEDS_n, B_tilde_data, MEDS_n, MEDS_n);

        LOG_MAT(nu, MEDS_n, MEDS_n);

        for (int j = 0; j < MEDS_n * MEDS_n; j++)
          bs_write(&bs, nu[j], GFq_bits);
      }
      bs_finalize(&bs);
    }
    {
      write_stream_str("", sm, (bs.byte_pos));
      memset(sm, 0x00, ((CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8))));
    }
    hal_getchar();
  }

  uint8_t *path = sm + (CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8)); // assign sm to path

  t_hash(stree, alpha, 0, 0);

  stree_to_path(stree, h, path, alpha); // GOt data from path

  // sm + path data
  memcpy(sm + (CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8)) + data_length - MEDS_digest_bytes - MEDS_st_salt_bytes, digest, MEDS_digest_bytes);
  memcpy(sm + (CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8)) + data_length - MEDS_st_salt_bytes, alpha, MEDS_st_salt_bytes);
  memcpy(sm + (CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8)) + data_length, m, mlen);

  *smlen = MEDS_SIG_BYTES + mlen;

  hal_putchar(0);

  write_stream_str("", sm + (CEILING(MEDS_m * MEDS_m * GFq_bits, 8) + CEILING(MEDS_n * MEDS_n * GFq_bits, 8)), data_length + mlen);

  hal_getchar();
  LOG_HEX(sm, MEDS_SIG_BYTES + mlen);

  free(sk);
  free(sm);
  return 0;
}