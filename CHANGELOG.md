# Changelog

## HiShare 1.0-3 (July 2026)

### Networking
- **Connect-back fallback for downloads**: when a direct TCP connection to a peer
  that advertises itself as non-firewalled never establishes (stale/wrong public
  address, CGNAT, broken port forwarding), the download is automatically retried
  once in accept/connect-back mode — the peer connects out to us instead. Applies
  only when we are reachable ourselves; user-cancelled transfers are not retried.

### Interface
- The header banner now shows the **number of shared files** (before the public
  IP:port indicator), updated live as files are added/removed or sharing is toggled.
- The Connect/Disconnect quick-action button uses new **HVIF vector icons**: red
  power symbol while connected (click disconnects), green while disconnected
  (click connects).
- **Dark-theme fixes**: transfer-row text now uses the themed text colour instead
  of fixed black; the scroll-corner square between scrollbars follows the system
  palette; the white 1-px bevel lines on column headers and transfer rows are gone
  (`B_LIGHTEN_MAX_TINT` bleaches any colour to pure white — highlights now lighten
  moderately on dark backgrounds).

### Miscellaneous
- Desktop notifications rebranded: groups "HiShare Downloads" / "HiShare Chat"
  (previously still "BeShare ...").

## HiShare 1.0 (2026)

HiShare 1.0 is the modernized edition of **BeShare 3.04**. The MUSCLE wire protocol
is unchanged, so HiShare interoperates with existing BeShare servers and clients.

### New identity
- Renamed to **HiShare**, version **1.0**, signature `application/x-vnd.HiShare`.
- Own settings (`hishare_settings`, `hishare_user_key`) and data folder
  (`/boot/home/HiShare/`). HiShare does **not** import old BeShare settings — it is
  a clean, separate app. The `BESHARE_HOME` env var overrides the data folder.
- Rebranded window title, About box, notifications and all UI strings (20 languages);
  the network client name reported to servers is "HiShare".

### Networking & reachability
- **Automatic router port-forwarding**: PCP → NAT-PMP → UPnP IGD, with lease renewal
  and clean removal on quit. Auto-manages the "I'm Firewalled" state.
- **External reachability probe** that detects private/CGNAT WAN addresses and shows
  your real public IP:port in the header and title, with a desktop notification.
- Thread-safe DNS (`getaddrinfo`) fix for concurrent connect + probe.

### Interface
- **Header banner**: app icon, your user name, live "Connected / Connecting… /
  Offline" status with a coloured dot, plus built-in quick-action buttons
  (Connect/Disconnect, Settings, Colours). Theme-aware (subtle gradient from the
  system palette).
- **Categorised Settings window** (Network / Transfers / Interface / Chat) replacing
  the old 24-item Settings menu; reuses the existing commands and persisted state.
- **Theme-correct colours everywhere**: chat, lists, column headers, the colour
  picker, scroll corners, transfer rows and URL tool-tips now derive from
  `ui_color()` and re-theme live on light/dark changes.
- **Custom-colours fix**: choosing a colour in the picker re-enables custom colours
  automatically; a "Use custom colours" checkbox was added; user-picked colours now
  survive a system theme change instead of being overwritten.
- Drag-and-drop files onto the window to share them; desktop notifications for
  finished downloads, private messages and mentions; numeric % on transfers.

### Localization
- The UI follows the **Haiku system language** through the Locale Kit (catkeys);
  the old per-app Language menu was removed. Fingerprint/catalog updated for the
  new signature.

### Under the hood
- **MUSCLE 3.20 → 6.11**, soak-tested (repeated 100 MB+ two-peer transfers, all
  hash-verified, no leaks/crashes).
- Byte-range (`maxbytes`) transfer extension — the protocol foundation for future
  multi-source (swarming) downloads. Capability-negotiated (`supports_ranges`), no
  regression for ordinary transfers.
- Component-wise version comparison; the BeShare-version update nag is disabled
  (HiShare has its own version line).

### Known limitations
- **TLS encrypted transfers are disabled and excluded from the build.** A crash on
  the SSL *client* path during a real transfer is still being fixed, so for 1.0 the
  feature is off at runtime (`BESHARE_TLS_ENABLED` in `ShareConstants.h`) **and** the
  build omits OpenSSL entirely — no `-DMUSCLE_ENABLE_SSL`, no `-lssl`/`-lcrypto`, and
  the SSL objects are dropped from the link (Makefile `ENABLE_SSL`, default `0`). As a
  result the binary has **no openssl3 dependency**, so the package installs on any
  Haiku (openssl3 is not part of a default install). Rebuild with `make ENABLE_SSL=1`
  (and flip `BESHARE_TLS_ENABLED` to `1`) to compile the TLS code back in once fixed.
- **IPv6** is not enabled yet (`-DMUSCLE_AVOID_IPV6`).
- Swarming (multi-source downloads) has the protocol foundation but no scheduler yet.
- Already-drawn chat text keeps its old colours until restart after a live theme change.

### Credits
Original BeShare by Jeremy Friesner & Vitaliy Mikitchenko; later updates by BBJimmy,
Pete, AGMS and others. MUSCLE by Jeremy Friesner / Meyer Sound. Haiku modernization
(HiShare) by atomozero.
