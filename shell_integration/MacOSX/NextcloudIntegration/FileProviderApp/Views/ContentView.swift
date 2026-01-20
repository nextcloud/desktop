//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import SwiftUI

struct ContentView: View {
    @Binding var accounts: [Account]

    @State private var selection = Set<Account.ID>()
    @State private var isAddingAccount = false

    init(accounts: Binding<[Account]>) {
        self._accounts = accounts
    }

    var body: some View {
        Group {
            if accounts.isEmpty {
                ContentUnavailableView("No Accounts", image: "person.2.slash", description: Text("Add an account to get started."))
            } else {
                Table(accounts, selection: $selection) {
                    TableColumn("Address") { account in
                        Text(account.address.absoluteString)
                    }
                    
                    TableColumn("User", value: \.user)
                        .width(50)
                    
                    TableColumn("Domain") { account in
                        Text(account.domain?.identifier.rawValue ?? "")
                    }
                }
                .contextMenu(forSelectionType: Account.ID.self) { items in
                    if items.isEmpty == false {
                        Button("Remove") {
                            removeAccount(ids: items)
                        }
                    }
                }
            }
        }
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button {
                    isAddingAccount = true
                } label: {
                    Label("Add Account", systemImage: "person.badge.plus")
                }
            }
        }
        .sheet(isPresented: $isAddingAccount) {
            AddAccountView(accounts: $accounts)
        }
    }

    func removeAccount(ids: Set<Account.ID>) {
        accounts.removeAll(where: { account in
            ids.contains(account.id)
        })
    }
}

#Preview("No Accounts") {
    ContentView(accounts: .constant([]))
}

#Preview("One Account") {
    ContentView(accounts: .constant([
        Account(address: URL(string: "http://localhost:8080")!, domain: .mock, password: "admin", user: "admin"),
    ]))
}

#Preview("Multiple Accounts") {
    ContentView(accounts: .constant([
        Account(address: URL(string: "http://localhost:8080")!, domain: .mock, password: "admin", user: "admin"),
        Account(address: URL(string: "http://localhost:8081")!, domain: .mock, password: "admin", user: "admin"),
        Account(address: URL(string: "http://localhost:8082")!, domain: .mock, password: "admin", user: "admin"),
    ]))
}
