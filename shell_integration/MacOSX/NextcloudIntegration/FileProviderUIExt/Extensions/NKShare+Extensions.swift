//
//  NKShare+Extensions.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 28/2/24.
//

import AppKit
import NextcloudKit

extension NKShare {
    enum ShareType: Int {
        case user = 0
        case group = 1
        case publicLink = 3
        case email = 4
        case federatedCloud = 6
        case circle = 7
        case talkConversation = 10
    }

    enum PermissionValues: Int {
        case readShare = 1
        case updateShare = 2
        case createShare = 4
        case deleteShare = 8
        case shareShare = 16
        case all = 31
    }

    var typeImage: NSImage? {
        var image: NSImage?
        switch shareType {
        case ShareType.user.rawValue:
            image = NSImage(
                systemSymbolName: "person.circle.fill", 
                accessibilityDescription: "User share icon"
            )
        case ShareType.group.rawValue:
            image = NSImage(
                systemSymbolName: "person.2.circle.fill",
                accessibilityDescription: "Group share icon"
            )
        case ShareType.publicLink.rawValue:
            image = NSImage(
                systemSymbolName: "link.circle.fill",
                accessibilityDescription: "Public link share icon"
            )
        case ShareType.email.rawValue:
            image = NSImage(
                systemSymbolName: "envelope.circle.fill",
                accessibilityDescription: "Email share icon"
            )
        case ShareType.federatedCloud.rawValue:
            image = NSImage(
                systemSymbolName: "cloud.circle.fill",
                accessibilityDescription: "Federated cloud share icon"
            )
        case ShareType.circle.rawValue:
            image = NSImage(
                systemSymbolName: "circle.circle.fill",
                accessibilityDescription: "Circle share icon"
            )
        case ShareType.talkConversation.rawValue:
            image = NSImage(
                systemSymbolName: "message.circle.fill",
                accessibilityDescription: "Talk conversation share icon"
            )
        default:
            return nil
        }

        var config = NSImage.SymbolConfiguration(textStyle: .body, scale: .large)
        if #available(macOS 12.0, *) {
            config = config.applying(
                .init(paletteColors: [.controlBackgroundColor, .controlAccentColor])
            )
        }
        return image?.withSymbolConfiguration(config)
    }

    var displayString: String {
        if label != "" {
            return label
        }

        switch shareType {
        case ShareType.user.rawValue:
            return "User share"
        case ShareType.group.rawValue:
            return "Group share"
        case ShareType.publicLink.rawValue:
            return "Public link share"
        case ShareType.email.rawValue:
            return "Email share"
        case ShareType.federatedCloud.rawValue:
            return "Federated cloud share"
        case ShareType.circle.rawValue:
            return "Circle share"
        case ShareType.talkConversation.rawValue:
            return "Talk conversation share"
        default:
            return "Unknown share"
        }
    }

    var expirationDateString: String? {
        guard let date = expirationDate else { return nil }
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "YYYY-MM-dd HH:mm:ss"
        return dateFormatter.string(from: date as Date)
    }

    static func formattedDateString(date: Date) -> String {
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "YYYY-MM-dd HH:mm:ss"
        return dateFormatter.string(from: date)
    }
}
