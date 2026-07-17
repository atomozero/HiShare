# BeShare — Security audit (file-serving / path handling)

Scope: whether a remote peer can make BeShare read or write files outside the
shared / downloads directories (directory traversal), via the MUSCLE file
transfer protocol.

## Result: no directory-traversal vulnerability found

### Serving files to a downloader (upload side) — SAFE BY DESIGN
When a peer requests a file, `ShareFileTransfer` resolves the requested name via
`ShareNetClient::FindSharedFile(name)` (ShareNetClient.cpp), which is a flat
lookup in the `_nameToEntry` hashtable:

```
CountedEntryRef * ref = _nameToEntry.Get(fileName);
```

`_nameToEntry` is populated **only** by the local share scanner
(`RefFilename()`), never from network input. A peer can therefore only obtain an
`entry_ref` for a name BeShare has already enumerated and advertised; an
unknown/crafted name (e.g. `../../etc/passwd`) is simply not in the table, so
`FindSharedFile` returns an empty ref and the subsequent `BEntry::Exists()` /
`BFile::SetTo()` fails. No path is constructed from remote input on the serving
path.

Note: a symlink the *local user* places inside their shared folder can point
outside it — that is the user's own choice, not a remote attack vector.

### Receiving files (download side, "retain file paths") — GUARDED
When `GetRetainFilePaths()` is enabled, a sender-supplied `beshare:Path` is used
to recreate a subdirectory tree under the downloads directory
(ShareFileTransfer.cpp ~955). Before use it passes through `CheckPath()`:

```
static void CheckPath(String & path) {
  if ((path()[0]=='/')||(path.StartsWith("../"))||(path.EndsWith("/.."))
      ||(path.IndexOf("/../")>=0)||(path.Equals(".."))) path = "";
}
```

This "fails closed": any absolute path or any `..` traversal component blanks the
whole path, so writes stay within the downloads directory. `CheckPath()` is also
applied to the path fields at ShareFileTransfer.cpp:514 and :1175.

## Recommendation
No code change required — the serving path is safe by construction and the
receiving path is guarded. If desired as defense-in-depth, `CheckPath()` could be
tightened to per-component filtering, but the current fail-closed behaviour is
sound; changing it risks regressions for no security gain.

_Audited against BeShare 3.04 + the UPnP/NAT-PMP branch._
