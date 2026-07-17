# Compiling HiShare

HiShare builds on **Haiku** (R1/beta5 or newer, x86_64). MUSCLE is bundled, so
there is nothing extra to download.

## Quick start

```
cd source/hishare
make
```

This produces the `HiShare` executable in `source/hishare/`. Run it from there,
or install it (see Packaging below).

## Make targets

| Target          | What it does                                                        |
|-----------------|---------------------------------------------------------------------|
| `make`          | Build the `HiShare` binary (release).                               |
| `make debug`    | Build with debug info.                                              |
| `make catalogs` | Regenerate the localization `.catkeys` / `.catalog` files from the built-in string tables (needs `python3` + `collectcatkeys`/`linkcatkeys`). |
| `make clean`    | Remove object files and the built executables.                     |

## Layout

```
HiShare/
  source/
    hishare/     <- the application (run "make" here)
    muscle/      <- the bundled MUSCLE 6.11 library (referenced as ../muscle)
  README.md
  COMPILING.md
  CHANGELOG.md
```

The Makefile references MUSCLE via `../muscle`, so keep `hishare/` and `muscle/`
side by side under `source/`.

## Dependencies

All are part of a stock Haiku install:

- Toolchain: `gcc`/`g++` (the default Haiku x86_64 compiler).
- Libraries linked: `be`, `network`, `bnetapi`, `translation`, `localestub`,
  and — for the (currently disabled) TLS code — `ssl` / `crypto`.
- Localization tools (only for `make catalogs`): `collectcatkeys`, `linkcatkeys`,
  `python3`.

## Build flags of note

The Makefile already sets what Haiku needs:

- `-D_BSD_SOURCE`, and on x86_64 `-DMUSCLE_64_BIT_PLATFORM` (without it MUSCLE
  truncates pointers on 64-bit).
- `-DMUSCLE_AVOID_IPV6 -DMUSCLE_USE_PTHREADS` (IPv6 is not enabled yet in 1.0).
- TLS is **off by default** in 1.0: the build omits OpenSSL entirely (no
  `-DMUSCLE_ENABLE_SSL`, no `-lssl`/`-lcrypto`, SSL objects excluded), so the
  binary has **no openssl3 dependency** and the package installs on any Haiku.
  Build with `make ENABLE_SSL=1` to compile the TLS code back in and link OpenSSL
  (also flip `BESHARE_TLS_ENABLED` to `1` in `ShareConstants.h` to expose the UI)
  — only worthwhile once the SSL client-path crash is fixed.

## App identity

- App signature: `application/x-vnd.HiShare`
- Settings: `~/config/settings/hishare_settings` and `hishare_user_key`
- Data folder: `/boot/home/HiShare/` (shared / downloads / logs). Override with the
  `BESHARE_HOME` environment variable for portable or multi-instance setups.

## Packaging

To build an `.hpkg`, stage the `HiShare` binary plus the non-English catalogs
(`data/locale/catalogs/x-vnd.HiShare/…`) and a `.PackageInfo`, then run
`package create`. (The 1.0 recipe under `pkg/` is being updated from the old
BeShare 3.04 one.)
