/*
 * Copyright (c) 2026 Sergey A. Osokin
 *
 * This software was developed by Sergey A. Osokin <osa@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
get_epp_ncpu(void)
{
	size_t size;
	int ncpu;

	size = sizeof(int);
	if (sysctlbyname("hw.ncpu", &ncpu, &size, NULL, 0) < 0) {
		warn("sysctlbyname(%s)", "hw.ncpu");
		return (-1);
	}

	return (ncpu);
}

static int
set_epp(char *arg)
{
	size_t size;
	char buf[32], *endptr;
	int i, ncpu, val;

	if (arg != NULL) {
		errno = 0;
		val = strtol(arg, &endptr, 10);
		if (errno == ERANGE || endptr == arg || *endptr != '\0') {
			warn("strtol(%s)", arg);
			return (-1);
		}
	}

	size = sizeof(int);

	ncpu = get_epp_ncpu();

	if (ncpu >= 0) {
		for (i = 0; i < ncpu; i++) {
			snprintf(buf, sizeof(buf), "dev.hwpstate_intel.%d.epp", i);
			if (sysctlbyname(buf, NULL, 0, &val, size) < 0) {
				warn("sysctlbyname(%s)", buf);
				return (-1);
			}
		}
	}

	return (0);
}

static int
print_epp(void)
{
	size_t size;
	char buf[32];
	int i, ncpu, val;

	size = sizeof(int);

	ncpu = get_epp_ncpu();

	if (ncpu >= 0) {
		printf("hw.cpu: %d\n", ncpu);

		for (i = 0; i < ncpu; i++) {
			snprintf(buf, sizeof(buf), "dev.hwpstate_intel.%d.epp", i);
			if (sysctlbyname(buf, &val, &size, NULL, 0) < 0) {
				warn("sysctlbyname(%s)", buf);
				return (-1);
			}

			printf("%s: %d\n", buf, val);
		}
	}

	return (0);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-hn] [value]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	char *value = NULL;

	while ((c = getopt(argc, argv, "hs:")) != -1) {
		switch (c) {
		case 's':
			value = optarg;
			break;
		case 'h':
		case '?':
		default:
			usage();
		}
	}

	if (value == NULL) {
		if (print_epp() < 0)
			exit(1);
	} else {
		if (set_epp(value) < 0)
			exit(1);
	}

	return (0);
}
