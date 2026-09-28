# eppctl

`eppctl` displays and sets the Energy/Performance Preference (EPP) of
AMD CPUs with Collaborative Processor Performance Control (CPPC) or
Intel CPUs with Hardware-Controlled Performance States (HWP) on FreeBSD.
It uses the `dev.hwpstate_amd.N.epp` or `dev.hwpstate_intel.N.epp` sysctls
provided by the `hwpstate_amd(4)` and `hwpstate_intel(4)` driver
correspondingly, and applies one value to every CPU at once.

## Requirements

- FreeBSD with `hwpstate_amd(4)` or `hwpstate_intel(4)` attached
  (AMD CPU with CPPC enabled or Intel CPU with HWP)
- root privileges to change the setting

## Build and install

```sh
make
make install
```

## Usage

```
eppctl [-h] [-s value]
```

Without options, `eppctl` prints the current EPP of every CPU:

```
$ eppctl
dev.hwpstate_intel.0.epp: 100
dev.hwpstate_intel.1.epp: 100
```

`-s value` sets every CPU to `value` and prints the old and new values:

```
# eppctl -s 0
dev.hwpstate_intel.0.epp: 100 -> 0
dev.hwpstate_intel.1.epp: 100 -> 0
```

`value` ranges from 0 (most performant) to 255 (most energy efficient).
On FreeBSD 14 and 15 the kernel uses a percentage instead, and accepts
only 0 to 100.

If any CPU cannot be updated, `eppctl` restores the CPUs it has already
changed to their previous values and exits with status 1.

## See also

`eppctl(8)`, `hwpstate_amd(4)`, `hwpstate_intel(4)`, `sysctl(8)`

## External links

[Collaborative Processor Performance Control (CPPC)](https://www.freebsd.org/status/report-2026-01-2026-03/cppc/)
