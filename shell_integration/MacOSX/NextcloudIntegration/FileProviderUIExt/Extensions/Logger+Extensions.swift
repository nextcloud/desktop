//
//  Logger+Extensions.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import OSLog

extension Logger {
    private static var subsystem = Bundle.main.bundleIdentifier!

    static let actionViewController = Logger(subsystem: subsystem, category: "actionViewController")
    static let authenticationViewController = Logger(subsystem: subsystem, category: "authenticationViewController")
    static let eviction = Logger(subsystem: subsystem, category: "eviction")
    static let lockViewController = Logger(subsystem: subsystem, category: "lockViewController")
    static let metadataProvider = Logger(subsystem: subsystem, category: "metadataProvider")
    static let shareCapabilities = Logger(subsystem: subsystem, category: "shareCapabilities")
    static let shareController = Logger(subsystem: subsystem, category: "shareController")
    static let shareeDataSource = Logger(subsystem: subsystem, category: "shareeDataSource")
    static let sharesDataSource = Logger(subsystem: subsystem, category: "sharesDataSource")
    static let shareOptionsView = Logger(subsystem: subsystem, category: "shareOptionsView")
    static let shareViewController = Logger(subsystem: subsystem, category: "shareViewController")
}

