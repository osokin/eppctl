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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
get_one_epp(const char *arch, int cpuid)
{
	size_t size;
	char buf[64];
	int val = 0;

	size = sizeof(val);
	snprintf(buf, sizeof(buf), "dev.hwpstate_%s.%d.epp", arch, cpuid);

	if (sysctlbyname(buf, &val, &size, NULL, 0) < 0) {
		if (errno == ENOENT) {
			/* no entity */
			return (-2);
		} else {
			warn("sysctlbyname(%s)", buf);
			return (-1);
		}
	}

	return val;
}

static int
get_epp(const char *arch, int *v, int maxid)
{
	int i, val;

	for (i = 0; i <= maxid; i++) {
		if ((val = get_one_epp(arch, i)) == -2) {
			break;
		} else if (val < 0) {
			return (-1);
		} else {
			v[i] = val;
		}
	}

	return (i);
}

static int
set_one_epp(const char *arch, int cpuid, int prevepp, int currepp)
{
	char buf[64];
	int errfail;

	snprintf(buf, sizeof(buf), "dev.hwpstate_%s.%d.epp", arch, cpuid);
	if (sysctlbyname(buf, NULL, NULL, &currepp, sizeof(currepp)) < 0) {
		errfail = errno;
		if (errfail == ENOENT)
			warnx("sysctlbyname(%s) is unknown", buf);
		else
			warnc(errfail, "%s", buf);

		return (-1);
	}

	printf("dev.hwpstate_%s.%d.epp: %d -> %d\n", arch, cpuid, prevepp, currepp);

	return (0);
}

static int
set_epp(const char *arch, const int *v, int ncpu, int val)
{
	char buf[64];
	int i, j;
	int errfail;

	for (i = 0; i < ncpu; i++) {
		snprintf(buf, sizeof(buf), "dev.hwpstate_%s.%d.epp", arch, i);
		if (sysctlbyname(buf, NULL, NULL, &val, sizeof(val)) < 0) {
			errfail = errno;
			if (errfail == ENOENT) {
				warnx("unexpected end of CPU list at %s", buf);
				return (-1);
			}
			warnc(errfail, "%s", buf);
			/*
			 * Something went wrong here, roll back
			 * previous changes.
			 */

			for (j = 0; j < i; j++) {
				snprintf(buf, sizeof(buf),
				    "dev.hwpstate_%s.%d.epp", arch, j);
				if (sysctlbyname(buf, NULL, NULL, &v[j],
				    sizeof(v[j])) < 0)
					warn("rollback of %s failed", buf);
			}

			return (-1);
		}
	}

	for (i = 0; i < ncpu; i++)
		printf("dev.hwpstate_%s.%d.epp: %d -> %d\n",
		    arch, i, v[i], val);

	return (0);
}

static void
print_one_epp(const char *arch, int cpuid, int currepp)
{
	printf("dev.hwpstate_%s.%d.epp: %d\n", arch, cpuid, currepp);
}

static void
print_epp(const char *arch, const int *v, int ncpu)
{
	int i;

	for (i = 0; i < ncpu; i++)
		print_one_epp(arch, i, v[i]);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-c cpuid] [-h] [-s value]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, archerr = 0, i, maxid, ncpu, retcode = 0, val = 0;
	int cflag = 0, cpu = 0, *cpus = NULL, prevepp = 0;
	char *value = NULL;
	char buf[2][64];
	size_t len = sizeof(maxid), size;
	int *v;
	const char *errstr;
	const char *arch[2] = {"amd", "intel"}, *detected_arch = NULL;
	const int maxval =
#if (__FreeBSD_version < 1600019)
			    100;
#else
			    255;
#endif

	while ((c = getopt(argc, argv, "c:hs:")) != -1) {
		switch (c) {
		case 'c':
			cpu = (int)strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "cpu number %s %s", optarg, errstr);
			cflag++;
			cpus = reallocarray(cpus, cflag, sizeof(*cpus));
			if (cpus == NULL)
				err(1, "reallocarray");
			cpus[cflag - 1] = cpu;
			break;
		case 's':
			value = optarg;
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;

	if (argc > 0)
		usage();

	for (i = 0; i < (int)nitems(arch); i++) {
		size = 0;

		snprintf(buf[i], sizeof(buf[i]), "dev.hwpstate_%s.0.%%desc", arch[i]);

		if (sysctlbyname(buf[i], NULL, &size, NULL, 0) != 0) {
			if (errno == ENOENT) {
				archerr += 1;
			} else {
				err(1, "sysctlbyname(dev.hwpstate_%s.0.%%desc)", arch[i]);
			}
		} else {
			detected_arch = arch[i];
			break;
		}
	}

	if (archerr == (int)nitems(arch))
		errx(1, "there's no attached hwpstate drivers");

	if (value != NULL) {
		val = strtonum(value, 0, maxval, &errstr);
		if (errstr != NULL)
			errx(1, "value %s %s (0-%d)", value, errstr, maxval);
	}

	if (sysctlbyname("kern.smp.maxid", &maxid, &len, NULL, 0) < 0)
		err(1, "sysctlbyname(kern.smp.maxid)");

	for (i = 0; i < cflag; i++)
		if (cpus[i] > maxid)
			errx(1, "cpu %d: no such CPU (max is %d)", cpus[i], maxid);

	if (cflag > 0) {
		for (i = 0; i < cflag; i++) {
			if ((prevepp = get_one_epp(detected_arch, cpus[i])) == -2) {
				warnx("cpuid %d: no hwpstate node", cpus[i]);
				retcode = 1;
				break;
			} else if (prevepp < 0) {
				retcode 1;
				break;
			}
			if (value != NULL) {
				if (set_one_epp(detected_arch, cpus[i], prevepp, val) < 0) {
					retcode = 1;
					break;
				}
			} else {
				print_one_epp(detected_arch, cpus[i], prevepp);
			}
		}

		free(cpus);
		return (retcode);
	}

	v = calloc(maxid + 1, sizeof(*v));
	if (v == NULL)
		err(1, "calloc");

	if ((ncpu = get_epp(detected_arch, v, maxid)) < 0) {
		retcode = 1;
	} else if (ncpu == 0) {
		warnx("hwpstate_%s(4) not attached", detected_arch);
		retcode = 1;
	} else if (value == NULL) {
		print_epp(detected_arch, v, ncpu);
	} else if (set_epp(detected_arch, v, ncpu, val) < 0) {
		retcode = 1;
	}

	free(v);

	return (retcode);
}
