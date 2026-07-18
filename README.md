<p align="center">
  <img src="hishare-icon.png" width="128" alt="HiShare icon">
</p>

# HiShare

Native Haiku file-sharing and chat client — the modernized edition of the classic
[BeShare](https://public.msli.com/lcs/beshare/) 3.04. It speaks the same MUSCLE
protocol, so it interoperates with existing BeShare servers and clients, while
adding automatic router port-forwarding, a theme-aware modern GUI, and a
refreshed build on top of MUSCLE 6.11.

> It all started as an update to BeShare 3.04 and grew into its own edition.

![HiShare main window](screenshots/hishare-main.png)

If HiShare saves you time, consider supporting development: [![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-atomozero-yellow?logo=buymeacoffee)](https://buymeacoffee.com/atomozero)


## Features

* Share and download **any** type of file over a MUSCLE server, with live
  queries — new matching files appear in your results as they are shared,
  no refresh needed
* **Multi-server**: connect to up to 8 MUSCLE servers at once — queries and
  results are aggregated, chat is broadcast (incoming lines are tagged with
  their server of origin), transfers and private messages follow their
  peer's server, and a Server column appears automatically in the lists.
  Extra connections are remembered across restarts.
* Browse files with their Haiku **attributes**, like a Tracker view
* Built-in **chat**, private messages and user watching
* Any number of simultaneous uploads/downloads, serialized per-host for
  efficiency, with resume support
* **Automatic router port-forwarding**: UPnP IGD, NAT-PMP and PCP are tried
  automatically so you are reachable from outside your home NAT without
  touching the "I'm Firewalled" switch
* **External reachability probe** that detects CGNAT and shows your real
  public IP:port in the header banner, next to your shared-file count
* **Connect-back fallback**: if a direct connection to a peer with a stale
  or unreachable advertised address fails, the download is retried
  automatically through a server-mediated connect-back
* **Modern, theme-aware GUI**: status header banner with live connection
  state and quick-action buttons (Connect/Disconnect, Settings, Colours),
  a categorised Settings window, and colours derived from the Haiku system
  palette — light and dark themes respected everywhere, re-themed live
* Drag & drop files or folders onto the window to share them
* Desktop notifications for finished downloads, private messages and mentions
* Localized via the Haiku Locale Kit (~20 languages) — the UI follows your
  system language automatically
* **MUSCLE 6.11** under the hood (upgraded from 3.20), soak-tested with
  repeated multi-hundred-MB transfers
* No external dependencies beyond Haiku system libraries — MUSCLE is bundled

## Quick start

```
cd source/hishare
make
./HiShare
```

Pick a server (a public one is preselected), type a user name and click
**Connect** — or just use the power button in the header banner. Then:

- Type a query (e.g. `*.hpkg`) and click **Start Query** to search files
- Select results and click **Download Selected Files** (or drag them out)
- Drop files or folders onto the window to share your own
- Click the **sliders icon** for the Settings window, the **palette icon**
  for colours
- The header banner shows your connection state, how many files you share,
  and your public address with a ✓ when the reachability probe confirms
  you are reachable from the internet
- **File → Connect to additional server…** joins another MUSCLE server on
  top of the current one; **File → Connections** lists every connection
  with per-server Connect/Disconnect/Remove actions

### Firewalls and NAT

HiShare maps a port on your router automatically (UPnP/NAT-PMP/PCP) and
verifies it with a real reachability probe; the "I'm Firewalled" flag is
managed for you. If your router supports none of those protocols, enable
**"I'm behind a firewall"** manually (File menu or Settings → Network):
peers that are *not* firewalled can then still download from you via
connect-back. Two firewalled peers cannot exchange files — that is a
protocol-level limitation.

## Build

Requires Haiku (R1/beta5 or newer, x86_64) with GCC and standard system
libraries. MUSCLE is bundled under `source/muscle/` — nothing to download.

```
cd source/hishare
make            # builds the HiShare binary
make debug      # build with debug info
make catalogs   # regenerate localization catalogs (needs python3)
make clean      # removes all build artifacts
```

See [`COMPILING.md`](COMPILING.md) for details.

### Translations

The UI follows the Haiku system language (Preferences → Locale). String
tables live in the source; catalogs are generated into
`source/hishare/locale/catalogs/` (`.catkeys` sources and compiled
`.catalog` files, both committed so you can build without extra tools).
After changing strings, run `make catalogs` to regenerate them.

## Protocol conformance

HiShare implements the BeShare/MUSCLE wire protocol and interoperates with
stock BeShare clients and servers — same queries, chat, transfers and
connect-back mechanism.

* Transfers between two non-firewalled peers connect directly; a firewalled
  peer is reached via a server-mediated **connect-back** request. HiShare
  adds an automatic connect-back **fallback** when a peer advertises an
  address that turns out to be unreachable (CGNAT, broken port forwarding).
* A **byte-range transfer extension** (`supports_ranges`) is negotiated per
  peer — the protocol foundation for future multi-source (swarming)
  downloads, with no regression for ordinary transfers.
* **TLS**: encrypted transfers are present in the codebase but disabled for
  1.0 (a crash on the SSL client path is still being fixed) and excluded
  from the build, so the binary has no OpenSSL dependency. Rebuild with
  `make ENABLE_SSL=1` to experiment. See `CHANGELOG.md`.

## Be careful
> **Developer's Note**: This software may contain traces of peanuts and LLM. It has been developed with passion for the Haiku platform.

## Lineage & credits

HiShare is built on BeShare, originally by **Jeremy Friesner** and **Vitaliy
Mikitchenko**, with later updates by BBJimmy, Pete, AGMS and others. The MUSCLE
messaging library is by Jeremy Friesner / Meyer Sound. Haiku modernization
(HiShare) by **atomozero**.

The application icon is the classic BeShare "flower" from the community
[HVIF Store](https://www.hvif-store.art) (MIT-licensed), keeping the visual
lineage.

BeShare, MUSCLE and the Santa's Gift Bag GUI classes are used under their
original licenses; see [`LICENSE`](LICENSE) for the full overview.

## Support

If you find this project useful, you can buy me a coffee: [![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-atomozero-yellow?logo=buymeacoffee)](https://buymeacoffee.com/atomozero)
