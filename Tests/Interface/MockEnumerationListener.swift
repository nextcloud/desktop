//
//  MockEnumerationListener.swift
//
//
//  Created by Claudio Cambra on 17/7/24.
//

import Foundation
import NextcloudFileProviderKit

public class MockEnumerationListener: NSObject, EnumerationListener {
    public var startActions = [UUID: Date]()
    public var finishActions = [UUID: Date]()
    public var errorActions = [UUID: Date]()

    public func enumerationActionStarted(actionId: UUID) {
        print("Enumeration action started with id: \(actionId)")
        startActions[actionId] = Date()
    }

    public func enumerationActionFinished(actionId: UUID) {
        print("Enumeration action finished with id: \(actionId)")
        finishActions[actionId] = Date()
    }

    public func enumerationActionFailed(actionId: UUID, error: Error) {
        print("Enumeration action failed with id: \(actionId) and error: \(error)")
        errorActions[actionId] = Date()
    }
}
