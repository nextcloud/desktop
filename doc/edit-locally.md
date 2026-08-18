<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Edit locally

Edit locally opens a file from the Nextcloud web interface with the locally
synchronised copy. The operation is a client-server handshake followed by the
normal desktop-client local-file workflow. It is not a download of the file
from the web interface.

## 1. The web interface starts an edit request

The **Open locally** action is available for one synchronisable file that the
current user can update. The web interface requests a short-lived token for
the remote path and constructs the desktop URL after receiving it:

```ts
let url = `nc://open/${uid}@` + window.location.host + encodePath(path)
url += '?token=' + result.data.ocs.data.token
window.open(url, '_self')
```

The resulting URL has this shape:

```text
nc://open/<user>@<browser-serialized-host>[:<port>]/<remote-path>?token=<token>
```

The server creates and validates the token; the browser constructs the
`nc://` URL.

## 2. The desktop client receives the URI

The URI scheme handler recognises `nc://open/...` as an
`OpenLocalEdit` request and forwards the original URL to
`EditLocallyManager`. The URI scheme handler does not perform account lookup
or token validation.

`EditLocallyManager::parseEditLocallyUrl()`:

1. Splits the URL path on `/`.
2. Takes the first component as `<user>@<host>[:<port>]`.
3. Treats the remaining components as the remote file path.
4. Reads the `token` query parameter.

The path and token are passed to
`AccountManager::accountFromUserId()`. If no configured account matches, the
client stops before making the server validation request and displays an
account-not-found error.

### IDN host handling

The browser supplies `window.location.host`. Browser URL serialization normally
uses the ASCII-compatible encoding (ACE/Punycode) for internationalized
domains, for example:

```text
Unicode:  cloud.münchen.example
Punycode: cloud.xn--mnchen-3ya.example
```

Qt's `QUrl::host()` returns the configured account host in Unicode form.
`accountFromUserId()` therefore converts the incoming host with
`QUrl::fromAce()` before comparing it, while preserving the username and
optional port. Both Unicode and Punycode links can consequently identify the
same account.

## 3. The desktop client validates the request

Before accessing local files, `EditLocallyVerificationJob` rejects:

- tokens that are not exactly 128 alphanumeric characters;
- empty or non-canonical remote paths; and
- requests for which account lookup returned no account.

For a valid request, it sends a POST request to:

```text
/ocs/v2.php/apps/files/api/v1/openlocaleditor/<token>
```

with the remote path as the `path` query parameter. The server recomputes the
path hash and verifies the authenticated user, path, token, and expiration.
The token is deleted after validation. Only an HTTP 200 response allows the
client to continue.

## 4. The client prepares the local file

After server validation, `EditLocallyManager` starts the platform-specific
local-file job:

- The standard job locates the configured sync folder for the account and
  remote path.
- It checks that the path is not excluded by selective sync.
- For nested paths, it fetches the remote parent metadata when needed.
- It determines whether the parent directory or file needs a targeted sync.
- It waits for an active sync to finish before starting the targeted sync.
- It synchronises the file when the local journal does not contain a current
  copy.

If the server supports file locking, the client attempts to acquire a lock
before opening the file. An existing lock is reported to the user; a lock
failure is reported, but the normal open attempt still proceeds.

On macOS builds with the File Provider module, the client first obtains the
file provider object identifier and attempts to open the file through the File
Provider implementation. If that path is unavailable, it falls back to the
standard local-file job.

## 5. The file is opened

The standard job calls `QDesktopServices::openUrl()` with the local file URL.
The operating system then chooses the registered application, such as a
spreadsheet editor. The loading dialog is removed and any deferred folder sync
is restarted after the open attempt.

Errors can occur at each stage and are surfaced through the tray notification
and, where applicable, a message box. Common causes include an invalid URI,
missing account, invalid or expired token, a path excluded by selective sync,
remote metadata failure, sync failure, or failure to open the local file.

## Tests

The URI parsing behavior is covered by
[`test/testurischemehandler.cpp`](../test/testurischemehandler.cpp).
Account matching and the IDN/Punycode regression are covered by
[`test/testaccountmanager.cpp`](../test/testaccountmanager.cpp), which
generates Punycode from a Unicode hostname with `QUrl::toAce()` and verifies
both representations, including a non-standard port, resolve to the same
account.
