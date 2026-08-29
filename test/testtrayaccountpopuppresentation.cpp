/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "macOS/trayaccountpopup/trayaccountpopuppresentation.h"

#include <QTest>

#include <vector>

using namespace OCC::Mac;

class TestTrayAccountPopupPresentation : public QObject
{
    Q_OBJECT

private:
    enum class Operation {
        OrderFront,
        ActivateApplication,
        MakeKey,
    };

private Q_SLOTS:
    void ordersPopupBeforeRequestingApplicationActivation()
    {
        auto operations = std::vector<Operation>();

        TrayAccountPopupPresentation::present(
            [&operations] {
                operations.push_back(Operation::OrderFront);
            },
            [&operations] {
                operations.push_back(Operation::ActivateApplication);
            },
            [&operations] {
                operations.push_back(Operation::MakeKey);
            });

        const auto expectedOperations = std::vector {
            Operation::OrderFront,
            Operation::ActivateApplication,
            Operation::MakeKey,
        };
        QVERIFY(operations == expectedOperations);
    }
};

QTEST_APPLESS_MAIN(TestTrayAccountPopupPresentation)
#include "testtrayaccountpopuppresentation.moc"
