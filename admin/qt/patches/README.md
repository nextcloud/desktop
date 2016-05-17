## Patches used

There are our patches on top of Qt 5.4.0, which we are currently
using for our binary packages on Windows and Mac OS X. Most of them
have been sent upstream and are part of newer Qt releases.

All changes are designed to up upstream, and all those that are
special hacks to Qt will bear a NOUPSTREAM in their name

The git-style numeration is ordered by order of creation, their
purpose is outlined in each patches' front matter.

### Part of Qt v5.4.1 and later
* 0001-Fix-crash-on-Mac-OS-if-PAC-URL-contains-non-URL-lega.patch
* 0002-Fix-possible-crash-when-passing-an-invalid-PAC-URL.patch
* 0003-Fix-crash-if-PAC-script-retrieval-returns-a-null-CFD.patch

### Part of Qt v5.4.2 and later
* 0004-Cocoa-Fix-systray-SVG-icons.patch
* 0005-OSX-Fix-disapearing-tray-icon.patch
* 0007-QNAM-Fix-upload-corruptions-when-server-closes-conne.patch
* 0018-Windows-Do-not-crash-if-SSL-context-is-gone-after-ro.patch

### Part of Qt v5.5.0 and later
* 0017-Win32-Re-init-system-proxy-if-internet-settings-chan.patch

### Part of Qt v5.5.1 and later
* 0007-X-Network-Fix-up-previous-corruption-patch.patch
* 0008-QNAM-Fix-reply-deadlocks-on-server-closing-connectio.patch
* 0014-Fix-SNI-for-TlsV1_0OrLater-TlsV1_1OrLater-and-TlsV1_.patch
* 0016-Fix-possible-crash-when-passing-an-invalid-PAC-URL.patch
* 0011-Make-sure-to-report-correct-NetworkAccessibility.patch

### Part of Qt v5.5.2 (UNRELEASED!)
* 0009-QNAM-Assign-proper-channel-before-sslErrors-emission.patch
* 0010-Don-t-let-closed-http-sockets-pass-as-valid-connecti.patch
* 0012-Make-sure-networkAccessibilityChanged-is-emitted.patch

### Part of Qt v5.6 and later
* 0009-QNAM-Assign-proper-channel-before-sslErrors-emission.patch
* 0010-Don-t-let-closed-http-sockets-pass-as-valid-connecti.patch
* 0011-Make-sure-to-report-correct-NetworkAccessibility.patch
* 0012-Make-sure-networkAccessibilityChanged-is-emitted.patch
* 0013-Make-UnknownAccessibility-not-block-requests.patch
* 0019-Ensure-system-tray-icon-is-prepared-even-when-menu-bar.patch

### Part of Qt 5.7 and later
* 0015-Remove-legacy-platform-code-in-QSslSocket-for-OS-X-1.patch

### Not submitted upstream to be part of any release:
* 0006-Fix-force-debug-info-with-macx-clang_NOUPSTREAM.patch
This is only needed if you intent to harvest debugging symbols
for breakpad.


