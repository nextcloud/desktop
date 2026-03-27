//
//  NKShare+Extensions.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import AppKit
import NextcloudKit

extension NKShare {
    var typeImage: NSImage? {
        var image: NSImage?
        switch shareType {
        case ShareType.internalLink.rawValue:
            image = NSImage(
                systemSymbolName: "square.and.arrow.up.circle.fill",
                accessibilityDescription: String(localized: "Internal link share icon")
            )
        case ShareType.user.rawValue:
            image = NSImage(
                systemSymbolName: "person.circle.fill", 
                accessibilityDescription: String(localized: "User share icon")
            )
        case ShareType.group.rawValue:
            image = NSImage(
                systemSymbolName: "person.2.circle.fill",
                accessibilityDescription: String(localized: "Group share icon")
            )
        case ShareType.publicLink.rawValue:
            image = NSImage(
                systemSymbolName: "link.circle.fill",
                accessibilityDescription: String(localized: "Public link share icon")
            )
        case ShareType.email.rawValue:
            image = NSImage(
                systemSymbolName: "envelope.circle.fill",
                accessibilityDescription: String(localized: "Email share icon")
            )
        case ShareType.federatedCloud.rawValue:
            image = NSImage(
                systemSymbolName: "cloud.circle.fill",
                accessibilityDescription: String(localized: "Federated cloud share icon")
            )
        case ShareType.team.rawValue:
            image = NSImage(
                systemSymbolName: "circle.circle.fill",
                accessibilityDescription: String(localized: "Team share icon")
            )
        case ShareType.talkConversation.rawValue:
            image = NSImage(
                systemSymbolName: "message.circle.fill",
                accessibilityDescription: String(localized: "Talk conversation share icon")
            )
        default:
            return nil
        }

        var config = NSImage.SymbolConfiguration(textStyle: .body, scale: .large)
        config = config.applying(.init(paletteColors: [.controlBackgroundColor, .controlAccentColor]))

        return image?.withSymbolConfiguration(config)
    }

    var displayString: String {
        if label != "" {
            return label
        }

        switch shareType {
        case ShareType.internalLink.rawValue:
            return String(localized: "Internal share (requires access to file)")
        case ShareType.user.rawValue:
            return String(format: String(localized: "User share (%@)"), shareWith)
        case ShareType.group.rawValue:
            return String(format: String(localized: "Group share (%@)"), shareWith)
        case ShareType.publicLink.rawValue:
            return String(localized: "Public link share")
        case ShareType.email.rawValue:
            return String(format: String(localized: "Email share (%@)"), shareWith)
        case ShareType.federatedCloud.rawValue:
            return String(format: String(localized: "Federated cloud share (%@)"), shareWith)
        case ShareType.team.rawValue:
            return String(format: String(localized: "Team share (%@)"), shareWith)
        case ShareType.talkConversation.rawValue:
            return String(format: String(localized: "Talk conversation share (%@)"), shareWith)
        default:
            return String(localized: "Unknown share")
        }
    }

    var expirationDateString: String? {
        guard let date = expirationDate else { return nil }
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "YYYY-MM-dd HH:mm:ss"
        return dateFormatter.string(from: date as Date)
    }

    var shareesCanEdit: Bool {
        get {
            NKShare.Permission(rawValue: permissions).contains(.update)
        }

        set {
            if newValue {
                permissions = NKShare.Permission(rawValue: permissions).union(.update).rawValue
            } else {
                permissions = NKShare.Permission(rawValue: permissions).subtracting(.update).rawValue
            }
        }
    }

    static func formattedDateString(date: Date) -> String {
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "YYYY-MM-dd HH:mm:ss"
        return dateFormatter.string(from: date)
    }
}
