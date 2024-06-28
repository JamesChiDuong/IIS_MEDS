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

#define CEILING(x,y) (((x) + (y) - 1) / (y))

/*Key generation*/
int crypto_sign_keypair_streaming(unsigned char *pk, unsigned char *sk)
{
  uint8_t delta[MEDS_sec_seed_bytes];
  /****Create delta******/
  randombytes(delta, MEDS_sec_seed_bytes);

  uint8_t sigma_G0[MEDS_pub_seed_bytes];
  uint8_t sigma[MEDS_sec_seed_bytes];

  //create sigma
  XOF((uint8_t*[]){sigma_G0, sigma},
    (size_t[]){MEDS_pub_seed_bytes, MEDS_sec_seed_bytes},
    delta, MEDS_sec_seed_bytes,2);

  LOG_VEC(sigma, MEDS_sec_seed_bytes);
  LOG_VEC_FMT(sigma_G0, MEDS_pub_seed_bytes, "sigma_G0");

  /****Create G0******/
  //store into Main_Buffer
  pmod_mat_t Main_Buffer[MEDS_k * MEDS_m * MEDS_n];
  rnd_sys_mat(Main_Buffer, MEDS_k, MEDS_m*MEDS_n, sigma_G0, MEDS_pub_seed_bytes);
  LOG_MAT(Main_Buffer, MEDS_k, MEDS_m*MEDS_n);
  //Send Go out
  hal_getchar();
  write_stream(Main_Buffer,MEDS_k * MEDS_m * MEDS_n);
  hal_putchar(0);


  pmod_mat_t *A_inv_data = (pmod_mat_t*)calloc(MEDS_m * MEDS_m, sizeof(pmod_mat_t));
  pmod_mat_t *B_inv_data = (pmod_mat_t*)calloc(MEDS_m * MEDS_m, sizeof(pmod_mat_t));

  /****Forall******/
  for (int i = 1; i < MEDS_s; i++)
  {
    pmod_mat_t *A = (pmod_mat_t*)calloc(MEDS_m * MEDS_m, sizeof(pmod_mat_t));
    pmod_mat_t *B = (pmod_mat_t*)calloc(MEDS_n * MEDS_n, sizeof(pmod_mat_t));
    while(1==1)
    {
      uint8_t sigma_Ti[MEDS_sec_seed_bytes];
      uint8_t sigma_a[MEDS_sec_seed_bytes];

      /****create signma_a, sigma_Ti,sigma******/
      XOF((uint8_t*[]){sigma_a, sigma_Ti, sigma},
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



      /****create G0prime from G0******/
      GFq_t *tmp = (GFq_t*)calloc(MEDS_k * MEDS_m * MEDS_n, sizeof(GFq_t));
      pmod_mat_mul_revise(tmp, MEDS_k, MEDS_m * MEDS_n,
      Ti, MEDS_k, MEDS_k,
      Main_Buffer, MEDS_k, MEDS_m * MEDS_n); //Main buffer here only G0 when i = 1, with another i, mainbuffer is G, Read G0?
      for (int c = 0; c < MEDS_m * MEDS_n; c++)
      {
        for (int r = 0; r < MEDS_k; r++)
        {
          pmod_mat_set_entry(Main_Buffer, MEDS_k,MEDS_m * MEDS_n, r, c, tmp[r*MEDS_m * MEDS_n + c]);
        }
      }
      LOG_MAT(Main_Buffer, MEDS_k, MEDS_m * MEDS_n);
      free(tmp); // deinit tmp buffer


      /****create A, B_inv_data from Gpime******/
      if(solve(A, B_inv_data, Main_Buffer, Amm) < 0)
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
      hal_putchar(0); //send start to reading

      //Read G0 back
      read_stream(Main_Buffer, MEDS_k * MEDS_m * MEDS_n);
      //write_stream(Main_Buffer,MEDS_k * MEDS_m * MEDS_n);// Verify G0

      pi(Main_Buffer, A, B, Main_Buffer); 
      if (pmod_mat_syst_ct(Main_Buffer, MEDS_k, MEDS_m*MEDS_n) != 0)
      {
        LOG("redo G[%i]", i);
        continue; // Not systematic; try again for index i.
      }
      LOG_MAT_FMT(Main_Buffer, MEDS_k, MEDS_m*MEDS_n, "G[%i]", i);

      //Send A,B matrix
      write_stream(A_inv_data, MEDS_m * MEDS_m);

      write_stream(B_inv_data, MEDS_m * MEDS_m);
      //Send G_data
      write_stream(Main_Buffer,MEDS_k * MEDS_m * MEDS_n);

      //Read G0 back for recalculating Gprime
      read_stream(Main_Buffer, MEDS_k * MEDS_m * MEDS_n);

      hal_getchar(); //receive data for done
      //successfull generated G[s] and send out to python scripts; break out of while loop
      break;
    }
    free(A);
    free(B);
  }

    /*******Create sk data****/
    sk = (unsigned char*)calloc(MEDS_SK_BYTES, sizeof(unsigned char));
    memcpy(sk, delta, MEDS_sec_seed_bytes);
    memcpy(sk + MEDS_sec_seed_bytes, sigma_G0, MEDS_pub_seed_bytes);

    bitstream_t bs;

    bs_init(&bs, sk + MEDS_sec_seed_bytes + MEDS_pub_seed_bytes, MEDS_SK_BYTES - MEDS_sec_seed_bytes - MEDS_pub_seed_bytes);

    for (int si = 1; si < MEDS_s; si++)
    {
      //Send start to read
      hal_putchar(0);
      //Read A_in_data
      read_stream(A_inv_data,MEDS_m * MEDS_m);
      for (int j = 0; j < MEDS_m*MEDS_m; j++)
      {
        bs_write(&bs, A_inv_data[j], GFq_bits);
      }
      bs_finalize(&bs);
      //Finish reading
      hal_getchar();
    }
    
    for (int si = 1; si < MEDS_s; si++)
    {
      //Send start to read
      hal_putchar(0);
      //Read B_in_data
      read_stream(B_inv_data,MEDS_m * MEDS_m);
      for (int j = 0; j < MEDS_n*MEDS_n; j++)
      {
        bs_write(&bs, B_inv_data[j], GFq_bits);
      }
      bs_finalize(&bs);
      //Finish read data
      hal_getchar();
    }
    LOG_HEX(sk, MEDS_SK_BYTES);

    hal_putchar(0);
    write_stream_str("sk = ", sk, MEDS_SK_BYTES);
    hal_getchar();
    //Deinit memory
    free(A_inv_data);
    free(B_inv_data);
    free(sk);


  // //Send PK
  // pk = (uint8_t*)calloc(MEDS_m*MEDS_n*MEDS_k, sizeof(uint8_t)); //167717 byte => declare 27000 byte
  // uint8_t *tmp_pk = pk;

  // memcpy(tmp_pk, sigma_G0, MEDS_pub_seed_bytes); //copy pub_seed at first 32 bytes

  // LOG_VEC(tmp_pk, MEDS_pub_seed_bytes, "sigma_G0 (pk)");
  // tmp_pk += MEDS_pub_seed_bytes; // Increase next pub_seed byte

  // bitstream_t bs;

  // bs_init(&bs, tmp_pk, MEDS_PK_BYTES - MEDS_pub_seed_bytes); // Init with 167717-32 byte first => Declare (27000 - 32)*6
  // for (int si = 1; si < MEDS_s; si++)
  // {
  //   for (int j = (MEDS_m-1)*MEDS_n; j < MEDS_m*MEDS_n; j++)
  //     bs_write(&bs, Main_Buffer[MEDS_m*MEDS_n + j], GFq_bits);
  // }

  // // for (int r = 2; r < MEDS_k; r++)
  // //   for (int j = MEDS_k; j < MEDS_m*MEDS_n; j++)
  // //     bs_write(&bs, Main_Buffer[r*MEDS_m*MEDS_n + j], GFq_bits);

  // bs_finalize(&bs);
  // free(pk);

  return 0;
}

int crypto_sign(
    unsigned char *sm, unsigned long *smlen,
    const unsigned char *m, unsigned long mlen,
    const unsigned char *sk
  )
{
  uint8_t delta[MEDS_sec_seed_bytes];

  randombytes(delta, MEDS_sec_seed_bytes);


  // skip secret seed
  sk += MEDS_sec_seed_bytes;

  pmod_mat_t G_0[MEDS_k * MEDS_m * MEDS_n];


  rnd_sys_mat(G_0, MEDS_k, MEDS_m*MEDS_n, sk, MEDS_pub_seed_bytes);

  sk += MEDS_pub_seed_bytes;


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

    bs_init(&bs, (uint8_t*)sk, MEDS_SK_BYTES - MEDS_sec_seed_bytes - MEDS_pub_seed_bytes);

    for (int si = 1; si < MEDS_s; si++)
    {
      for (int j = 0; j < MEDS_m*MEDS_m; j++)
        A_inv[si][j] = bs_read(&bs, GFq_bits);

      bs_finalize(&bs);
    }

    for (int si = 1; si < MEDS_s; si++)
    {
      for (int j = 0; j < MEDS_n*MEDS_n; j++)
        B_inv[si][j] = bs_read(&bs, GFq_bits);

      bs_finalize(&bs);
    }

    bs_finalize(&bs);
  }


  for (int i = 1; i < MEDS_s; i++)
    LOG_MAT(A_inv[i], MEDS_m, MEDS_m);

  for (int i = 1; i < MEDS_s; i++)
    LOG_MAT(B_inv[i], MEDS_n, MEDS_n);

  LOG_MAT(G_0, MEDS_k, MEDS_m*MEDS_m);

  LOG_VEC(delta, MEDS_sec_seed_bytes);


  uint8_t stree[MEDS_st_seed_bytes * SEED_TREE_size] = {0};
  uint8_t alpha[MEDS_st_salt_bytes];

  uint8_t *rho = &stree[MEDS_st_seed_bytes * SEED_TREE_ADDR(0,0)];

  XOF((uint8_t*[]){rho, alpha},
      (size_t[]){MEDS_st_seed_bytes, MEDS_st_salt_bytes},
      delta, MEDS_sec_seed_bytes,
      2);

  t_hash(stree, alpha, 0, 0);

  uint8_t *sigma = &stree[MEDS_st_seed_bytes * SEED_TREE_ADDR(MEDS_seed_tree_height, 0)];

  for (int i = 0; i < MEDS_t; i++)
  {
     LOG_HEX_FMT((&sigma[i*MEDS_st_seed_bytes]), MEDS_st_seed_bytes, "sigma[%i]", i);
  }

  pmod_mat_t A_tilde_data[MEDS_t * MEDS_m * MEDS_m];
  pmod_mat_t B_tilde_data[MEDS_t * MEDS_m * MEDS_m];

  pmod_mat_t *A_tilde[MEDS_t];
  pmod_mat_t *B_tilde[MEDS_t];

  for (int i = 0; i < MEDS_t; i++)
  {
    A_tilde[i] = &A_tilde_data[i * MEDS_m * MEDS_m];
    B_tilde[i] = &B_tilde_data[i * MEDS_n * MEDS_n];
  }


  uint8_t seed_buf[MEDS_st_salt_bytes + MEDS_st_seed_bytes + sizeof(uint32_t)] = {0};
  memcpy(seed_buf, alpha, MEDS_st_salt_bytes);

  uint8_t *addr_pos = seed_buf + MEDS_st_salt_bytes + MEDS_st_seed_bytes;


  keccak_state h_shake;
  shake256_init(&h_shake);

  for (int i = 0; i < MEDS_t; i++)
  {
    pmod_mat_t G_tilde_ti[MEDS_k * MEDS_m * MEDS_m];

    while (1 == 1)
    {
      uint8_t sigma_A_tilde_i[MEDS_pub_seed_bytes];
      uint8_t sigma_B_tilde_i[MEDS_pub_seed_bytes];

      for (int j = 0; j < 4; j++)
        addr_pos[j] = (i >> (j*8)) & 0xff;

      memcpy(seed_buf + MEDS_st_salt_bytes, &sigma[i*MEDS_st_seed_bytes], MEDS_st_seed_bytes);

      LOG_HEX_FMT(seed_buf, MEDS_st_salt_bytes + MEDS_st_seed_bytes + 4, "sigma_prime[%i]", i);

      XOF((uint8_t*[]){sigma_A_tilde_i, sigma_B_tilde_i, &sigma[i*MEDS_st_seed_bytes]},
           (size_t[]){MEDS_pub_seed_bytes, MEDS_pub_seed_bytes, MEDS_st_seed_bytes},
           seed_buf, MEDS_st_salt_bytes + MEDS_st_seed_bytes + 4,
           3);

      LOG_HEX_FMT(sigma_A_tilde_i, MEDS_pub_seed_bytes, "sigma_A_tilde[%i]", i);
      rnd_inv_matrix(A_tilde[i], MEDS_m, MEDS_m, sigma_A_tilde_i, MEDS_pub_seed_bytes);

      LOG_HEX_FMT(sigma_B_tilde_i, MEDS_pub_seed_bytes, "sigma_B_tilde[%i]", i);
      rnd_inv_matrix(B_tilde[i], MEDS_n, MEDS_n, sigma_B_tilde_i, MEDS_pub_seed_bytes);

      LOG_MAT_FMT(A_tilde[i], MEDS_m, MEDS_m, "A_tilde[%i]", i);
      LOG_MAT_FMT(B_tilde[i], MEDS_n, MEDS_n, "B_tilde[%i]", i);


      pi(G_tilde_ti, A_tilde[i], B_tilde[i], G_0);


      LOG_MAT_FMT(G_tilde_ti, MEDS_k, MEDS_m*MEDS_n, "G_tilde[%i]", i);

      if (pmod_mat_syst_ct(G_tilde_ti, MEDS_k, MEDS_m*MEDS_n) == 0)
        break;
    }

    LOG_MAT_FMT(G_tilde_ti, MEDS_k, MEDS_m*MEDS_n, "G_tilde[%i]", i);

    bitstream_t bs;
    uint8_t bs_buf[CEILING((MEDS_k * (MEDS_m*MEDS_n - MEDS_k)) * GFq_bits, 8)];
    
    bs_init(&bs, bs_buf, CEILING((MEDS_k * (MEDS_m*MEDS_n - MEDS_k)) * GFq_bits, 8));

    for (int r = 0; r < MEDS_k; r++)
      for (int j = MEDS_k; j < MEDS_m*MEDS_n; j++)
        bs_write(&bs, G_tilde_ti[r * MEDS_m*MEDS_n + j], GFq_bits);

    shake256_absorb(&h_shake, bs_buf, CEILING((MEDS_k * (MEDS_m*MEDS_n - MEDS_k)) * GFq_bits, 8));
  }

  shake256_absorb(&h_shake, (uint8_t*)m, mlen);

  shake256_finalize(&h_shake);

  uint8_t digest[MEDS_digest_bytes];

  shake256_squeeze(digest, MEDS_digest_bytes, &h_shake);

  LOG_VEC(digest, MEDS_digest_bytes);


  uint8_t h[MEDS_t];

  parse_hash(digest, MEDS_digest_bytes, h, MEDS_t);

  LOG_VEC(h, MEDS_t);


  bitstream_t bs;

  bs_init(&bs, sm, MEDS_w * (CEILING(MEDS_m*MEDS_m * GFq_bits, 8) + CEILING(MEDS_n*MEDS_n * GFq_bits, 8)));

  uint8_t *path = sm + MEDS_w * (CEILING(MEDS_m*MEDS_m * GFq_bits, 8) + CEILING(MEDS_n*MEDS_n * GFq_bits, 8));

  t_hash(stree, alpha, 0, 0);

  stree_to_path(stree, h, path, alpha);

  for (int i = 0; i < MEDS_t; i++)
  {
    if (h[i] > 0)
    {
      {
        pmod_mat_t mu[MEDS_m*MEDS_m];

        pmod_mat_mul(mu, MEDS_m, MEDS_m, A_tilde[i], MEDS_m, MEDS_m, A_inv[h[i]], MEDS_m, MEDS_m);

        LOG_MAT(mu, MEDS_m, MEDS_m);

        for (int j = 0; j < MEDS_m*MEDS_m; j++)
          bs_write(&bs, mu[j], GFq_bits);
      }

      bs_finalize(&bs);

      {
        pmod_mat_t nu[MEDS_n*MEDS_n];

        pmod_mat_mul(nu, MEDS_n, MEDS_n, B_inv[h[i]], MEDS_n, MEDS_n, B_tilde[i], MEDS_n, MEDS_n);

        LOG_MAT(nu, MEDS_n, MEDS_n);

        for (int j = 0; j < MEDS_n*MEDS_n; j++)
          bs_write(&bs, nu[j], GFq_bits);
      }

      bs_finalize(&bs);
    }
  }

  memcpy(sm + MEDS_SIG_BYTES - MEDS_digest_bytes - MEDS_st_salt_bytes, digest, MEDS_digest_bytes);
  memcpy(sm + MEDS_SIG_BYTES - MEDS_st_salt_bytes, alpha, MEDS_st_salt_bytes);
  memcpy(sm + MEDS_SIG_BYTES, m, mlen);

  *smlen = MEDS_SIG_BYTES + mlen;

  LOG_HEX(sm, MEDS_SIG_BYTES + mlen);

  return 0;
}

int crypto_sign_open(
    unsigned char *m, unsigned long *mlen,
    const unsigned char *sm, unsigned long smlen,
    const unsigned char *pk
  )
{
  LOG_HEX(pk, MEDS_PK_BYTES);
  LOG_HEX(sm, smlen);

  pmod_mat_t G_data[MEDS_k*MEDS_m*MEDS_n * MEDS_s];
  pmod_mat_t *G[MEDS_s];

  for (int i = 0; i < MEDS_s; i++)
    G[i] = &G_data[i * MEDS_k * MEDS_m * MEDS_n];


  rnd_sys_mat(G[0], MEDS_k, MEDS_m*MEDS_n, pk, MEDS_pub_seed_bytes);

  {
    bitstream_t bs;

    bs_init(&bs, (uint8_t*)pk + MEDS_pub_seed_bytes, MEDS_PK_BYTES - MEDS_pub_seed_bytes);

    for (int i = 1; i < MEDS_s; i++)
    {
      for (int r = 0; r < MEDS_k; r++)
        for (int c = 0; c < MEDS_k; c++)
          if (r == c)
            pmod_mat_set_entry(G[i], MEDS_k, MEDS_m * MEDS_n, r, c, 1);
          else
            pmod_mat_set_entry(G[i], MEDS_k, MEDS_m * MEDS_n, r, c, 0);

      for (int j = (MEDS_m-1)*MEDS_n; j < MEDS_m*MEDS_n; j++)
        G[i][MEDS_m*MEDS_n + j] = bs_read(&bs, GFq_bits);

      for (int r = 2; r < MEDS_k; r++)
        for (int j = MEDS_k; j < MEDS_m*MEDS_n; j++)
          G[i][r*MEDS_m*MEDS_n + j] = bs_read(&bs, GFq_bits);

      for (int ii = 0; ii < MEDS_m; ii++)
        for (int j = 0; j < MEDS_n; j++)
          G[i][ii*MEDS_n + j] = ii == j ? 1 : 0;

      for (int ii = 0; ii < MEDS_m-1; ii++)
        for (int j = 0; j < MEDS_n; j++)
          G[i][MEDS_m*MEDS_n + ii*MEDS_n + j] = (ii+1) == j ? 1 : 0;

      bs_finalize(&bs);
    }
  }

  for (int i = 0; i < MEDS_s; i++)
    LOG_MAT_FMT(G[i], MEDS_k, MEDS_m*MEDS_n, "G[%i]", i);

  LOG_HEX_FMT(sm, MEDS_w * (CEILING(MEDS_m*MEDS_m * GFq_bits, 8) + CEILING(MEDS_n*MEDS_n * GFq_bits, 8)), "munu");
  LOG_HEX_FMT(sm + MEDS_w * (CEILING(MEDS_m*MEDS_m * GFq_bits, 8) + CEILING(MEDS_n*MEDS_n * GFq_bits, 8)),
      MEDS_max_path_len * MEDS_st_seed_bytes, "path");

  uint8_t *digest = (uint8_t*)sm + (MEDS_SIG_BYTES - MEDS_digest_bytes - MEDS_st_salt_bytes);

  uint8_t *alpha = (uint8_t*)sm + (MEDS_SIG_BYTES - MEDS_st_salt_bytes);

  LOG_HEX(digest, MEDS_digest_bytes);
  LOG_HEX(alpha, MEDS_st_salt_bytes);

  uint8_t h[MEDS_t];

  parse_hash(digest, MEDS_digest_bytes, h, MEDS_t);


  bitstream_t bs;

  bs_init(&bs, (uint8_t*)sm, MEDS_w * (CEILING(MEDS_m*MEDS_m * GFq_bits, 8) + CEILING(MEDS_n*MEDS_n * GFq_bits, 8)));

  uint8_t *path = (uint8_t*)sm + MEDS_w * (CEILING(MEDS_m*MEDS_m * GFq_bits, 8) + CEILING(MEDS_n*MEDS_n * GFq_bits, 8));

  uint8_t stree[MEDS_st_seed_bytes * SEED_TREE_size] = {0};

  path_to_stree(stree, h, path, alpha);

  uint8_t *sigma = &stree[MEDS_st_seed_bytes * SEED_TREE_ADDR(MEDS_seed_tree_height, 0)];

  pmod_mat_t G_hat_i[MEDS_k*MEDS_m*MEDS_n];

  pmod_mat_t mu[MEDS_m*MEDS_m];
  pmod_mat_t nu[MEDS_n*MEDS_n];


  uint8_t seed_buf[MEDS_st_salt_bytes + MEDS_st_seed_bytes + sizeof(uint32_t)] = {0};
  memcpy(seed_buf, alpha, MEDS_st_salt_bytes);

  uint8_t *addr_pos = seed_buf + MEDS_st_salt_bytes + MEDS_st_seed_bytes;


  keccak_state shake;
  shake256_init(&shake);

  for (int i = 0; i < MEDS_t; i++)
  {
    if (h[i] > 0)
    {
      for (int j = 0; j < MEDS_m*MEDS_m; j++)
        mu[j] = bs_read(&bs, GFq_bits) % MEDS_p;

      bs_finalize(&bs);

      for (int j = 0; j < MEDS_n*MEDS_n; j++)
        nu[j] = bs_read(&bs, GFq_bits) % MEDS_p;

      bs_finalize(&bs);


      LOG_MAT_FMT(mu, MEDS_m, MEDS_m, "mu[%i]", i);
      LOG_MAT_FMT(nu, MEDS_n, MEDS_n, "nu[%i]", i);

      // Check if mu is invetible.
      {
        pmod_mat_t tmp_mu[MEDS_m*MEDS_m];

        memcpy(tmp_mu, mu, MEDS_m*MEDS_m*sizeof(GFq_t));

        if (pmod_mat_syst_ct(tmp_mu, MEDS_m, MEDS_m) != 0)
        {
          fprintf(stderr, "Signature verification failed; malformed signature!\n");

          return -1;
        }
      }

      // Check if nu is invetible.
      {
        pmod_mat_t tmp_nu[MEDS_n*MEDS_n];

        memcpy(tmp_nu, nu, MEDS_n*MEDS_n*sizeof(GFq_t));

        if (pmod_mat_syst_ct(tmp_nu, MEDS_n, MEDS_n) != 0)
        {
          fprintf(stderr, "Signature verification failed; malformed signature!\n");

          return -1;
        }
      }


      pi(G_hat_i, mu, nu, G[h[i]]);


      LOG_MAT_FMT(G_hat_i, MEDS_k, MEDS_m*MEDS_n, "G_hat[%i]", i);

      if (pmod_mat_syst_ct(G_hat_i, MEDS_k, MEDS_m*MEDS_n) < 0)
      {
        fprintf(stderr, "Signature verification failed!\n");

        return -1;
      }

      LOG_MAT_FMT(G_hat_i, MEDS_k, MEDS_m*MEDS_n, "G_hat[%i]", i);
    }
    else
    {
      while (1 == 1)
      {
        LOG_VEC_FMT(&sigma[i*MEDS_st_seed_bytes], MEDS_st_seed_bytes, "seeds[%i]", i);

        uint8_t sigma_A_hat_i[MEDS_pub_seed_bytes];
        uint8_t sigma_B_hat_i[MEDS_pub_seed_bytes];

        for (int j = 0; j < 4; j++)
          addr_pos[j] = (i >> (j*8)) & 0xff;

        memcpy(seed_buf + MEDS_st_salt_bytes, &sigma[i*MEDS_st_seed_bytes], MEDS_st_seed_bytes);

        LOG_HEX_FMT(seed_buf, MEDS_st_salt_bytes + MEDS_st_seed_bytes + 4, "sigma_prime[%i]", i);

        XOF((uint8_t*[]){sigma_A_hat_i, sigma_B_hat_i, &sigma[i*MEDS_st_seed_bytes]},
            (size_t[]){MEDS_pub_seed_bytes, MEDS_pub_seed_bytes, MEDS_st_seed_bytes},
            seed_buf, MEDS_st_salt_bytes + MEDS_st_seed_bytes + 4,
            3);

        pmod_mat_t A_hat_i[MEDS_m*MEDS_m];
        pmod_mat_t B_hat_i[MEDS_n*MEDS_n];

        LOG_HEX_FMT(sigma_A_hat_i, MEDS_pub_seed_bytes, "sigma_A_hat[%i]", i);
        rnd_inv_matrix(A_hat_i, MEDS_m, MEDS_m, sigma_A_hat_i, MEDS_pub_seed_bytes);

        LOG_HEX_FMT(sigma_B_hat_i, MEDS_pub_seed_bytes, "sigma_B_hat[%i]", i);
        rnd_inv_matrix(B_hat_i, MEDS_n, MEDS_n, sigma_B_hat_i, MEDS_pub_seed_bytes);

        LOG_MAT_FMT(A_hat_i, MEDS_m, MEDS_m, "A_hat[%i]", i);
        LOG_MAT_FMT(B_hat_i, MEDS_n, MEDS_n, "B_hat[%i]", i);


        pi(G_hat_i, A_hat_i, B_hat_i, G[0]);

        LOG_MAT_FMT(G_hat_i, MEDS_k, MEDS_m*MEDS_n, "G_hat[%i]", i);


        if (pmod_mat_syst_ct(G_hat_i, MEDS_k, MEDS_m*MEDS_n) == 0)
        {
          LOG_MAT_FMT(G_hat_i, MEDS_k, MEDS_m*MEDS_n, "G_hat[%i]", i);
          break;
        }

        LOG_MAT_FMT(G_hat_i, MEDS_k, MEDS_m*MEDS_n, "G_hat[%i]", i);
      }
    }


    bitstream_t bs;
    uint8_t bs_buf[CEILING((MEDS_k * (MEDS_m*MEDS_n - MEDS_k)) * GFq_bits, 8)];
    
    bs_init(&bs, bs_buf, CEILING((MEDS_k * (MEDS_m*MEDS_n - MEDS_k)) * GFq_bits, 8));

    for (int r = 0; r < MEDS_k; r++)
      for (int j = MEDS_k; j < MEDS_m*MEDS_n; j++)
        bs_write(&bs, G_hat_i[r * MEDS_m*MEDS_n + j], GFq_bits);
 
    shake256_absorb(&shake, bs_buf, CEILING((MEDS_k * (MEDS_m*MEDS_n - MEDS_k)) * GFq_bits, 8));
  }

  shake256_absorb(&shake, (uint8_t*)(sm + MEDS_SIG_BYTES), smlen - MEDS_SIG_BYTES);

  shake256_finalize(&shake);

  uint8_t check[MEDS_digest_bytes];

  shake256_squeeze(check, MEDS_digest_bytes, &shake);

  if (memcmp(digest, check, MEDS_digest_bytes) != 0)
  {
    fprintf(stderr, "Signature verification failed!\n");

    return -1;
  }

  memcpy(m, (uint8_t*)(sm + MEDS_SIG_BYTES), smlen - MEDS_SIG_BYTES);
  *mlen = smlen - MEDS_SIG_BYTES;

  return 0;
}