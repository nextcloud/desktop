## Patches used

There are our patches on top of Qt 5.4.0, which we are currently
using for our binary packages on Windows and Mac OS. Most of them
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
* 0004-OSX-Fix-disapearing-tray-icon.patch
  (TODO: actual patch slighly differs)
* 0006-QNAM-Fix-upload-corruptions-when-server-closes-conne.patch
  (TODO: Actual patch on build machine spans over two commit but is identical)

### Part of Qt v5.5.1 and later
* 0008-QNAM-Fix-reply-deadlocks-on-server-closing-connectio.patch
  (TODO: actual patch has different name)

### Upstreamed but not in any release yet (as of 2015-11-16)
* 0009-QNAM-Assign-proper-channel-before-sslErrors-emission.patch
* 0011-Make-sure-to-report-correct-NetworkAccessibility.patch
* 0012-Make-sure-networkAccessibilityChanged-is-emitted.patch
* 0013-Make-UnknownAccessibility-not-block-requests.patch

### Not submitted to be part of any release:
0005-Fix-force-debug-info-with-macx-clang_NOUPSTREAM.patch


