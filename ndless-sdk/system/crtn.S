/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* for C++ support */

/* A modern newlib's libc_a-fini.o calls _fini and libc_a-init.o calls _init,
 * both of which the libc expects the C runtime to provide. These files predate
 * that convention, so an arm-none-eabi-gcc of 2024 links ./stemhelp.elf with
 * "undefined reference to `_fini`". Both are empty here on purpose: the app's
 * exit path is __crt0_exit, not the libc's atexit chain. */
.section .text.fini_stub
	.global _fini
	.type _fini, %function
_fini:	bx lr

.section .text.init_stub
	.global _init
	.type _init, %function
_init:	bx lr

.section .text.crtn
exit: .weak exit /* If linked without newlib, used by crt0. Can't be defined in crt0, as newlibs exit is also weak */
	bl __crt0_exit

.section .init_array
	.long -1 /* Terminating character for ctor list */
    
.section .fini_array.0
	.long -1
