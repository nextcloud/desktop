//
//  EnumerationListener.swift
//  
//
//  Created by Claudio Cambra on 16/7/24.
//

import FileProvider
import Foundation

public protocol EnumerationListener: NSObject {
    var domain: NSFileProviderDomain { get }
    func started(actionId: UUID)
    func finished(actionId: UUID)
    func failed(actionId: UUID, error: Error)
}
