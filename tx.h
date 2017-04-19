// MetaTM Project
// File Name: tx.h
//
// Description: Header file for syscall benchmarks.  Encapsulates a
// simple, lightweight xbegin/xend system call implementation.
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include <unistd.h>
#include <sys/syscall.h>

#define ETXABORT 132

#ifdef __LP64__
// 64bit
#define __NR_xbegin 285
#define __NR_xend   286
#define __NR_xabort 287
#else
// 32bit
#define __NR_xbegin  324
#define __NR_xend    325
#define __NR_xabort  326
#define __NR_xpause  328
#define __NR_xresume 329
#endif

static __inline__ void osa_print(char * str, unsigned int num) {
   __asm__ __volatile__ ("xchg %%bx, %%bx " \
         : /*no output*/				  \
         : "b"(str), "c"(num), "S"(1) );
}

#ifndef __LP64__
/* 32 bit */

static unsigned int trampoline = 0xffffe400;

#define __syscall_lite(no, trampoline) asm volatile("mov %0, %%eax\n"		\
					     "call *%1    \n"		        \
					     : : "g"(no), "g"(trampoline) : "eax", "memory")

#define syscall_lite(arg) __syscall_lite(arg, trampoline)

#else

/* 64 bit */

#define syscall_lite(arg) asm volatile("movq %0, %%rax\n" \
				       "syscall\n" : : "g"(arg) : "rax", "memory")


#endif

#define TX_DEFAULTS                 0
#define TX_ABORT_UNSUPPORTED        0
#define TX_NONDURABLE               1
#define TX_NO_USER_ROLLBACK         2
#define TX_NO_AUTO_RETRY            4
#define TX_ERROR_UNSUPPORTED        8
#define TX_LIVE_DANGEROUSLY        16
#define TX_ORDERED_COMMIT          32
#define TX_NO_STALL                64
#define TX_ORDERED_RESET_SEQUENCE 128


/* xbegin:
 *   
 *    flags - can be one of the above.  A good default is TX_DEFAULTS,
 *            which does not select any of the other flags below.
 *
 *            TX_NONDURABLE turns off durability for faster
 *            performance (at some safety cost).
 *
 *            TX_NOUSER_ROLLBACK disables copy-on-write paging for the
 *            user address space.  This is primarily for use by
 *            user-level TMs, although applications may use this with
 *            some care.
 *
 *            TX_NOAUTO_RETRY disables automatic retry of a
 *            transaction on an abort.  The most typical approach to
 *            writing transactions has an implicit expectation that a
 *            failed transaction will automatically restart. 
 *
 *    xsw - Transaction status word.  This is a memory word that is
 *          updated by the kernel to reflect if the transaction is
 *          aborted, committed, etc.  This may be NULL.
 *
 */

#define xbegin(flags, xsw)              syscall(__NR_xbegin, flags, xsw)
#define xbegin_ordered(flags, xsw, idx) syscall(__NR_xbegin, flags|TX_ORDERED_COMMIT, xsw, idx)
#define xend()                          syscall(__NR_xend)
#define xabort()                        syscall(__NR_xabort)
#define xpause()                        syscall(__NR_xpause)
#define xresume(tx_id)                  syscall(__NR_xresume, tx_id)
