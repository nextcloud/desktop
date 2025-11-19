//
//  ShareViewDataSourceUIDelegate.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
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
