#include <hal.h>
#include <stdio.h>


extern const char _elf_name[];

#ifdef STM32F4  // assembler demo only for the target device
int test_asm(int);
extern int somedata; // = 0x12345678;
#endif

int main(void)
{
  hal_setup();

  hal_led_on();

  for (int rounds = 0; rounds < 10; rounds++)
  {
    printf("Hello world from \"%s\"!\n", _elf_name);

    int val = 0;

    for (int i = 0; i < 4; i++)
      val = val << 8 | hal_getchar();

    uint64_t t_start = hal_get_time();

    val *= 3;

    uint64_t t_stop = hal_get_time();

    for (int i = 0; i < 4; i++)
      hal_putchar((val >> ((3-i)*8)) & 0xff);


    printf("%i time: %u    ", val, (unsigned) (t_stop - t_start));

#ifdef STM32F4  // assembler demo only for the target device
    val = test_asm(1);

    printf("asm: %x + 1 = %x\n", somedata, val);
#else
    printf("\n");
#endif

  }

  hal_led_off();

  while (1)
    ;
  return 0;
}