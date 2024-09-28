#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rng.h"
#include "api.h"
#include <hal.h>
#if defined(STM32F4)
#include <aes.h>
#endif
#define DATA_LENGTH 48

#define KAT_SUCCESS          0
#define KAT_FILE_OPEN_ERROR -1
#define KAT_DATA_ERROR      -3
#define KAT_CRYPTO_FAILURE  -4
#define SEED_CODE 18
#define MSG_CODE 19
#define PK_CODE 20
#define SK_CODE 21
#define SML_CODE 22
#define SM_CODE 23
#define STOP_CODE 24
// #define PARAMETER_FOR_C "MEDS9923"
 /*For checking data*/
#define MAX_MARKER_LEN      50
/*******************************/
extern const char _elf_name[];

//Function prototype
static void fprintBstr(char *S, unsigned char *A, unsigned long long L);
static void ReadHex(unsigned char *A, int Length, char data);
int main()
{
    char                fn_req[32], fn_rsp[32];
    FILE                *fp_req, *fp_rsp;
    //Declare local variable
    unsigned char       entropy_input[DATA_LENGTH];
    unsigned char       seed[DATA_LENGTH];
    unsigned long       mlen, smlen, mlen1;
    unsigned char       msg[3300];
    unsigned char       *m, *sm, *m1;
    int                 val = 0;
    int                 val_check = 0;
    int                 ret_val;
    unsigned char       pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES];
    char                get_char;
    /*For checking data*/
    int                 done;
    int                 count_pk = 0;
    int                 count_seed = 0;
    unsigned char       seed_hex[DATA_LENGTH];
    char                get_char_data;
    //Define function
    hal_setup();

  

    hal_getchar();
    //Init randome byte
    /*******************************/
    /*For implement*/

    for (int i=0; i<DATA_LENGTH; i++)
    {
        entropy_input[i] = i;
    }
    printf("Send data from \"%s\" - %s\n",_elf_name,CRYPTO_ALGNAME);
    randombytes_init(entropy_input, NULL, 256);
    
    // printf("Send data from \"%s\" - %s\n",_elf_name,CRYPTO_ALGNAME);
    for (int i = 0; i < 100; i++)
    {
        printf("count = %d\n",i);
        randombytes(seed,DATA_LENGTH);
        fprintBstr("seed = ", seed, DATA_LENGTH);
        mlen = 33*(i+1);
        printf("mlen = %d\n", mlen);
        randombytes(msg, mlen);
        fprintBstr("msg = ", msg, mlen);

        printf("pk =\n");
        printf("sk =\n");
        printf("smlen =\n");
        printf("sm =\n\n");
        if(i==99)
        {
            printf("finished\n");
        }
        /***************************/
    }
    memset(seed, 0x00, 48);
    get_char = hal_getchar();
    do 
    {
        get_char = hal_getchar();
        for (int i = 0; i < 4; i++)
        {
            val = val << 8 | get_char;
        }
        val_check = val >> 3*8;
        if(val_check == SEED_CODE)
        {
            for(int i = 0; i < 48; i ++)
            {
                seed[i] = hal_getchar();
            }
        }
        else if (val_check == MSG_CODE)
        {
            mlen = 33*(count_pk+1);
            count_pk = count_pk + 1;
            randombytes_init(seed, NULL, 256);
            m = (unsigned char *)calloc(mlen, sizeof(unsigned char));
            m1 = (unsigned char *)calloc(mlen+CRYPTO_BYTES, sizeof(unsigned char));
            sm = (unsigned char *)calloc(mlen+CRYPTO_BYTES, sizeof(unsigned char));
            for(int i = 0; i < mlen; i ++)
            {
                m[i] = hal_getchar();
            }
        }
        
        else if(val_check == PK_CODE)
        {
            if((ret_val = crypto_sign_keypair(pk, sk)) != 0)
            {
                printf("crypto_sign_keypair returned <%d>\n", ret_val);
                
                return KAT_CRYPTO_FAILURE;
            }
            hal_led_on();
            fprintBstr("pk = ", pk, CRYPTO_PUBLICKEYBYTES);
        }
        else if(val_check == SK_CODE)
        {
            fprintBstr("sk = ", sk, CRYPTO_SECRETKEYBYTES);
        }
        else if (val_check == SML_CODE)
        {
            
            if ( (ret_val = crypto_sign(sm, &smlen, m, mlen, sk)) != 0) {
                printf("crypto_sign returned <%d>\n", ret_val);
                return KAT_CRYPTO_FAILURE;
            }
            printf("smlen = %d\n", smlen);
        }
        else if (val_check == SM_CODE)
        {
            fprintBstr("sm = ", sm, smlen);
            hal_led_off();
            // if ( (ret_val = crypto_sign_open(m1, &mlen1, sm, smlen, pk)) != 0) 
            // {
            //     hal_led_off();
            //     printf("crypto_sign_open returned <%d>\n", ret_val);
            //     sreturn KAT_CRYPTO_FAILURE;
            // }
            // if ( mlen != mlen1 ) 
            // {
            //     printf("crypto_sign_open returned bad 'mlen': Got <%llu>, expected <%llu>\n", mlen1, mlen);
            //     return KAT_CRYPTO_FAILURE;
            // }
            
            // if ( memcmp(m, m1, mlen))
            // {
            //     printf("crypto_sign_open returned bad 'm' value\n");
            //     return KAT_CRYPTO_FAILURE;
            // }
            free(m);
            free(m1);
            free(sm);
        }
        else if (val_check == STOP_CODE)
        {
            printf("finished from C\n");

        }

    }while (get_char != '\n');
}

static void fprintBstr(char *S, unsigned char *A, unsigned long long L)
{
    unsigned long i;

    printf("%s", S);

    for ( i=0; i<L; i++ )
    {
        printf("%02X", A[i]);
    }

    if ( L == 0 )
    {
        printf("00");
    }
    printf("\n");
}
static void ReadHex(unsigned char *A, int Length, char data)
{
    for(int i = 0; i < Length; i ++)
    {
        A[i] = data;
    }
}
