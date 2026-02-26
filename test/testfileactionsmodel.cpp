/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryFile>

#include "../src/gui/integration/fileactionsmodel.h"
#include "syncenginetestutils.h"
#include "folderman.h"
#include "testhelper.h"

using namespace OCC;

namespace {
static QByteArray client_integration = R"(
{
    "client_integration": {
        "analytics": {
            "version": 0.1,
            "context-menu": [
                {
                    "name": "Visualize data in Analytics",
                    "url": "/ocs/v2.php/apps/analytics/createFromDataFile",
                    "method": "POST",
                    "mimetype_filters": "text/csv",
                    "params": {
                        "fileId": "{fileId}"
                    },
                    "icon": "/apps/analytics/img/app.svg"
                },
                {
                    "name": "Visualize data in Analytics",
                    "url": "/ocs/v2.php/apps/analytics/createFromDataFile",
                    "method": "POST",
                    "mimetype_filters": "",
                    "params": {
                        "fileId": "{fileId}"
                    },
                    "icon": "/apps/analytics/img/app.svg"
                }
            ]
        },
        "assistant": {
            "version": 0.1,
            "context-menu": [
                {
                    "name": "Summarize using AI",
                    "url": "/ocs/v2.php/apps/assistant/api/v1/file-action/{fileId}/core:text2text:summary",
                    "method": "POST",
                    "mimetype_filters": "text/, application/msword, application/vnd.openxmlformats-officedocument.wordprocessingml.document, application/vnd.oasis.opendocument.text, application/pdf",
                    "icon": "/apps/assistant/img/client_integration/summarize.svg"
                },
                {
                    "name": "Transcribe audio using AI",
                    "url": "/ocs/v2.php/apps/assistant/api/v1/file-action/{fileId}/core:audio2text",
                    "method": "POST",
                    "mimetype_filters": "audio/",
                    "icon": "/apps/assistant/img/client_integration/speech_to_text.svg"
                },
                {
                    "name": "Text-To-Speech using AI",
                    "url": "/ocs/v2.php/apps/assistant/api/v1/file-action/{fileId}/core:text2speech",
                    "method": "POST",
                    "mimetype_filters": "text/, application/msword, application/vnd.openxmlformats-officedocument.wordprocessingml.document, application/vnd.oasis.opendocument.text, application/pdf",
                    "icon": "/apps/assistant/img/client_integration/text_to_speech.svg"
                }
            ]
        },
        "contacts": {
            "version": 0.1,
            "context-menu": [
                {
                    "name": "Import contacts",
                    "url": "/ocs/v2.php/apps/contacts/api/v1/import/{fileId}",
                    "method": "POST",
                    "mimetype_filters": "text/vcard"
                }
            ]
        }
    }
}
)";
}

class TestFileActionsModel : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<FakeFolder> _fakeFolder;
    std::unique_ptr<FolderMan> _folderMan;
    FileActionsModel _model;
    QString _mimeType;

private slots:

    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);
        QStandardPaths::setTestModeEnabled(true);

        _fakeFolder.reset(new FakeFolder{FileInfo{}});
        QCOMPARE(_fakeFolder->currentLocalState(), _fakeFolder->currentRemoteState());

        _folderMan.reset(new FolderMan{});
        auto syncFolderDefinition = folderDefinition(_fakeFolder->localPath());
        const auto folder = FolderMan::instance()->addFolder(&_fakeFolder->accountState(),
                                                             syncFolderDefinition);
        QVERIFY(folder);

        _fakeFolder->account()->setCapabilities(QJsonDocument::fromJson(client_integration)
                                                    .object()
                                                    .toVariantMap());
    }

    void testSetAccountState()
    {
        QSignalSpy accountChangedSpy(&_model,
                                     &FileActionsModel::accountStateChanged);
        _model.setAccountState(&_fakeFolder->accountState());
        QCOMPARE(accountChangedSpy.count(), 1);
        QCOMPARE(_model.accountState(), &_fakeFolder->accountState());
    }

    void testFileChangedDocument()
    {
        // sync file
        const auto fileName = QStringLiteral("document.odt");
        _fakeFolder->localModifier().insert(_fakeFolder->localPath() + fileName);
        QVERIFY(_fakeFolder->syncOnce());
        // set file id
        const auto fileInfo = _fakeFolder->remoteModifier().find(fileName);
        QVERIFY(fileInfo);
        QVERIFY(!fileInfo->fileId.isEmpty());
        _model.setFileId(fileInfo->fileId);
        QCOMPARE(_model.fileId(), fileInfo->fileId);
        // emit fileChanged
        QSignalSpy fileChangedSpy(&_model,
                                  &FileActionsModel::fileChanged);
        _model.setLocalPath(_fakeFolder->localPath() + fileName);
        QCOMPARE(fileChangedSpy.count(), 1);
        QCOMPARE(_model.localPath(), _fakeFolder->localPath() + fileName);
        QCOMPARE(_model.mimeType().name(), QStringLiteral("application/vnd.oasis.opendocument.text"));
        QVERIFY(!_model.fileIcon().isEmpty());
        _model.setRemoteItemPath(_fakeFolder->localPath());
        QCOMPARE(fileChangedSpy.count(), 2);
        QCOMPARE(_model.remoteItemPath(), _fakeFolder->localPath());
        // get file actions from capabilities
        QCOMPARE(_model.rowCount(), 6);
    }

    void testFileChangedImage()
    {
        // sync file
        const auto fileName = QStringLiteral("holidays.png");
        _fakeFolder->localModifier().insert(_fakeFolder->localPath() + fileName);
        QVERIFY(_fakeFolder->syncOnce());
        // set file id
        const auto fileInfo = _fakeFolder->remoteModifier().find(fileName);
        QVERIFY(fileInfo);
        QVERIFY(!fileInfo->fileId.isEmpty());
        _model.setFileId(fileInfo->fileId);
        QCOMPARE(_model.fileId(), fileInfo->fileId);
        // emit fileChanged
        QSignalSpy fileChangedSpy(&_model,
                                  &FileActionsModel::fileChanged);
        _model.setLocalPath(_fakeFolder->localPath() + fileName);
        QCOMPARE(fileChangedSpy.count(), 1);
        QCOMPARE(_model.localPath(), _fakeFolder->localPath() + fileName);
        QCOMPARE(_model.mimeType().name(), QStringLiteral("image/png"));
        QVERIFY(!_model.fileIcon().isEmpty());
        // get file actions from capabilities
        QCOMPARE(_model.rowCount(), 4);
    }

    void testFileChangedVideo()
    {
        // sync file
        const auto fileName = QStringLiteral("holidays.mp4");
        _fakeFolder->localModifier().insert(_fakeFolder->localPath() + fileName);
        QVERIFY(_fakeFolder->syncOnce());
        // set file id
        const auto fileInfo = _fakeFolder->remoteModifier().find(fileName);
        QVERIFY(fileInfo);
        QVERIFY(!fileInfo->fileId.isEmpty());
        _model.setFileId(fileInfo->fileId);
        QCOMPARE(_model.fileId(), fileInfo->fileId);
        // emit fileChanged
        QSignalSpy fileChangedSpy(&_model,
                                  &FileActionsModel::fileChanged);
        _model.setLocalPath(_fakeFolder->localPath() + fileName);
        QCOMPARE(fileChangedSpy.count(), 1);
        QCOMPARE(_model.localPath(), _fakeFolder->localPath() + fileName);
        QCOMPARE(_model.mimeType().name(), QStringLiteral("video/mp4"));
        QVERIFY(!_model.fileIcon().isEmpty());
        // get file actions from capabilities
        QCOMPARE(_model.rowCount(), 4);
    }

    void testFileChangedPlainText()
    {
        // sync file
        const auto fileName = QStringLiteral("test");
        _fakeFolder->localModifier().insert(_fakeFolder->localPath() + fileName);
        QVERIFY(_fakeFolder->syncOnce());
        // set file id
        const auto fileInfo = _fakeFolder->remoteModifier().find(fileName);
        QVERIFY(fileInfo);
        QVERIFY(!fileInfo->fileId.isEmpty());
        _model.setFileId(fileInfo->fileId);
        QCOMPARE(_model.fileId(), fileInfo->fileId);
        // emit fileChanged
        QSignalSpy fileChangedSpy(&_model,
                                  &FileActionsModel::fileChanged);
        _model.setLocalPath(_fakeFolder->localPath() + fileName);
        QCOMPARE(fileChangedSpy.count(), 1);
        QCOMPARE(_model.localPath(), _fakeFolder->localPath() + fileName);
        QCOMPARE(_model.mimeType().name(), QStringLiteral("text/plain"));
        QVERIFY(!_model.fileIcon().isEmpty());
        // get file actions from capabilities
        QCOMPARE(_model.rowCount(), 4);
    }

    void testFileChangedContact()
    {
        // sync file
        const auto fileName = QStringLiteral("contact.vcf");
        _fakeFolder->localModifier().insert(_fakeFolder->localPath() + fileName);
        QVERIFY(_fakeFolder->syncOnce());
        // set file id
        const auto fileInfo = _fakeFolder->remoteModifier().find(fileName);
        QVERIFY(fileInfo);
        QVERIFY(!fileInfo->fileId.isEmpty());
        _model.setFileId(fileInfo->fileId);
        QCOMPARE(_model.fileId(), fileInfo->fileId);
        // emit fileChanged
        QSignalSpy fileChangedSpy(&_model,
                                  &FileActionsModel::fileChanged);
        _model.setLocalPath(_fakeFolder->localPath() + fileName);
        QCOMPARE(fileChangedSpy.count(), 1);
        QCOMPARE(_model.localPath(), _fakeFolder->localPath() + fileName);
#ifndef Q_OS_LINUX
        QCOMPARE(_model.mimeType().name(), QStringLiteral("text/x-vcard"));
#else
        QCOMPARE(_model.mimeType().name(), QStringLiteral("text/vcard"));
#endif
        QVERIFY(!_model.fileIcon().isEmpty());
        // get file actions from capabilities
        QCOMPARE(_model.rowCount(), 5);
    }

    void testDataRoles()
    {
        // sync file
        const auto fileName = QStringLiteral("random.odt");
        _fakeFolder->localModifier().insert(_fakeFolder->localPath() + fileName);
        QVERIFY(_fakeFolder->syncOnce());
        // set file id
        const auto fileInfo = _fakeFolder->remoteModifier().find(fileName);
        QVERIFY(fileInfo);
        QVERIFY(!fileInfo->fileId.isEmpty());
        _model.setFileId(fileInfo->fileId);
        QCOMPARE(_model.fileId(), fileInfo->fileId);
        // emit fileChanged
        _model.setLocalPath(_fakeFolder->localPath() + fileName);
        // get file actions from capabilities
        QCOMPARE(_model.rowCount(), 6);
        // check for data roles
        for (auto i = 0; i < _model.rowCount(); ++i) {
            const auto index = _model.index(i);
            QVERIFY(!index.data(FileActionsModel::DataRole::FileActionIconRole).toString().isEmpty());
            QVERIFY(!index.data(FileActionsModel::DataRole::FileActionNameRole).toString().isEmpty());
            QVERIFY(!index.data(FileActionsModel::DataRole::FileActionUrlRole).toString().isEmpty());
            QVERIFY(!index.data(FileActionsModel::DataRole::FileActionMethodRole).toString().isEmpty());
            // FileActionsModel::FileActionParamsRole can be empty
            // FileActionsModel::DataRole::FileActionResponseLabelRole can be empty
            // FileActionsModel::DataRole::FileActionResponseUrlRole can be empty
        }
    }

    void testParseUrl()
    {
        // sync file
        const auto fileName = QStringLiteral("random2");
        _fakeFolder->localModifier().insert(_fakeFolder->localPath() + fileName);
        QVERIFY(_fakeFolder->syncOnce());
        // set file id
        const auto fileInfo = _fakeFolder->remoteModifier().find(fileName);
        QVERIFY(fileInfo);
        QVERIFY(!fileInfo->fileId.isEmpty());
        _model.setFileId(fileInfo->fileId);
        QCOMPARE(_model.fileId(), fileInfo->fileId);
        // emit fileChanged
        _model.setLocalPath(_fakeFolder->localPath() + fileName);
        // get file actions from capabilities
        QCOMPARE(_model.rowCount(), 4);
        // parse request url
        for (auto i = 0; i < _model.rowCount(); ++i) {
            auto index = _model.index(i);
            auto url = index.data(FileActionsModel::DataRole::FileActionUrlRole).toString();
            const auto requestUrl = _model.parseUrl(url);
            if (url.contains("{fileId}")) { // "/ocs/v2.php/apps/assistant/api/v1/file-action/{fileId}/core:text2text:summary"
                QVERIFY(requestUrl.contains(fileInfo->fileId));
            }
        }
    }
};

QTEST_MAIN(TestFileActionsModel)
#include "testfileactionsmodel.moc"
