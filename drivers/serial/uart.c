/* Copyright (C) 2016 David Gao <davidgao1001@gmail.com>
 * Copyright (C) 2016 Gan Quan <coin2028@hotmail.com>
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
#endif /* HAVE_CONFIG_H */

/* from uart driver */
#include <uart.h>

/* from libc */
#include <libc/stdio.h>

#define BUFSIZ	1024

#ifdef RAW /* baremetal driver */

void uart_puts(const char *str)
{
	for (; *str != '\0'; ++str)
		uart_putbyte((unsigned char)*str);
}

#else /* not RAW, or kernel driver */

/*
 * If a serial console requires \r\n to change line, we usually don't
 * want to specify "\r\n" in C string.
 *
 * This uart_puts() function automatically prepends a carriage return
 * before a newline.  Exceptions can be explicitly made in their own
 * device drivers.
 *
 * NOTE:
 * In disassemblies you can still see the code of this weak function.
 * This is *correct*.  The strong function code appears before the weak
 * function, and therefore the weak code would never be executed.
 */

void __weak uart_puts(const char *str)
{
	for (; *str != '\0'; ++str) {
		if (*str == '\n')
			uart_putbyte('\r');
		uart_putbyte((unsigned char)*str);
	}
}

#endif /* RAW */

ssize_t uart_printf(const char *fmt, ...)
{
	int result;
	va_list ap;
	va_start(ap, fmt);
	result = uart_vprintf(fmt, ap);
	va_end(ap);
	return result;
}

ssize_t uart_vprintf(const char *fmt, va_list ap)
{
	int result;
	char printf_buf[BUFSIZ];
	result = vsnprintf(printf_buf, BUFSIZ, fmt, ap);
	uart_puts(printf_buf);
	return result;
}

