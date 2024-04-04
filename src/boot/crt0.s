
.set noreorder

.set _STACK_SIZE, 0x100

# We're going to override ps1-bare-metal's _start() with a minimal version that
# skips .bss initialization (getting rid of memset() in the process) and moves
# the stack to a statically allocated buffer.
.section .text._start, "ax", @progbits
.global _start
.type _start, @function

_start:
	la    $gp, _gp
	j     main
	addiu $sp, $gp, %gprel(_stackBuffer) + _STACK_SIZE - 16

.section .sbss._stackBuffer, "aw"
.type _stackBuffer, @object

_stackBuffer:
	.space _STACK_SIZE
