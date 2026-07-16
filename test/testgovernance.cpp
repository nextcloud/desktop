/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "syncenginetestutils.h"

#include "governance/applygovernancelabel.h"
#include "governance/deletegovernancelabel.h"
#include "governance/getavailablegovernancelabels.h"
#include "governance/getgovernancelabels.h"
#include "governance/governancelabelslistmodel.h"
#include "governance/governancetypes.h"

#include <QAbstractItemModelTester>
#include <QTest>

using namespace OCC;
using namespace Qt::StringLiterals;

class GovernanceTestHelper : public QObject
{
    Q_OBJECT

public:
    GovernanceTestHelper(QObject *parent = nullptr)
        : QObject{parent}
    {
    }

Q_SIGNALS:
    void setupSucceeded();

public slots:
    void setup(FakeFolder &fakeFolder)
    {
        fakeFolder.setServerOverride([this] (FakeQNAM::Operation operation, const QNetworkRequest &request, [[maybe_unused]] QIODevice *device) -> QNetworkReply*
        {
            const auto requestPathString = request.url().path();
            const auto requestPath = QStringView{requestPathString};
            const auto routeIndex = requestPath.indexOf(u"/ocs/v2.php/apps/governance/v1/labels/"_s);
            if (routeIndex == -1) {
                return nullptr;
            }

            const auto governanceRequestParameters = requestPath.mid(routeIndex + u"/ocs/v2.php/apps/governance/v1/labels/"_s.size()).split(u"/"_s);
            qDebug() << requestPath << routeIndex << governanceRequestParameters << operation;

            switch (operation)
            {
            case QNetworkAccessManager::CustomOperation:
                if (request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == u"DELETE"_s) {
                    return new FakePayloadReply{operation, request, fakeDeleteGovernanceLabelReply(governanceRequestParameters).toUtf8(), this};
                } else if (request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == u"GET"_s) {
                    if (governanceRequestParameters.count() == 4 && governanceRequestParameters.at(3) == u"available"_s) {
                        return new FakePayloadReply{operation, request, fakeGetAvailableGovernanceLabelsReply(governanceRequestParameters).toUtf8(), this};
                    } else if (governanceRequestParameters.count() == 2) {
                        return new FakePayloadReply{operation, request, fakeGetGovernanceLabelsReply(governanceRequestParameters).toUtf8(), this};
                    }
                } else if (request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == u"POST"_s) {
                    return new FakePayloadReply{operation, request, fakeApplyGovernanceLabelReply(governanceRequestParameters).toUtf8(), this};
                }
                break;
            case QNetworkAccessManager::HeadOperation:
                break;
            case QNetworkAccessManager::GetOperation:
                if (governanceRequestParameters.count() == 4 && governanceRequestParameters.at(3) == u"available"_s) {
                    return new FakePayloadReply{operation, request, fakeGetAvailableGovernanceLabelsReply(governanceRequestParameters).toUtf8(), this};
                } else if (governanceRequestParameters.count() == 2) {
                    return new FakePayloadReply{operation, request, fakeGetGovernanceLabelsReply(governanceRequestParameters).toUtf8(), this};
                }
                break;
            case QNetworkAccessManager::PutOperation:
                break;
            case QNetworkAccessManager::PostOperation:
                return new FakePayloadReply{operation, request, fakeApplyGovernanceLabelReply(governanceRequestParameters).toUtf8(), this};
                break;
            case QNetworkAccessManager::DeleteOperation:
                return new FakePayloadReply{operation, request, fakeDeleteGovernanceLabelReply(governanceRequestParameters).toUtf8(), this};
                break;
            case QNetworkAccessManager::UnknownOperation:
                break;
            }

            return nullptr;
        });

        Q_EMIT setupSucceeded();
    }

private:
    [[nodiscard]] QString fakeGetGovernanceLabelsReply(const QList<QStringView> &parameters) const
    {
        qDebug() << parameters;
        if (parameters.at(0) == u"FILES"_s && parameters.at(1) == u"117"_s) {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": {
      "hold": [],
      "retention": [],
      "sensitivity": null
    }
  }
}
            )json"_s;

            return replyJson;
        } else if (parameters.at(0) == u"FILES"_s && parameters.at(1) == u"117117"_s) {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "failure",
      "statuscode": 404,
      "message": "Entity with id 117117 not found"
    },
    "data": []
  }
}
            )json"_s;

            return replyJson;
        } else {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "failure",
      "statuscode": 400,
      "message": "Invalid entity type FILEStdytf"
    },
    "data": []
  }
}
            )json"_s;

            return replyJson;
        }
    }

    [[nodiscard]] QString fakeGetAvailableGovernanceLabelsReply(const QList<QStringView> &parameters) const
    {
        if (parameters.at(0) == u"FILES"_s && parameters.at(1) == u"117"_s) {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": [
      {
        "id": "91785883351310337",
        "name": "Test Sensitivity",
        "priority": 0,
        "description": "",
        "color": "bf4040",
        "scopes": [
          "FILES"
        ]
      }
    ]
  }
}
            )json"_s;

            return replyJson;
        } else if (parameters.at(0) == u"FILES"_s && parameters.at(1) == u"117117"_s) {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": [
      {
        "id": "91785883351310337",
        "name": "Test Sensitivity",
        "priority": 0,
        "description": "",
        "color": "bf4040",
        "scopes": [
          "FILES"
        ]
      }
    ]
  }
}
            )json"_s;

            return replyJson;
        } else {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "failure",
      "statuscode": 400,
      "message": "Invalid entity type FILEStdytf"
    },
    "data": []
  }
}
            )json"_s;

            return replyJson;
        }
    }

    [[nodiscard]] QString fakeApplyGovernanceLabelReply(const QList<QStringView> &parameters) const
    {
        if (parameters.at(1) == u"117"_s) {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": {
      "hold": [],
      "retention": [],
      "sensitivity": null
    }
  }
}
            })json"_s;

            return replyJson;
        } else {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": {
      "hold": [],
      "retention": [],
      "sensitivity": null
    }
  }
}
            })json"_s;

            return replyJson;
        }
    }

    [[nodiscard]] QString fakeDeleteGovernanceLabelReply(const QList<QStringView> &parameters) const
    {
        if (parameters.at(1) == u"117"_s) {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": {
      "hold": [],
      "retention": [],
      "sensitivity": null
    }
  }
}
            })json"_s;

            return replyJson;
        } else {
            const auto replyJson = uR"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": {
      "hold": [],
      "retention": [],
      "sensitivity": null
    }
  }
}
            })json"_s;

            return replyJson;
        }
    }
};

class TestGovernance : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);
        OCC::Logger::instance()->setLogFile(QStringLiteral("-"));
        OCC::Logger::instance()->addLogRule({ QStringLiteral("sync.httplogger=true") });

        QStandardPaths::setTestModeEnabled(true);
    }

    void testApplyGovernanceLabel()
    {
        FakeFolder fakeFolder{{}};
        GovernanceTestHelper testHelper;
        testHelper.setup(fakeFolder);

        ApplyGovernanceLabel myJob;
        QSignalSpy finishedSpy(&myJob, &ApplyGovernanceLabel::finished);
        QSignalSpy finishedWithErrorSpy(&myJob, &ApplyGovernanceLabel::finishedWithError);

        fakeFolder.remoteModifier().insert("test.txt");

        QVERIFY(fakeFolder.syncOnce());

        myJob.setAccount(fakeFolder.account());
        myJob.setEntityId(u"1"_s);
        myJob.setEntityType(Governance::EntityType::Files);
        myJob.setLabelType(Governance::LabelType::Sensitivity);
        myJob.setLabelId(u"1"_s);

        myJob.start();

        finishedSpy.wait();
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedWithErrorSpy.count(), 0);
    }

    void testDeleteGovernanceLabel()
    {
        FakeFolder fakeFolder{{}};
        GovernanceTestHelper testHelper;
        testHelper.setup(fakeFolder);

        DeleteGovernanceLabel myJob;
        QSignalSpy finishedSpy(&myJob, &DeleteGovernanceLabel::finished);
        QSignalSpy finishedWithErrorSpy(&myJob, &DeleteGovernanceLabel::finishedWithError);

        fakeFolder.remoteModifier().insert("test.txt");

        QVERIFY(fakeFolder.syncOnce());

        myJob.setAccount(fakeFolder.account());
        myJob.setEntityId(u"1"_s);
        myJob.setEntityType(Governance::EntityType::Files);
        myJob.setLabelType(Governance::LabelType::Sensitivity);
        myJob.setLabelId(u"1"_s);

        myJob.start();

        finishedSpy.wait();
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedWithErrorSpy.count(), 0);
    }

    void testGetAvailableGovernanceLabels_ValidIdValidType()
    {
        FakeFolder fakeFolder{{}};
        GovernanceTestHelper testHelper;
        testHelper.setup(fakeFolder);

        GetAvailableGovernanceLabels myJob;
        QSignalSpy finishedSpy(&myJob, &GetAvailableGovernanceLabels::finished);
        QSignalSpy finishedWithErrorSpy(&myJob, &GetAvailableGovernanceLabels::finishedWithError);

        fakeFolder.remoteModifier().insert("test.txt");

        QVERIFY(fakeFolder.syncOnce());

        myJob.setAccount(fakeFolder.account());
        myJob.setEntityId(u"117"_s);
        myJob.setEntityType(Governance::EntityType::Files);
        myJob.setLabelType(Governance::LabelType::Sensitivity);

        myJob.start();

        finishedSpy.wait();
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedWithErrorSpy.count(), 0);
    }

    void testGetAvailableGovernanceLabels_InvalidIdValidType()
    {
        FakeFolder fakeFolder{{}};
        GovernanceTestHelper testHelper;
        testHelper.setup(fakeFolder);

        GetAvailableGovernanceLabels myJob;
        QSignalSpy finishedSpy(&myJob, &GetAvailableGovernanceLabels::finished);
        QSignalSpy finishedWithErrorSpy(&myJob, &GetAvailableGovernanceLabels::finishedWithError);

        fakeFolder.remoteModifier().insert("test.txt");

        QVERIFY(fakeFolder.syncOnce());

        myJob.setAccount(fakeFolder.account());
        myJob.setEntityId(u"117117"_s);
        myJob.setEntityType(Governance::EntityType::Files);
        myJob.setLabelType(Governance::LabelType::Sensitivity);

        myJob.start();

        finishedSpy.wait();
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedWithErrorSpy.count(), 0);
    }

    void testGetAvailableGovernanceLabels_ValidIdInvalidType()
    {
        FakeFolder fakeFolder{{}};
        GovernanceTestHelper testHelper;
        testHelper.setup(fakeFolder);

        GetAvailableGovernanceLabels myJob;
        QSignalSpy finishedSpy(&myJob, &GetAvailableGovernanceLabels::finished);
        QSignalSpy finishedWithErrorSpy(&myJob, &GetAvailableGovernanceLabels::finishedWithError);

        fakeFolder.remoteModifier().insert("test.txt");

        QVERIFY(fakeFolder.syncOnce());

        myJob.setAccount(fakeFolder.account());
        myJob.setEntityId(u"117"_s);
        myJob.setEntityType(Governance::EntityType::Custom);
        myJob.setCustomEntityType(u"FILEStdytf"_s);
        myJob.setLabelType(Governance::LabelType::Sensitivity);

        myJob.start();

        finishedSpy.wait();
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(finishedWithErrorSpy.count(), 1);
    }

    void testGetGovernanceLabels_ValidIdValidEntityType()
    {
        FakeFolder fakeFolder{{}};
        GovernanceTestHelper testHelper;
        testHelper.setup(fakeFolder);

        GetGovernanceLabels myJob;
        QSignalSpy finishedSpy(&myJob, &GetGovernanceLabels::finished);
        QSignalSpy finishedWithErrorSpy(&myJob, &GetGovernanceLabels::finishedWithError);

        fakeFolder.remoteModifier().insert("test.txt");

        QVERIFY(fakeFolder.syncOnce());

        myJob.setAccount(fakeFolder.account());
        myJob.setEntityId(u"117"_s);
        myJob.setEntityType(Governance::EntityType::Files);

        myJob.start();

        finishedSpy.wait();
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedWithErrorSpy.count(), 0);
    }

    void testGetGovernanceLabels_InvalidId()
    {
        FakeFolder fakeFolder{{}};
        GovernanceTestHelper testHelper;
        testHelper.setup(fakeFolder);

        GetGovernanceLabels myJob;
        QSignalSpy finishedSpy(&myJob, &GetGovernanceLabels::finished);
        QSignalSpy finishedWithErrorSpy(&myJob, &GetGovernanceLabels::finishedWithError);

        fakeFolder.remoteModifier().insert("test.txt");

        QVERIFY(fakeFolder.syncOnce());

        myJob.setAccount(fakeFolder.account());
        myJob.setEntityId(u"117117"_s);
        myJob.setEntityType(Governance::EntityType::Files);

        myJob.start();

        finishedWithErrorSpy.wait();
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(finishedWithErrorSpy.count(), 1);
    }

    void testGetGovernanceLabels_ValidIdInvalidEntityType()
    {
        FakeFolder fakeFolder{{}};
        GovernanceTestHelper testHelper;
        testHelper.setup(fakeFolder);

        GetGovernanceLabels myJob;
        QSignalSpy finishedSpy(&myJob, &GetGovernanceLabels::finished);
        QSignalSpy finishedWithErrorSpy(&myJob, &GetGovernanceLabels::finishedWithError);

        fakeFolder.remoteModifier().insert("test.txt");

        QVERIFY(fakeFolder.syncOnce());

        myJob.setAccount(fakeFolder.account());
        myJob.setEntityId(u"117"_s);
        myJob.setEntityType(Governance::EntityType::Custom);
        myJob.setCustomEntityType(u"FILEStdytf"_s);

        myJob.start();

        finishedWithErrorSpy.wait();
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(finishedWithErrorSpy.count(), 1);
    }

    void testGovernanceLabelListModel_setData()
    {
        GovernanceLabelsListModel myModel;
        QAbstractItemModelTester myModelTester(&myModel);

        const auto availableReplyJson = R"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": [
      {
        "id": "91785883351310337",
        "name": "Test Sensitivity",
        "priority": 0,
        "description": "This is a long description",
        "color": "bf4040",
        "canAssign": "no",
        "canRemove": "no",
        "isAssigned": false
      },
      {
        "id": "91345129959310149",
        "name": "High Sensitivity",
        "priority": 1,
        "description": "This is a long description for high sensitive label",
        "color": "56cd40",
        "canAssign": "no",
        "canRemove": "no",
        "isAssigned": false
      }
    ]
  }
}
            )json"_ba;
        const auto availableReplyData = QJsonDocument::fromJson(availableReplyJson);

        const auto existingReplyJson = R"json(
{
  "ocs": {
    "meta": {
      "status": "ok",
      "statuscode": 200,
      "message": "OK"
    },
    "data": {
      "retention": [],
      "sensitivity": [
        {
          "id": "91785883351310337",
          "name": "Test Sensitivity",
          "priority": 0,
          "description": "This is a long description",
          "color": "bf4040",
          "canAssign": "no",
          "canRemove": "no",
          "isAssigned": true
        }
      ]
    }
  }
}
)json"_ba;
        const auto existingReplyData = QJsonDocument::fromJson(existingReplyJson);

        myModel.setEntityId(u"123456"_s);
        myModel.setLabelBehavior(GovernanceLabelsListModel::LabelBehavior::UniqueLabel);
        myModel.setLabelType(Governance::LabelType::Sensitivity);

        myModel.setAvailableLabelsJsonData(availableReplyData);
        QCOMPARE(myModel.rowCount(), 1);

        myModel.setExistingLabelsJsonData(existingReplyData);

        QCOMPARE(myModel.rowCount(), 3);

        const auto firstLabelIndex = myModel.index(0);
        QVERIFY(firstLabelIndex.isValid());
        QCOMPARE(myModel.data(firstLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::IdRole)), u"91785883351310337"_s);
        QCOMPARE(myModel.data(firstLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::NameRole)), u"Test Sensitivity"_s);
        QCOMPARE(myModel.data(firstLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::PriorityRole)), 0);
        QCOMPARE(myModel.data(firstLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::DescriptionRole)), u"This is a long description"_s);
        QCOMPARE(myModel.data(firstLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::ColorRole)), u"bf4040"_s);
        QCOMPARE(myModel.data(firstLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::SelectedRole)).toBool(), true);

        const auto secondLabelIndex = myModel.index(1);
        QVERIFY(secondLabelIndex.isValid());
        QCOMPARE(myModel.data(secondLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::IdRole)), u"91345129959310149"_s);
        QCOMPARE(myModel.data(secondLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::NameRole)), u"High Sensitivity"_s);
        QCOMPARE(myModel.data(secondLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::PriorityRole)), 1);
        QCOMPARE(myModel.data(secondLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::DescriptionRole)), u"This is a long description for high sensitive label"_s);
        QCOMPARE(myModel.data(secondLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::ColorRole)), u"56cd40"_s);
        QCOMPARE(myModel.data(secondLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::SelectedRole)).toBool(), false);

        const auto noneLabelIndex = myModel.index(2);
        QVERIFY(noneLabelIndex.isValid());
        QCOMPARE(myModel.data(noneLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::IdRole)), -1);
        QCOMPARE(myModel.data(noneLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::NameRole)), u"None"_s);
        QCOMPARE(myModel.data(noneLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::PriorityRole)), -1);
        QCOMPARE(myModel.data(noneLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::DescriptionRole)), u"No label"_s);
        QCOMPARE(myModel.data(noneLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::ColorRole)), {});
        QCOMPARE(myModel.data(noneLabelIndex, static_cast<int>(GovernanceLabelsListModel::LabelsListModelRoles::SelectedRole)).toBool(), false);
    }

    void testGovernanceLabelListModel_refreshData()
    {
        GovernanceLabelsListModel myModel;
        QAbstractItemModelTester myModelTester(&myModel);
        QSignalSpy modelRefreshDataSignalSpy(&myModel, &GovernanceLabelsListModel::refreshAvailableLabelsData);

        myModel.setEntityId(u"117"_s);
        myModel.setLabelType(Governance::LabelType::Sensitivity);

        QCOMPARE(modelRefreshDataSignalSpy.count(), 1);
        QCOMPARE(modelRefreshDataSignalSpy.at(0).count(), 2);
        QCOMPARE(modelRefreshDataSignalSpy.at(0).at(0).value<OCC::Governance::LabelType>(), OCC::Governance::LabelType::Sensitivity);
        QCOMPARE(modelRefreshDataSignalSpy.at(0).at(1), u"117"_s);
    }
};

QTEST_GUILESS_MAIN(TestGovernance)
#include "testgovernance.moc"
