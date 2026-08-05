# Changelog

## HiShare 1.2 (August 2026)

A focused pass on the multi-server experience, plus interface polish.

### Multi-server identity & connections
- **Your name and status now reach every connection.** They were only applied to
  the primary connection, so on secondary servers you showed up under the client
  default name (e.g. "binky"). Name and status are now published on all
  connections, and a connection added at runtime inherits them.
- **HiShare never publishes an empty name**, and a peer that publishes one is
  shown as *Anonymous* instead of a blank row.
- **Connect to all known servers** (File menu) — brings up every server in the
  known-server list in one action, skipping loopback/localhost, your own public
  IP, and servers you're already on.
- **Duplicate connections are skipped.** Two connections that resolve to the same
  box (e.g. the same server by hostname and by IP) no longer open two sessions
  that make you appear multiple times and can get you dropped — the second stays
  idle with a note in the log.

### Speed & sharing
- **Auto-detect connection speed** (Settings → Transfers, opt-in) — passively
  measures your real upload and download throughput during transfers, advertises
  the upload rate as your bandwidth, and shows both in the header (`↓ … ↑ …`).
- **Correct shared-file count** in the header — it no longer flashes "0 files
  shared" while a connection is reconnecting (it reports the largest count any
  connection holds).
- The bogus **"couldn't find shared folder"** error no longer appears when a
  share scan is simply superseded by a reconnect.

### Interface
- **Search row aligned** — the Start/Stop query buttons are no longer taller than
  the fields and menus beside them.
- **Header connection status fixed** — it shows green "Connected" as soon as any
  server is up (amber "Connecting…" only while actually establishing one), instead
  of getting stuck on "Connecting…" whenever one configured connection was idle.
- **Hide own echoed messages** (Settings → Chat, on by default) — a public message
  you send is no longer shown once per server it was echoed back from.
- More of the interface is **localized** (menu items, header, Settings).

## HiShare 1.1-6 (August 2026)

Interface polish and localization follow-ups.

### Interface
- **Nameless users are gone.** A peer that published an empty name (e.g. a
  client whose user cleared their name field) used to appear as a **blank row**
  in the users list. Two fixes: incoming — such a peer now shows as *Anonymous*
  instead of an empty cell; outgoing — HiShare itself **never publishes an empty
  name** anymore (it falls back to the default, so your own client can no longer
  appear nameless to others).
- **Carico / Load column no longer clips.** The right-justified load value
  (e.g. `(0/20) 0%`) was chopped on the **left** — losing the leading `(` — when
  the column was narrower than its content. It now keeps a minimum width that
  fits its widest realistic value, enforced even over a saved column width
  (the value is atomic and can't be sensibly abbreviated).

### Localization
- The **header subtitle** is now localized — *"Connesso a N server · N file
  condivisi"* instead of the previous English text — with correct Italian
  singular/plural.
- The **reachability line** in Settings → Network (*"Raggiungibile…"* /
  *"Non raggiungibile…"* / *"Raggiungibilità non ancora verificata."*) is
  localized.

## HiShare 1.1-5 (August 2026)

Fixes the crash-on-quit that 1.1-4's watchdog only masked.

### Robustness
- **Fixed a SIGSEGV on quit** (thread-dump-confirmed). While tearing down a
  server connection, `~ShareNetClient` runs `DisconnectFromServer()` *after* the
  handler has already been detached from the window, so `Looper()` is `NULL`;
  the code then dereferenced it (`((ShareWindow*)Looper())->SharesScanComplete()`
  via `EndScanSharesBatch`, plus `SetQueryInProgress` / `SetConnectStatus` /
  `UpdateTitleBar`), crashing on a `NULL` `this`. The crashed window thread was
  then held alive by the debug server, which is what looked like a "hang after
  the window closed". All of `ShareNetClient`'s window callbacks reached during
  destruction now null-check `Looper()` first. The 1.1-4 quit watchdog stays as
  a belt-and-suspenders guarantee.

## HiShare 1.1-4 (August 2026)

Interface polish, full Italian localization, and a shutdown-hang safeguard.

### Interface
- **Streamlined header bar.** The separate Server / Name / Status row is gone,
  reclaiming a full row: the **Server field moved onto the search row**, and
  **Name and Status editing moved into the Settings window** (new *Profile*
  card). The results list now sits directly under the header banner.
- **Quick status menu in the header** — a flat "dot + text" chip next to the
  connection indicator (Here / Away / Custom…), kept in sync with `/status`,
  `/away` and auto-away. Its dot is **blue** (vs. the green connection dot) so
  the two adjacent indicators read as distinct.
- **Typing a new server address + Enter now (re)connects** to it while already
  connected, instead of only updating the labels — no need to disconnect first.
  An unchanged address doesn't needlessly drop the current connection.
- **Alignment fixes**: the search-row controls (menus, fields, buttons) now
  share one baseline; result-list column headers are no longer truncated
  (e.g. "Dimensione" instead of "Dime…").

### Localization
- The header states, the empty-transfers hint, the **Information** button and
  the **entire Settings window and prompt dialogs** are now localized (Italian
  provided; other languages fall back to English).
- The string resolver gained a **universal English fallback**, so a string
  missing from a language table shows English instead of nothing.

### Robustness
- **No more hang on quit.** Settings are now saved *before* the network/thread
  teardown (so a stalled router/socket can't cost you your settings), and a
  watchdog guarantees the process exits even if a teardown thread stalls — the
  app can no longer appear to "hang" after its window has closed.

## HiShare 1.1-3 (July 2026)

Automatic router port-forwarding fixes (from a field report where the mapping
was never created behind a Firewalla/Hyper-V setup):

- **Mapping conflicts are handled**: when the router already forwards our port
  to another host, the UPnP `AddPortMapping` now reads the SOAP `errorCode` and,
  on a conflict (718, or 501/402/729 as some routers report it), retries on
  alternative external ports; the port the router actually granted is then
  advertised to peers.
- **More robust SSDP discovery**: the M-SEARCH is sent out of the correct
  interface (`IP_MULTICAST_IF`/`TTL`), send failures are detected instead of
  silently ignored, and every announced device LOCATION is tried until one
  serves a usable description (routers sometimes advertise URLs they don't serve).
- **Clearer diagnostics**: when a router answers discovery but its UPnP control
  service is dead, HiShare says so (and suggests a router reboot) instead of the
  misleading "no router found"; and the repeated once-a-minute retry warning is
  no longer spammed to the chat log when nothing changed.
- The Settings window's reachability line now updates live when a probe verdict
  arrives, instead of showing a stale snapshot.

## HiShare 1.1-2 (July 2026)

Packaging-only revision, no code changes:

- The bundled documentation now includes the updated README (multi-server
  section) alongside CHANGELOG and LICENSE.
- The post-install script ships with its executable bit set (it silently
  failed to run in earlier packages; harmless, as the app creates its data
  folders on demand).

## HiShare 1.1-1 (July 2026)

### Multi-server support

HiShare can now be connected to up to **8 MUSCLE servers at once**:

- **File → Connect to additional server…** opens a prompt for the server
  address; the **Connections** submenu lists every connection with its state
  and per-connection **Connect / Disconnect / Remove** actions.
- Extra connections are **persisted** and re-created at the next startup;
  "Connect" (menu, power button) brings every offline connection online.
- **Queries** run on all connected servers and the results are aggregated;
  a server that (re)connects while a query is live joins it immediately.
- **Chat** is broadcast to every connected server; with more than one
  connection, incoming chat is tagged with its server of origin
  (`[servername]`), and private messages/pings go through the target user's
  own server. Users are keyed per-connection, so identical session IDs on
  different servers never collide.
- **Transfers** are bound to their peer's server connection end-to-end:
  downloads, connect-back requests, the connect-back retry fallback,
  restarts, restored-from-archive transfers (the server is remembered by
  name) and inbound uploads (bound when the peer identifies itself).
- **Header banner** shows "Connected to N servers" with a per-server ✓/…/✗
  tooltip; the status dot is green only when every connection is up.
- A **Server column** appears automatically in the users and results lists
  whenever more than one connection exists (and hides again at one).
- Per-connection state and **auto-reconnect** (each connection retries with
  its own backoff). A dropped connection takes only its own users and
  results with it; single-server behaviour is unchanged throughout.

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
