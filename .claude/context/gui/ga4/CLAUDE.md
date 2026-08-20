# src/gui/ga4

Google Analytics 4 (Measurement Protocol) telemetry integration for the fork, used to send anonymous usage/click events from the desktop client. Files carry "This file is part of Nextcloud Destop - Ionos HiDrive Next" headers (sic, typo in the original) and explicit "Modifications" notes, meaning this is an adapted version of an (upstream/older ownCloud-style) GA integration, reworked to target the newer GA4 `/g/collect` endpoint instead of the old Universal Analytics Measurement Protocol.

## Key classes/components

- `GAnalytics` (`ganalytics.h/.cpp`) — thread-safe **singleton** (`GAnalytics::getInstance()`) public API: configure measurement ID, client ID, account, send interval, enable/disable, anonymize IPs, log level; `sendEvent()`/`sendEventImmediatley()` queue events. Delegates all real work to a private `GAnalyticsWorker`.
- `GAnalyticsWorker` (`ganalytics_worker.h/.cpp`) — the actual implementation ("d-pointer" of `GAnalytics`, declared `friend`): owns a `QQueue<QueryBuffer>` of pending events, a `QTimer` that periodically POSTs them via `OCC::Account::sendRawRequest`, builds the GA4 query string (measurement id, client/session ids, user agent, screen resolution, language, event name, engagement time…), and drops events older than 4 hours.
- `DataCollectionWrapper` (`datacollectionwrapper.h/.cpp`) — the app-facing wrapper actually called from GUI code (settings, login/logout); exposes enums `TrackingPage`/`TrackingEvent`/`TrackingElement` (e.g. `GeneralSettings`, `PrivacyPolicy`, `AutoStart`) and slots `login()`, `accountRemoved()`, `clicked()`, `opened()`; maps them to string event/page names and forwards to `GAnalytics::getInstance()`. Also hardcodes the per-platform/per-buildtype GA4 measurement IDs (`GA_MEASUREMENT_ID`) in `initDataCollection()`.

## How it fits together

GUI code (settings dialogs, `application.cpp`) talks only to `DataCollectionWrapper`, which forwards typed tracking calls to the `GAnalytics` singleton, which hands them to its `GAnalyticsWorker` for queuing/batching/HTTP delivery to `www.google-analytics.com`. `setSendData(bool)` toggles the whole pipeline via the user's telemetry opt-in setting.

## Fork-specific notes

- The whole folder is a fork addition on top of Nextcloud (no such telemetry exists upstream); explicit code comments mark it as ported/modified for GA4 (a `Modifications:` block comment listing `* - Changed the usage of Measurement Protocol to the GA4 API`), and measurement IDs / `TODO SES-169` comments confirm it's BRICKMAKERS/IONOS-owned code.

*Quelle: src/gui/ga4 — Stand 2026-08-17, automatisch erstellt, bitte gegenlesen.*
