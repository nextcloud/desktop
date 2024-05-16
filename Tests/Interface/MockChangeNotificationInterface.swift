//
//  MockChangeNotificationInterface.swift
//
//
//  Created by Claudio Cambra on 16/5/24.
//

import Foundation
import NextcloudFileProviderKit

public class MockChangeNotificationInterface: ChangeNotificationInterface {
    public var changeHandler: (() -> Void)?
    public init(changeHandler: (() -> Void)? = nil) {
        self.changeHandler = changeHandler
    }
    public func notifyChange() {
        changeHandler?()
    }
}
