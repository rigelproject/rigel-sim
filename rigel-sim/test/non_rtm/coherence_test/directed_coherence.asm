.equ STACKSIZESHIFT, (18) #; 256kB per Core (for up to 4-way MT)

.section .text
.set mipsrigel32
.globl _start 

_start:       
  nop

  #; Get the HW thread ID.
	mfsr	$1, $6 

  #; Adjust the stack.
	ori		$2, $0, 1
	slli  $2, $2, STACKSIZESHIFT
	mul		$1, $2, $1
	sub		$sp, $1, $0 

  #; Start the test.
  jal main

_end_sim:  
  event $0, 19 # dump the reg file
_halt_sim:
  hlt;
  beq    $0, $0, _halt_sim
  nop;
  nop;
  nop;
  nop;


main:
  mfsr  $1, $6

  addi  $2, $0, 0x42
  add   $3, $1, $2
  mvui  $4, %hi(FOO1)
  ori   $4, $4, %lo(FOO1)
  ldw   $5, $4, 0
  stw   $5, $4, 64
  ldw   $6, $4, 256

LL0:
  subi  $3, $3, 1
  bnz   $3, LL0

  #; Return from main().
  jalr $31

.data
FOO1: .word(0x1234567) 
