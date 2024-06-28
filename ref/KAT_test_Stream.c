#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include <hal.h>
#if defined(STM32F4)
#include <aes.h>
#endif
#define DATA_LENGTH 48
#define COUNTER_LENGTH 100
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
/*Prototype Declare*/
static void init_entropy (unsigned char * entropy_input, unsigned int Length);
static int do_seedgen(unsigned char * seed, unsigned int Length, unsigned int CntLenght);
/*Prototype Defination*/
static void init_entropy (unsigned char * entropy_input, unsigned int Length)
{
    for (int i=0; i<Length; i++)
    {
        entropy_input[i] = i;
    }
    randombytes_init(entropy_input, NULL, 256);
}
static int do_seedgen(unsigned char * seed, unsigned int Length, unsigned int CntLenght)
{
    unsigned char* msg = (unsigned char*)calloc(3300, sizeof(unsigned char));
    int mlen = 0;
    for (int i = 0; i < CntLenght; i++)
    {
        printf("count = %d\n",i);
        randombytes(seed,DATA_LENGTH);
        write_stream_str("seed = ", seed, DATA_LENGTH);
        mlen = 33*(i+1);
        printf("mlen = %d\n", mlen);
        randombytes(msg, mlen);
        write_stream_str("msg = ", msg, mlen);

        printf("pk =\n");
        printf("sk =\n");
        printf("smlen =\n");
        printf("sm =\n\n");
        if(i==99)
        {
            printf("finished\n");
            free(msg);
            return 0;
        }
        /***************************/
    }
    return -1;
}
/*Main Function*/

int main()
{
    // char                fn_req[32], fn_rsp[32];
    // FILE                *fp_req, *fp_rsp;
    //Declare local variable
    unsigned char       entropy_input[DATA_LENGTH];
    unsigned char       seed[DATA_LENGTH];
    unsigned long       mlen, smlen;
    unsigned char       *m, *sm, *m1;
    int                 val = 0;
    int                 val_check = 0;
    int                 ret_val;
    unsigned char       *pk, *sk;
    char                get_char;
    /*For checking data*/
    int                 done;
    int                 count_pk = 0;
    int                 count_seed = 0;
    char                get_char_data;
    //Define function
    hal_setup();
    hal_led_off();
  

    hal_getchar();
    //Init randome byte
    /*******************************/
    init_entropy(entropy_input,DATA_LENGTH);

    /*For implement*/
    printf("Send data from \"%s\" - %s\n",_elf_name,CRYPTO_ALGNAME);
    
    if((ret_val = do_seedgen(seed, DATA_LENGTH,COUNTER_LENGTH))!= 0)
    {
        printf("crypto_seed_keypair returned <%d>\n", ret_val);
        return KAT_CRYPTO_FAILURE;
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
            hal_putchar(0);
            // m1 = (unsigned char *)calloc(mlen+CRYPTO_BYTES, sizeof(unsigned char));
        //     // sm = (unsigned char *)calloc(mlen+CRYPTO_BYTES, sizeof(unsigned char));
            for(int i = 0; i < mlen; i ++)
            {
                m[i] = hal_getchar();
            }
        }
        
        else if(val_check == PK_CODE)
        {
            if((ret_val = crypto_sign_keypair_streaming(pk,sk)) != 0)
            {
                printf("crypto_sign_keypair returned <%d>\n", ret_val);
                
                return KAT_CRYPTO_FAILURE;
            }
            hal_led_on();
            write_stream_str("pk = ", m, 10);
        }
        else if(val_check == SK_CODE)
        {
            printf("sk = DONE\n")
            free(sk);
        }
        else if (val_check == SML_CODE)
        {
            
            // if ( (ret_val = crypto_sign(sm, &smlen, m, mlen, sk)) != 0) {
            //     printf("crypto_sign returned <%d>\n", ret_val);
            //     return KAT_CRYPTO_FAILURE;
            // }
            printf("smlen = %d\n", smlen);
        }
        else if (val_check == SM_CODE)
        {
            write_stream_str("sm = ", m, 10);
            //fprintBstr("sm = ", sm, smlen);
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
            // free(m1);
            // free(sm);
        }
        else if (val_check == STOP_CODE)
        {
            printf("finished from C\n");

        }

    }while (get_char != '\n');
}
