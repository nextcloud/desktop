<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Blocking synchronization

How to stop the extension talking to the server, and what that does and does not cover.

## Overview

Classic sync folders can be paused. The file provider path had no equivalent, and this is it: a
process-global `blockSync` boolean in `UserDefaults.standard`, which under the extension resolves to
the `com.nextcloud.desktopclient.FileProviderExt` domain.

```
defaults write com.nextcloud.desktopclient.FileProviderExt blockSync -bool true
defaults delete com.nextcloud.desktopclient.FileProviderExt blockSync
```

It takes effect immediately, without restarting the extension. When the key is absent — or holds
anything which is not a boolean — synchronization is not blocked. That holds in every build
configuration: a debug build must not synchronize differently from a release build, and a value
which cannot be read is not a reason to stop synchronizing.

## What blocking does

**The extension performs no network input or output with the server.** Every request from the system
which would need it is refused with `NSFileProviderErrorServerUnreachable`. That is not a fiction:
as far as the file provider framework is concerned the server genuinely is unreachable, and the
framework already knows how to behave — it presents the situation to the user, throttles the items
it could not sync, and retries once the extension says the trouble has passed.

Five requests are refused:

| Request | Direction |
| --- | --- |
| ``FileProviderExtension/enumerator(for:request:)`` | remote changes arriving |
| ``FileProviderExtension/fetchContents(for:version:request:completionHandler:)`` | downloads |
| ``FileProviderExtension/createItem(basedOn:fields:contents:options:request:completionHandler:)`` | uploads |
| ``FileProviderExtension/modifyItem(_:baseVersion:changedFields:contents:options:request:completionHandler:)`` | uploads |
| ``FileProviderExtension/deleteItem(identifier:baseVersion:options:request:completionHandler:)`` | uploads |

Refusing to provide an enumerator is what stops remote changes arriving, and it is worth
understanding why one gate covers both the push and the polling path. Change discovery happens in
the main app — the websocket for push notifications, the root ETag poll otherwise — but what the app
sends the extension carries no data. It signals the working set, the framework invalidates it, and
the framework then asks the extension for an enumerator. Declining that request declines everything
which was discovered, whichever way it was discovered, and the app needs to know nothing about it.

Looking an item up is deliberately **not** refused. It is answered from the local database, so the
Finder keeps showing the name and size of everything already known. Classic sync does not hide what
it has while it is paused, and neither should this.

## What blocking does not do

**Work already under way is not cancelled.** Blocking refuses new requests; it does not reach into
accepted ones. An upload which began a second before the key was set finishes uploading. Code which
needs the extension to be quiet rather than merely uncooperative should set the key and then wait
for the reported sync state to settle.

**Content already on disk stays readable.** For a replicated extension the system serves materialized
files from its own copy without involving the extension at all, so reading what has already been
downloaded keeps working. Only a file which still has to be fetched fails.

**A folder never visited before cannot be browsed.** Its contents have to be enumerated to be shown,
and enumeration is refused. Already-enumerated folders are unaffected.

**Thumbnails still reach the server.** Thumbnail requests take their own path and are not gated. This
is a known gap rather than a decision.

## Unblocking

Removing the key is enough. The extension notices through a key-value observation, releases the items
the framework throttled by calling `signalErrorResolved(_:completionHandler:)`, and signals the
working set so that whatever changed on the server in the meantime is enumerated.

The refusals themselves do not depend on that observation — they read the value directly, each time.
If the notification is ever missed the only consequence is that recovery waits for whatever the
system would have done next. Blocking can be left on by accident; it cannot be left on by failure.

## A note on the errors

`NSFileProviderErrorServerUnreachable` is documented as a back-off error for enumeration, fetching
and deletion: the system reports it, stops asking, and waits to be signalled. For creation and
modification Apple's documentation lists a different set, so there the error is treated as transient
and the request is retried instead of throttled. Retries are cheap, because the refusal happens
before any network call is made, but it does mean a long block costs some repeated work. The
alternatives are worse and were rejected deliberately: `NSFileProviderErrorCannotSynchronize` is not
retried until the item is touched again, which would leave changes stranded after unblocking, and
`NSFileProviderErrorExcludedFromSync` makes the system delete the item.
