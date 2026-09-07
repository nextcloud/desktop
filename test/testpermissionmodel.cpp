/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

#include <memory>

#include "gui/sharing/permissionmodel.h"
#include "gui/sharing/unifiedshare.h"
#include "syncenginetestutils.h"

using namespace OCC;
using namespace OCC::Gui::Sharing;
using namespace Qt::StringLiterals;

namespace
{
std::unique_ptr<Share> shareFromJson(const QJsonObject &data, const AccountPtr &account)
{
    return std::unique_ptr<Share>(Share::fromJson(QJsonDocument{QJsonObject{{"ocs"_L1, QJsonObject{{"data"_L1, data}}}}}, account));
}
}

class TestPermissionModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesSharePermissionsAndRoles()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto share = shareFromJson(
            QJsonObject{
                {"permissions"_L1,
                 QJsonArray{
                     QJsonObject{{"class"_L1, "view"_L1}, {"display_name"_L1, "View files"_L1}, {"hint"_L1, "Read-only access"_L1}, {"enabled"_L1, true}},
                     QJsonObject{{"class"_L1, "download"_L1}, {"display_name"_L1, "Download files"_L1}, {"enabled"_L1, false}},
                 }},
            },
            fakeFolder.account());

        PermissionModel model;
        QAbstractItemModelTester modelTester{&model};
        model.setShare(share.get());

        QCOMPARE(model.roleNames().value(PermissionModel::LabelRole), "label"_ba);
        QCOMPARE(model.roleNames().value(PermissionModel::ClassNameRole), "className"_ba);
        QCOMPARE(model.roleNames().value(PermissionModel::PlaceholderRole), "hint"_ba);
        QCOMPARE(model.roleNames().value(PermissionModel::EnabledRole), "enabled"_ba);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), PermissionModel::LabelRole).toString(), "View files"_L1);
        QCOMPARE(model.data(model.index(0), PermissionModel::ClassNameRole).toString(), "view"_L1);
        QCOMPARE(model.data(model.index(0), PermissionModel::PlaceholderRole).toString(), "Read-only access"_L1);
        QVERIFY(model.data(model.index(0), PermissionModel::EnabledRole).toBool());
        QVERIFY(!model.data(model.index(1), PermissionModel::EnabledRole).toBool());
        QCOMPARE(model.rowCount(model.index(0)), 0);
    }

    void recipientOverridesSharePermissionAndFallsBackForMissingData()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto share = shareFromJson(
            QJsonObject{
                {"permissions"_L1,
                 QJsonArray{
                     QJsonObject{{"class"_L1, "view"_L1}, {"display_name"_L1, "View files"_L1}, {"enabled"_L1, true}},
                     QJsonObject{{"class"_L1, "download"_L1}, {"display_name"_L1, "Download files"_L1}, {"enabled"_L1, true}},
                 }},
                {"recipients"_L1,
                 QJsonArray{QJsonObject{
                     {"class"_L1, "user"_L1},
                     {"display_name"_L1, "Alice"_L1},
                     {"value"_L1, "alice"_L1},
                 }}},
            },
            fakeFolder.account());
        QVERIFY(share);

        const auto recipient = share->recipients().constFirst();
        PermissionModel model;
        QAbstractItemModelTester modelTester{&model};
        model.setShare(share.get());
        model.setRecipient(recipient);

        QCOMPARE(model.rowCount(), 2);
        QVERIFY(model.data(model.index(1), PermissionModel::EnabledRole).toBool());

        recipient->setPermissionOverride("download"_L1, false);
        QVERIFY(!model.data(model.index(1), PermissionModel::EnabledRole).toBool());

        recipient->updateFromJson(QJsonObject{
            {"class"_L1, "user"_L1},
            {"display_name"_L1, "Alice"_L1},
            {"value"_L1, "alice"_L1},
            {"permissions"_L1, QJsonArray{}},
        });
        QVERIFY(model.data(model.index(1), PermissionModel::EnabledRole).toBool());
    }

    void updatesWhenAssignedSharePermissionsChange()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto share = shareFromJson(
            QJsonObject{
                {"permissions"_L1, QJsonArray{QJsonObject{{"class"_L1, "view"_L1}, {"display_name"_L1, "View files"_L1}, {"enabled"_L1, true}}}},
            },
            fakeFolder.account());

        PermissionModel model;
        model.setShare(share.get());
        QAbstractItemModelTester modelTester{&model};
        QSignalSpy resetSpy{&model, &QAbstractItemModel::modelReset};

        const auto updatedShare = QJsonDocument{QJsonObject{
            {"ocs"_L1,
             QJsonObject{{"data"_L1,
                          QJsonObject{{"permissions"_L1,
                                       QJsonArray{QJsonObject{{"class"_L1, "view"_L1}, {"display_name"_L1, "View files"_L1}, {"enabled"_L1, false}}}}}}}}}};
        share->updateFromJson(updatedShare);

        QCOMPARE(resetSpy.count(), 1);
        QVERIFY(!model.data(model.index(0), PermissionModel::EnabledRole).toBool());
    }
};

QTEST_MAIN(TestPermissionModel)
#include "testpermissionmodel.moc"
