//
//  ShareTableViewDataSource.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 27/2/24.
//

import AppKit
import FileProvider
import NextcloudKit
class ShareTableViewDataSource: NSObject, NSTableViewDataSource {
    var sharesTableView: NSTableView? {
        didSet {
            sharesTableView?.dataSource = self
            sharesTableView?.reloadData()
        }
    }
    private var itemIdentifier: NSFileProviderItemIdentifier?
    private var itemURL: URL?
    private var shares: [NKShare] = [] {
        didSet { sharesTableView?.reloadData() }
    }
}
