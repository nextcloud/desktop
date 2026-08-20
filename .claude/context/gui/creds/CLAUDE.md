# src/gui/creds

This folder implements account authentication and credential storage for the desktop client: obtaining a username/password (or app password/OAuth-style token) from the server, persisting it securely in the OS keychain, and re-authenticating GUI dialogs when credentials become invalid. It builds on the credential abstractions in `src/libsync/creds` (`AbstractCredentials`, `HttpCredentials`) and adds the GUI-facing dialogs/flows.

## Key classes/components

- `CredentialsFactory` (`credentialsfactory.h/.cpp`) — factory function `create(type)` that instantiates the right `AbstractCredentials` subclass ("http" → `HttpCredentialsGui`, "webflow" → `WebFlowCredentials`, else `DummyCredentials`).
- `HttpCredentialsGui` (`httpcredentialsgui.h/.cpp`) — GUI variant of basic-auth `HttpCredentials`; shows a `QInputDialog` password prompt (`askFromUser`) and builds the "request an app password" link for older server versions.
- `Flow2Auth` (`flow2auth.h/.cpp`) — implements Nextcloud "Login Flow v2": POSTs to `/index.php/login/v2`, opens the browser (or copies the link) for the user to authorize, then polls the server until an app password is returned; emits `result()`/`statusChanged()`.
- `WebFlowCredentials` (`webflowcredentials.h/.cpp`) — `AbstractCredentials` implementation for the "webflow" auth type; manages username/password/client-cert storage in the keychain (via `KeychainChunk` read/write jobs, chunked to work around Windows credential size limits), injects Basic-Auth headers and client SSL certs through a custom `WebFlowCredentialsAccessManager` (subclass of `AccessManager`), and triggers re-auth (`askFromUser`) by opening `WebFlowCredentialsDialog`.
- `WebFlowCredentialsDialog` (`webflowcredentialsdialog.h/.cpp`) — the `QDialog` shown for (re-)login; hosts either a `Flow2AuthWidget` (Login Flow v2) or, if built `WITH_WEBENGINE`, an embedded `WebView` for the legacy web login flow; forwards captured credentials via `urlCatched()`.

## How it fits together

`CredentialsFactory` picks the credentials class based on the account's stored auth `type`. `WebFlowCredentials` is the modern default; when it needs interactive login it opens `WebFlowCredentialsDialog`, which in turn drives `Flow2Auth` (or the WebEngine view) to obtain a token/app-password, then reports back through `slotAskFromUserCredentialsProvided`, after which `WebFlowCredentials::persist()` writes everything to the keychain via `KeychainChunk` jobs. `HttpCredentialsGui` is a simpler legacy path used for plain HTTP basic auth.

## Fork-specific notes

- `WebFlowCredentialsDialog` styles itself via `whitelabeltheme.h`/`WLTheme` (dialog background, fonts, colors) instead of upstream Nextcloud's default Qt styling — this is BRICKMAKERS whitelabel-branding code.
- `Flow2Auth`, `HttpCredentialsGui`, `WebFlowCredentials`, and `CredentialsFactory` are otherwise essentially unmodified upstream Nextcloud/ownCloud logic (Login Flow v2, keychain persistence, HTTP basic auth).

*Quelle: src/gui/creds — Stand 2026-08-17, automatisch erstellt, bitte gegenlesen.*
