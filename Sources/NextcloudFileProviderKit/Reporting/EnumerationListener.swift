//
//  EnumerationListener.swift
//  
//
//  Created by Claudio Cambra on 16/7/24.
//

import FileProvider
import Foundation

public protocol EnumerationListener: NSObject {
    func enumerationActionStarted(actionId: UUID)
    func enumerationActionFinished(actionId: UUID)
    func enumerationActionFailed(actionId: UUID, error: Error)
}
