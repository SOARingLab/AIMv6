/* Copyright (C) 2016 Gan Quan <coin2028@hotmail.com>
 *
 * This file is part of AIMv6.
 *
 * AIMv6 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AIMv6 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef __LP64__	/* 32 bit */
#define TARGET	"elf32-tradlittlemips"
#else
#define TARGET	"elf64-tradlittlemips"
#endif

#include <mmu.h>
#include <asm-generic/vmaim.lds.h>

OUTPUT_FORMAT(TARGET)

#undef TARGET

ENTRY(__start)

SECTIONS {
	. = KERN_START;
	.text : {
		*(.text*);
	}

	STRUCT_ALIGN();
	.rodata : {
		*(.rodata*);
		EARLY_INIT(STRUCT_ALIGNMENT)
		NORM_INIT(STRUCT_ALIGNMENT)
		LATE_INIT(STRUCT_ALIGNMENT)
	}
	/* Global Offset Table (GOT) for position independent code.
	 * Since we are using our own linker script, we must provide
	 * the base address of GOT, denoted by symbol _gp, to our linker.
	 *
	 * Don't ask me why I select this value; this is the default choice
	 * of GCC.  You can view the default linker script used by GCC and
	 * see the same line via the following options:
	 * -Wl,--verbose
	 * You may notice one difference.  The reason I choose 'PROVIDE'
	 * instead of 'HIDDEN' is somewhat complex.  We could not ensure
	 * that BIOSes or bootloaders may set up GP and t9 registers for
	 * us (because AFAIK current operating systems including Linux
	 * and OpenBSD still uses a non-PIC kernel).  We have to setup GP
	 * and t9 by our own.
	 */
	PROVIDE(_gp = ALIGN (16) + 0x7ff0);
	.got : {
		*(.got*);
	}
	.data : {
		*(.data*);
	}
	/* Uninitialized sections comes after */
	.bss : {
		PROVIDE(__bss_begin__ = .);
		PROVIDE(_bss_begin = .);
		*(.bss*);
		PROVIDE(__bss_end__ = .);
		PROVIDE(_bss_end = .);
	}
	/*
	 * MIPS ISA doesn't require alignment, paging or anything else
	 * to run a kernel.  This alignment is only for cache-friendliness.
	 */
	. = ALIGN(KSTACKSIZE);
	.boot : {
		PROVIDE(boot_page_index = .);
		. += KSTACKSIZE;
		PROVIDE(boot_stack = .);
	}
	PROVIDE(_kern_end = ALIGN(PAGE_SIZE));
}
