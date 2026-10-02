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
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * The largest value accepted by any kernel.  FreeBSD 16.0 and later take the
 * raw hardware value, 0-255; earlier releases take a percentage, 0-100, and
 * reject anything larger with EINVAL.  __FreeBSD_version did not change when
 * the scale did, so let the kernel decide rather than guess at build time.
 */
#define	EPP_MAX		255

static int
get_one_epp(const char *arch, int cpuid)
{
	size_t size;
	char buf[64];
	int val = 0;

	size = sizeof(val);
	snprintf(buf, sizeof(buf), "dev.hwpstate_%s.%d.epp", arch, cpuid);

	if (sysctlbyname(buf, &val, &size, NULL, 0) < 0) {
		if (errno == ENOENT)
			return (-2);
		warn("sysctlbyname(%s)", buf);
		return (-1);
	}

	return (val);
}

static int
set_epp(const char *arch, const int *ids, const int *old, int n, int val)
{
	char buf[64];
	int errfail, i, j;

	for (i = 0; i < n; i++) {
		snprintf(buf, sizeof(buf), "dev.hwpstate_%s.%d.epp", arch,
		    ids[i]);
		if (sysctlbyname(buf, NULL, NULL, &val, sizeof(val)) < 0) {
			errfail = errno;
			warnc(errfail, "%s", buf);
			if (errfail == EINVAL && i == 0 && val > 100)
				warnx("FreeBSD before 16.0 accepts only 0-100");

			/* Roll back the CPUs already changed. */
			for (j = 0; j < i; j++) {
				snprintf(buf, sizeof(buf),
				    "dev.hwpstate_%s.%d.epp", arch, ids[j]);
				if (sysctlbyname(buf, NULL, NULL, &old[j],
				    sizeof(old[j])) < 0)
					warn("rollback of %s failed", buf);
			}
			return (-1);
		}
	}

	for (i = 0; i < n; i++)
		printf("dev.hwpstate_%s.%d.epp: %d -> %d\n", arch, ids[i],
		    old[i], val);

	return (0);
}

static void
print_epp(const char *arch, const int *ids, const int *v, int n)
{
	int i;

	for (i = 0; i < n; i++)
		printf("dev.hwpstate_%s.%d.epp: %d\n", arch, ids[i], v[i]);
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
	int c, i, j, maxid, n, retcode = 0, val = 0;
	int cflag = 0, cpu = 0, *cpus = NULL;
	char *value = NULL;
	char buf[64];
	size_t len = sizeof(maxid), size;
	int *v;
	const char *errstr;
	const char *arch[2] = {"amd", "intel"}, *detected_arch = NULL;

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

	if (value != NULL) {
		val = strtonum(value, 0, EPP_MAX, &errstr);
		if (errstr != NULL)
			errx(1, "value %s %s (0-%d)", value, errstr, EPP_MAX);
	}

	for (i = 0; i < (int)nitems(arch); i++) {
		size = 0;
		snprintf(buf, sizeof(buf), "dev.hwpstate_%s.0.%%desc", arch[i]);
		if (sysctlbyname(buf, NULL, &size, NULL, 0) == 0) {
			detected_arch = arch[i];
			break;
		}
		if (errno != ENOENT)
			err(1, "sysctlbyname(%s)", buf);
	}
	if (detected_arch == NULL)
		errx(1, "no hwpstate_amd(4) or hwpstate_intel(4) attached");

	if (sysctlbyname("kern.smp.maxid", &maxid, &len, NULL, 0) < 0)
		err(1, "sysctlbyname(kern.smp.maxid)");

	for (i = 0; i < cflag; i++) {
		if (cpus[i] > maxid)
			errx(1, "cpu %d: no such CPU (max is %d)", cpus[i],
			    maxid);
		for (j = 0; j < i; j++)
			if (cpus[j] == cpus[i])
				errx(1, "cpu %d given more than once", cpus[i]);
	}

	if (cflag == 0) {
		cpus = calloc(maxid + 1, sizeof(*cpus));
		if (cpus == NULL)
			err(1, "calloc");
		for (i = 0; i <= maxid; i++)
			cpus[i] = i;
		n = maxid + 1;
	} else
		n = cflag;

	v = calloc(n, sizeof(*v));
	if (v == NULL)
		err(1, "calloc");

	for (i = 0; i < n; i++) {
		if ((v[i] = get_one_epp(detected_arch, cpus[i])) == -2) {
			if (cflag == 0)
				break;
			errx(1, "cpu %d: no EPP control", cpus[i]);
		} else if (v[i] < 0)
			exit(1);
	}
	n = i;

	if (n == 0) {
		warnx("hwpstate_%s(4) provides no EPP control%s", detected_arch,
		    strcmp(detected_arch, "amd") == 0 ?
		    " (is machdep.hwpstate_amd_cppc_enable set?)" : "");
		retcode = 1;
	} else if (value == NULL) {
		print_epp(detected_arch, cpus, v, n);
	} else if (set_epp(detected_arch, cpus, v, n, val) < 0) {
		retcode = 1;
	}

	free(v);
	free(cpus);

	return (retcode);
}
