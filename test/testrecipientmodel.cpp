/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

#include <memory>

#include "gui/sharing/recipientmodel.h"
#include "gui/sharing/unifiedshare.h"
#include "syncenginetestutils.h"

using namespace OCC;
using namespace OCC::Gui::Sharing;
using namespace Qt::StringLiterals;

namespace
{
std::unique_ptr<Share> shareFromJson(const QJsonObject &data, const AccountPtr &account)
{
    return Share::fromJson(QJsonDocument{QJsonObject{{"ocs"_L1, QJsonObject{{"data"_L1, data}}}}}, account);
}
}

class TestRecipientModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesRecipientDataAndRoles()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto share = shareFromJson(
            QJsonObject{
                {"recipients"_L1,
                 QJsonArray{
                     QJsonObject{
                         {"class"_L1, "federated-user"_L1},
                         {"display_name"_L1, "Alice"_L1},
                         {"value"_L1, "alice"_L1},
                         {"instance"_L1, "cloud.example.com"_L1},
                         {"icon"_L1, QJsonObject{{"svg"_L1, "<svg/>"_L1}, {"light"_L1, "light.svg"_L1}, {"dark"_L1, "dark.svg"_L1}}},
                         {"secret"_L1, QJsonObject{{"updatable"_L1, true}, {"value"_L1, "secret"_L1}, {"url"_L1, "https://example.com/s/secret"_L1}}},
                         {"initiator"_L1, QJsonObject{{"display_name"_L1, "Bob"_L1}}},
                     },
                     QJsonObject{
                         {"class"_L1, "user"_L1},
                         {"display_name"_L1, "Carol"_L1},
                         {"value"_L1, "carol"_L1},
                     },
                 }},
            },
            fakeFolder.account());

        RecipientModel model;
        QAbstractItemModelTester modelTester{&model};
        model.setShare(share.get());

        QCOMPARE(model.roleNames().value(RecipientModel::LabelRole), "label"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::ClassNameRole), "className"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::ValueRole), "value"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::InstanceRole), "instance"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::IconSvgUrlRole), "iconSvgUrl"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::IconLightRole), "iconLight"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::IconDarkRole), "iconDark"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::SecretUpdatableRole), "secretUpdatable"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::SecretValueRole), "secretValue"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::SecretUrlRole), "secretUrl"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::InitiatorDisplayNameRole), "initiatorDisplayName"_ba);
        QCOMPARE(model.roleNames().value(RecipientModel::RecipientRole), "recipient"_ba);

        QCOMPARE(model.rowCount(), 2);
        const auto firstIndex = model.index(0);
        QCOMPARE(model.data(firstIndex, RecipientModel::LabelRole).toString(), "Alice"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::ClassNameRole).toString(), "federated-user"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::ValueRole).toString(), "alice"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::InstanceRole).toString(), "cloud.example.com"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::IconSvgUrlRole).toString(), "data:image/svg+xml;base64,PHN2Zy8+"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::IconLightRole).toString(), "light.svg"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::IconDarkRole).toString(), "dark.svg"_L1);
        QVERIFY(model.data(firstIndex, RecipientModel::SecretUpdatableRole).toBool());
        QCOMPARE(model.data(firstIndex, RecipientModel::SecretValueRole).toString(), "secret"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::SecretUrlRole).toString(), "https://example.com/s/secret"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::InitiatorDisplayNameRole).toString(), "Bob"_L1);
        QCOMPARE(model.data(firstIndex, RecipientModel::RecipientRole).value<Recipient *>(), share->recipients().constFirst());
        QVERIFY(model.data(model.index(1), RecipientModel::InstanceRole).toString().isEmpty());
        QCOMPARE(model.rowCount(model.index(0)), 0);
    }

    void resetsForRecipientUpdatesAndIgnoresOldShares()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto firstShare = shareFromJson(
            QJsonObject{{"recipients"_L1, QJsonArray{QJsonObject{{"class"_L1, "user"_L1}, {"display_name"_L1, "Alice"_L1}, {"value"_L1, "alice"_L1}}}}},
            fakeFolder.account());
        const auto secondShare = shareFromJson(
            QJsonObject{{"recipients"_L1, QJsonArray{QJsonObject{{"class"_L1, "user"_L1}, {"display_name"_L1, "Bob"_L1}, {"value"_L1, "bob"_L1}}}}},
            fakeFolder.account());

        RecipientModel model;
        model.setShare(firstShare.get());
        model.setShare(secondShare.get());
        QAbstractItemModelTester modelTester{&model};
        QSignalSpy resetSpy{&model, &QAbstractItemModel::modelReset};

        const auto emptyRecipientsUpdate = [](const auto &share) {
            share->updateFromJson(QJsonDocument{QJsonObject{
                {"ocs"_L1, QJsonObject{{"data"_L1, QJsonObject{{"recipients"_L1, QJsonArray{}}}}}},
            }});
        };

        emptyRecipientsUpdate(firstShare);
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(model.rowCount(), 1);

        emptyRecipientsUpdate(secondShare);
        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_MAIN(TestRecipientModel)
#include "testrecipientmodel.moc"
