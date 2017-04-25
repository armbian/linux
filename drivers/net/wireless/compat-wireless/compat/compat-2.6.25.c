/*
 * Copyright 2007-2010	Luis R. Rodriguez <mcgrof@winlab.rutgers.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.25.
 */

#include <linux/miscdevice.h>

/**
 * The following things are out of ./lib/vsprintf.c
 * The new iwlwifi driver is using them.
 */

/**
 * strict_strtoul - convert a string to an unsigned long strictly
 * @cp: The string to be converted
 * @base: The number base to use
 * @res: The converted result value
 *
 * strict_strtoul converts a string to an unsigned long only if the
 * string is really an unsigned long string, any string containing
 * any invalid char at the tail will be rejected and -EINVAL is returned,
 * only a newline char at the tail is acceptible because people generally
 * change a module parameter in the following way:
 *
 * 	echo 1024 > /sys/module/e1000/parameters/copybreak
 *
 * echo will append a newline to the tail.
 *
 * It returns 0 if conversion is successful and *res is set to the converted
 * value, otherwise it returns -EINVAL and *res is set to 0.
 *
 * simple_strtoul just ignores the successive invalid characters and
 * return the converted value of prefix part of the string.
 */
int strict_strtoul(const char *cp, unsigned int base, unsigned long *res);

/**
 * strict_strtol - convert a string to a long strictly
 * @cp: The string to be converted
 * @base: The number base to use
 * @res: The converted result value
 *
 * strict_strtol is similiar to strict_strtoul, but it allows the first
 * character of a string is '-'.
 *
 * It returns 0 if conversion is successful and *res is set to the converted
 * value, otherwise it returns -EINVAL and *res is set to 0.
 */
int strict_strtol(const char *cp, unsigned int base, long *res);

#define define_strict_strtoux(type, valtype)				\
int strict_strtou##type(const char *cp, unsigned int base, valtype *res)\
{									\
	char *tail;							\
	valtype val;							\
	size_t len;							\
									\
	*res = 0;							\
	len = strlen(cp);						\
	if (len == 0)							\
		return -EINVAL;						\
									\
	val = simple_strtou##type(cp, &tail, base);			\
	if ((*tail == '\0') ||						\
		((len == (size_t)(tail - cp) + 1) && (*tail == '\n'))) {\
		*res = val;						\
		return 0;						\
	}								\
									\
	return -EINVAL;							\
}									\

#define define_strict_strtox(type, valtype)				\
int strict_strto##type(const char *cp, unsigned int base, valtype *res)	\
{									\
	int ret;							\
	if (*cp == '-') {						\
		ret = strict_strtou##type(cp+1, base, res);		\
		if (!ret)						\
			*res = -(*res);					\
	} else								\
		ret = strict_strtou##type(cp, base, res);		\
									\
	return ret;							\
}									\

define_strict_strtoux(l, unsigned long)
define_strict_strtox(l, long)

EXPORT_SYMBOL(strict_strtoul);
EXPORT_SYMBOL(strict_strtol);

