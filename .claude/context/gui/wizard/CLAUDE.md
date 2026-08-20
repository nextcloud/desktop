# src/gui/wizard

The "new connection" / account-setup wizard: a `QWizard` (`OwncloudWizard`) that walks a user
from a welcome screen through server URL entry, credential/login, optional data-protection
consent, terms-of-service acceptance, and local/remote folder configuration to finish adding an
account. Pages are plain `QWizardPage` subclasses registered by page ID (see
`WizardCommon::Pages` in owncloudwizardcommon.h) and are owned/created by `OwncloudWizard`.

## Key classes/components

- **owncloudwizard.h/.cpp** — `OwncloudWizard`: the `QWizard` container; owns and registers all
  pages, holds shared state (account, client cert/key data, flow/VFS flags), relays signals
  (`determineAuthType`, `connectToOCUrl`, `createLocalAndRemoteFolders`, `basicSetupFinished`).
- **owncloudwizardcommon.h/.cpp** — `WizardCommon` namespace: shared `Pages` enum (page-ID
  ordering), style helpers (title templates, error/hint label styling) used by all pages.
- **welcomepage.h/.cpp/.ui** — `WelcomePage`: first page; branding slideshow + "Log in" /
  "Create account" buttons; picks next page (server setup, or webview for registration).
- **owncloudsetuppage.h/.cpp** + **owncloudsetupnocredspage.ui** — `OwncloudSetupPage`: server
  URL entry, proxy settings button, client-certificate handling (`AddCertificateDialog`),
  triggers `DetermineAuthTypeJob` and branches `nextId()` to HTTP creds / Flow2 / WebView page.
- **owncloudconnectionmethoddialog.h/.cpp/.ui** — small modal shown when a plain-HTTP/self-signed
  server is detected, letting the user choose No-TLS vs. client-side TLS vs. going back.
- **abstractcredswizardpage.h/.cpp** — `AbstractCredentialsWizardPage`: base class for
  credential-entry pages; default `nextId()` routes to Terms-of-Service (if required) or
  Advanced Setup, and exposes `getCredentials()`.
- **owncloudhttpcredspage.h/.cpp/.ui** — `OwncloudHttpCredsPage`: username/password form for
  Basic auth servers.
- **flow2authcredspage.h/.cpp** + **flow2authwidget.h/.cpp/.ui** — `Flow2AuthCredsPage` (wizard
  page) hosts `Flow2AuthWidget`, which drives OAuth2/LoginFlowV2 (`Flow2Auth`): shows a browser
  link + polls for completion.
- **webview.h/.cpp/.ui** + **webviewpage.h/.cpp** — `WebView` (embedded `QWebEngineView`,
  WITH_WEBENGINE only) and `WebViewPage` wrapping it, used for browser-based login/registration
  flows that can't use Flow2.
- **termsofservicewizardpage.h/.cpp** + **termsofservicecheckwidget.h/.cpp/.ui** —
  `TermsOfServiceWizardPage` hosts `TermsOfServiceCheckWidget`, which polls whether the user has
  accepted ToS in the browser before letting the wizard proceed.
- **dataprotectionpage.h/.cpp/.ui** — `DataProtectionPage`: whitelabel consent/tracking notice
  (IONOS/STRATO), branches to `Page_AdvancedSetup` or `Page_DataProtectionSettings`.
- **dataprotectionsettingspage.h/.cpp/.ui** — `DataProtectionSettingsPage`: detailed
  cookie/tracking preference toggles shown only if the user opens "settings" from the consent page.
- **owncloudadvancedsetuppage.h/.cpp/.ui** — `OwncloudAdvancedSetupPage`: final page; local
  folder picker, sync-everything/selective-sync/virtual-files radio choice, quota lookup, avatar
  fetch; terminal page (`nextId()` returns -1).
- **wizardproxysettingsdialog.h/.cpp** + **proxysettings.ui** — `WizardProxySettingsDialog`:
  modal for manual proxy host/port/credentials, reachable from the server-setup page.
- **slideshow.h/.cpp** — `SlideShow`: generic animated image/label carousel widget used on the
  welcome page.
- **linklabel.h/.cpp** — `LinkLabel`: clickable/underline-on-hover `QLabel` used for hyperlink-
  style text throughout the wizard.

## How it fits together

`OwncloudWizard` registers all pages against `WizardCommon::Pages` IDs and lets each page's
`nextId()` decide the actual route. Typical flow: `Page_Welcome` → `Page_ServerSetup` (URL entry,
emits the `determineAuthType` signal, handled outside this folder in
`owncloudsetupwizard.cpp::slotDetermineAuthType` which runs the actual `DetermineAuthTypeJob`) →
depending on the detected auth type: `Page_HttpCreds` (Basic), `Page_Flow2AuthCreds`
(OAuth2/LoginFlowV2), or `Page_WebView` (browser-based, non-Flow2 builds). From a credentials
page, `AbstractCredentialsWizardPage::nextId()` (used by `Page_HttpCreds`) sends the user to
`Page_TermsOfService` if the server requires ToS acceptance, else to `Page_AdvancedSetup` — or,
if `useVirtualFileSyncByDefault()`, straight to the wizard end (`-1`), skipping Advanced Setup
entirely. The `Page_DataProtection` detour (with an optional further detour to
`Page_DataProtectionSettings`) exists only in the **Flow2-specific override**
(`Flow2AuthCredsPage::nextId()`, `#ifdef IONOS_BUILD`) — Basic-auth users never see it.
`Page_AdvancedSetup` is the terminal page that creates local/remote folders and finishes the
wizard (`basicSetupFinished`).

## Fork-specific vs upstream
- `dataprotectionpage.*`, `dataprotectionsettingspage.*`, and IONOS/STRATO logos
  and copy inside `welcomepage.cpp`, `flow2authwidget.cpp` (guarded by `IONOS_WL_BUILD`/
  `STRATO_WL_BUILD`/`IONOS_BUILD`) are whitelabel-only additions layered on top of upstream
  Nextcloud; `whitelabeltheme.h` (`WLTheme`) supplies fork-specific styling.
- Everything else (welcome/setup/creds/webview/advanced-setup/proxy pages, slideshow, linklabel)
  is generic upstream Nextcloud wizard code shared by all brand variants.

*Quelle: src/gui/wizard — Stand 2026-08-20, automatisch erstellt, bitte gegenlesen.*
