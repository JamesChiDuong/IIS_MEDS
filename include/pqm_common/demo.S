// define exported symbols
.globl test_asm
.globl somedata

.text
.align 4
.type  test_asm, %function
test_asm:
  push {r4, r5}        // save caller registers
  ldr r4, =somedata    // load address of symbol 'somedata'
  ldr r5, [r4]         // load data
  add r0, r0, r5       // add data to the first function parameter
  pop {r4, r5}         // restore caller registers
  bx lr                // return to caller; result is in r0
  .size test_asm, .-test_asm

.data
.align 4
.type somedata, %object
somedata:
  .word 0x01234567
  .size somedata, .-somedata

