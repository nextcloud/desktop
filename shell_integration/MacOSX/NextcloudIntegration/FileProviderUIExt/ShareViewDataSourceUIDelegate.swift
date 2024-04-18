//
//  ShareViewDataSourceUIDelegate.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 28/2/24.
//

import Foundation
import NextcloudKit

protocol ShareViewDataSourceUIDelegate {
    func fetchStarted()
    func fetchFinished()
    func hideOptions(_ sender: Any)
    func showOptions(share: NKShare)
    func showError(_ errorString: String)
}
