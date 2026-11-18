//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

///
/// Generation of the on-disk format of the metadata database.
///
/// Bump it with every change of the schema in ``DatabaseSchema``. It is recorded in the file as `PRAGMA user_version` and in the domain's defaults, so a build which meets a newer store than it understands can set the store up from scratch instead of misreading it.
///
/// No build distributed to users, release candidates and betas included, has shipped this store yet. Until one does, a change of the schema edits `v1_initial` instead of registering a migration and raises ``oldestMigratable`` together with ``current``, so a store written before the change is set up from scratch instead of being kept without it. The change which prepares that first build freezes ``oldestMigratable`` at the version it ships; from then on every change of the schema gets a migration of its own and raises ``current`` alone.
///
enum StoreVersion {
    static let current = 3

    ///
    /// The oldest store version the migrator brings up to date. An older store is set up from scratch.
    ///
    /// Raising it after a build was distributed sets up the store of everyone using that build from scratch, which drops what only the store records: pins, exclusions from sync, lock tokens and the bookkeeping of chunked uploads.
    ///
    static let oldestMigratable = 3
}
