//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import os
import SwiftUI

struct AddAccountView: View {
    @Environment(\.dismiss) var dismiss

    @Binding var accounts: [Account]

    @State var address: String = ""
    @State var password: String = ""
    @State var user: String = ""

    let logger = Logger(category: "AddAccountView")

    init(accounts: Binding<[Account]>) {
        self._accounts = accounts
    }

    var body: some View {
        Form {
            TextField("Address", text: $address)
            TextField("User", text: $user)
            SecureField("Password", text: $password)

            Button {
                submit()
            } label: {
                Text("Add")
            }
        }
        .padding()
    }

    func submit() {
        Task {
            do {
                guard let address = URL(string: self.address) else {
                    logger.error("Failed to create URL from address string: \(self.address)")
                    return
                }

                let account = Account(address: address, password: password, user: user)
                let manager = FileProviderDomainManager()
                let domain = try await manager.add(for: account)
                account.domain = domain
                accounts.append(account)
                dismiss()
            } catch {
                logger.error("Failed to add account: \(error)")
            }
        }
    }
}

#Preview {
    AddAccountView(accounts: .constant([]))
}
