/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "updatefiledropmetadata.h"
#include "syncengine.h"
#include "syncenginetestutils.h"
#include "testhelper.h"
#include "owncloudpropagator_p.h"
#include "propagatorjobs.h"
#include "clientsideencryption.h"
#include "foldermetadata.h"

#include <QtTest>

namespace
{
   constexpr auto fakeE2eeFolderName = "fake_e2ee_folder";
   const QString fakeE2eeFolderPath = QStringLiteral("/") + fakeE2eeFolderName;
 };

using namespace OCC;

class TestSecureFileDrop : public QObject
{
    Q_OBJECT

    FakeFolder _fakeFolder{FileInfo()};
    QSharedPointer<OwncloudPropagator> _propagator;
    QScopedPointer<FolderMetadata> _parsedMetadataWithFileDrop;
    QScopedPointer<FolderMetadata> _parsedMetadataAfterProcessingFileDrop;

    int _lockCallsCount = 0;
    int _unlockCallsCount = 0;
    int _propFindCallsCount = 0;
    int _getMetadataCallsCount = 0;
    int _putMetadataCallsCount = 0;

private slots:
    void initTestCase()
    {
        _fakeFolder.remoteModifier().mkdir(fakeE2eeFolderName);
        _fakeFolder.remoteModifier().insert(fakeE2eeFolderName + QStringLiteral("/") + QStringLiteral("fake_e2ee_file"), 100);
        _fakeFolder.setServerOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(device);
            QNetworkReply *reply = nullptr;

            const auto path = req.url().path();

            if (path.contains(QStringLiteral("/end_to_end_encryption/api/v1/lock/"))) {
                if (op == QNetworkAccessManager::DeleteOperation) {
                    reply = new FakePayloadReply(op, req, {}, nullptr);
                    ++_unlockCallsCount;
                } else if (op == QNetworkAccessManager::PostOperation) {
                    QFile fakeJsonReplyFile(QStringLiteral("fake2eelocksucceeded.json"));
                    if (fakeJsonReplyFile.open(QFile::ReadOnly)) {
                        const auto jsonDoc = QJsonDocument::fromJson(fakeJsonReplyFile.readAll());
                        reply = new FakePayloadReply(op, req, jsonDoc.toJson(), nullptr);
                        ++_lockCallsCount;
                    } else {
                        qCritical() << "Could not open fake JSON file!";
                        reply = new FakePayloadReply(op, req, {}, nullptr);
                    }
                }
            } else if (path.contains(QStringLiteral("/end_to_end_encryption/api/v1/meta-data/"))) {
                if (op == QNetworkAccessManager::GetOperation) {
                    QFile fakeJsonReplyFile(QStringLiteral("fakefiledrope2eefoldermetadata.json"));
                    if (fakeJsonReplyFile.open(QFile::ReadOnly)) {
                        const auto jsonDoc = QJsonDocument::fromJson(fakeJsonReplyFile.readAll());
                        const auto pathSplit = path.split(QLatin1Char('/'));
                        const QString folderRemotePath = pathSplit.last() + QStringLiteral("/");
                        _parsedMetadataWithFileDrop.reset(new FolderMetadata(_fakeFolder.syncEngine().account(), jsonDoc.toJson(), folderRemotePath));
                        _parsedMetadataAfterProcessingFileDrop.reset(new FolderMetadata(_fakeFolder.syncEngine().account(), jsonDoc.toJson(), folderRemotePath));
                        [[maybe_unused]] const auto result = _parsedMetadataAfterProcessingFileDrop->moveFromFileDropToFiles();
                        reply = new FakePayloadReply(op, req, jsonDoc.toJson(), nullptr);
                        ++_getMetadataCallsCount;
                    } else {
                        qCritical() << "Could not open fake JSON file!";
                        reply = new FakePayloadReply(op, req, {}, nullptr);
                    }
                } else if (op == QNetworkAccessManager::PutOperation) {
                    reply = new FakePayloadReply(op, req, {}, nullptr);
                    ++_putMetadataCallsCount;
                }
            } else if (req.attribute(QNetworkRequest::CustomVerbAttribute) == QStringLiteral("PROPFIND") && path.endsWith(fakeE2eeFolderPath)) {
                auto fileState = _fakeFolder.currentRemoteState();
                reply = new FakePropfindReply(fileState, op, req, nullptr);
                ++_propFindCallsCount;
            }

            return reply;
        });

        auto transProgress = connect(&_fakeFolder.syncEngine(), &SyncEngine::transmissionProgress, [&](const ProgressInfo &pi) {
            Q_UNUSED(pi);
            _propagator = _fakeFolder.syncEngine().getPropagator();
        });

        QVERIFY(_fakeFolder.syncOnce());

        disconnect(transProgress);
    };

    void testUpdateFileDropMetadata()
    {
        const auto updateFileDropMetadataJob = new UpdateFileDropMetadataJob(_propagator.data(), fakeE2eeFolderPath);
        connect(updateFileDropMetadataJob, &UpdateFileDropMetadataJob::fileDropMetadataParsedAndAdjusted, this, [this, updateFileDropMetadataJob](const FolderMetadata *const metadata) {
            if (!metadata || metadata->files().isEmpty() || metadata->fileDrop().isEmpty()) {
                return;
            }

            emit fileDropMetadataParsedAndAdjusted();

            QSignalSpy updateFileDropMetadataJobSpy(updateFileDropMetadataJob, &UpdateFileDropMetadataJob::finished);
            QSignalSpy fileDropMetadataParsedAndAdjustedSpy(this, &TestSecureFileDrop::fileDropMetadataParsedAndAdjusted);

            QVERIFY(updateFileDropMetadataJob->scheduleSelfOrChild());

            QVERIFY(updateFileDropMetadataJobSpy.wait(3000));

            QVERIFY(_parsedMetadataWithFileDrop);
            QVERIFY(_parsedMetadataWithFileDrop->isFileDropPresent());

            QVERIFY(_parsedMetadataAfterProcessingFileDrop);

            QVERIFY(!updateFileDropMetadataJobSpy.isEmpty());
            QVERIFY(!updateFileDropMetadataJobSpy.at(0).isEmpty());
            QCOMPARE(updateFileDropMetadataJobSpy.at(0).first().toInt(), SyncFileItem::Status::Success);

            QVERIFY(!fileDropMetadataParsedAndAdjustedSpy.isEmpty());

            QCOMPARE(_lockCallsCount, 1);
            QCOMPARE(_unlockCallsCount, 1);
            QCOMPARE(_propFindCallsCount, 2);
            QCOMPARE(_getMetadataCallsCount, 1);
            QCOMPARE(_putMetadataCallsCount, 1);

            updateFileDropMetadataJob->deleteLater();
        });
    }

signals:
    void fileDropMetadataParsedAndAdjusted();
};

QTEST_GUILESS_MAIN(TestSecureFileDrop)
#include "testsecurefiledrop.moc"
