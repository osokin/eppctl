/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/types.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct map {
	int cpu;
	int val;
};

static int
get_epp(struct map **kv, int maxid)
{
	size_t size;
	char buf[64];
	int i, val;

	for (i = 0; i <= maxid; i++) {
		size = sizeof(int);
		snprintf(buf, sizeof(buf), "dev.hwpstate_intel.%d.epp", i);

		if (sysctlbyname(buf, &val, &size, NULL, 0) < 0) {
			if (errno == ENOENT) {
				/*
				 * We are probably done here.  There's no
				 * more CPUs to read.
				 */
				break;
			} else {
				warn("sysctlbyname(%s)", buf);
				return (-1);
			}
		}

		kv[i] = malloc(sizeof(struct map));
		if (kv[i] == NULL)
			err(1, "malloc");

		kv[i]->cpu = i;
		kv[i]->val = val;
	}

	return (i);
}

static int
set_epp(struct map **kv, int ncpu, char *arg)
{
	size_t size;
	char buf[64], fail[64];
	int i, j, val;
	int errfail;
	const char *errstr;

	val = strtonum(arg, 0, 100, &errstr);
	if (errstr != NULL) {
		warnx("strtonum(%s): %s", arg, errstr);
		return (-1);
	}

	for (i = 0; i < ncpu; i++) {
		size = sizeof(int);

		snprintf(buf, sizeof(buf), "dev.hwpstate_intel.%d.epp", i);
		if (sysctlbyname(buf, NULL, 0, &val, size) < 0) {
			errfail = errno;
			if (errno == ENOENT) {
				/*
				 * We are probably done here.  There's no
				 * more CPUs to update.
				 */
				warnx("unexpected end of CPU list at %s", buf);
				return (-1);

			} else {
				/*
				 * Something goes wrong here, rollback
				 * previous changes.
				 */
				strlcpy(fail, buf, sizeof(fail));

				for (j = 0; j < i; j++) {
					size = sizeof(int);

					snprintf(buf, sizeof(buf), "dev.hwpstate_intel.%d.epp", j);
					if (sysctlbyname(buf, NULL, 0, &kv[j]->val, size) < 0) {
						warn("sysctlbyname(%s)", buf);
						return (-1);
					}
				}
				warnc(errfail, "sysctlbyname(%s)", fail);
				return (-1);

			}
		}
	}

	for (i = 0; i < ncpu; i++) {
		printf("dev.hwpstate_intel.%d.epp: %d -> %d\n", kv[i]->cpu, kv[i]->val, val);
	}

	return (0);
}

static void
print_epp(struct map **kv, int ncpu)
{
	int i;

	for (i = 0; i < ncpu; i++) {
		printf("dev.hwpstate_intel.%d.epp: %d\n", kv[i]->cpu, kv[i]->val);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-h] [-s value]\n\twhere value from 0 to 100\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, i, maxid, ncpu, retcode = 0;
	char *value = NULL;
	size_t len = sizeof(maxid);
	struct map **kv = NULL;

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
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		usage();
	}

	if (sysctlbyname("kern.smp.maxid", &maxid, &len, NULL, 0) < 0)
		err(1, "sysctlbyname(kern.smp.maxid)");

	kv = calloc(maxid + 1, sizeof(struct map *));
	if (kv == NULL)
		err(1, "calloc");

	if ((ncpu = get_epp(kv, maxid)) < 0) {
		for (i = 0; i <= maxid && kv[i] != NULL; i++)
			free(kv[i]);
		free(kv);
		exit(1);
	}

	if (ncpu == 0)
		errx(1, "hwpstate_intel(4) not attached");

	if (value == NULL) {
		print_epp(kv, ncpu);
		retcode = 0;
	} else {
		if (set_epp(kv, ncpu, value) < 0)
			retcode = 1;
	}

	for (i = 0; i <= maxid && kv[i] != NULL; i++)
		free(kv[i]);
	free(kv);

	return (retcode);
}
