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

        {
            QFile e2eTestFakeCert(QStringLiteral("e2etestsfakecert.pem"));
            if (e2eTestFakeCert.open(QFile::ReadOnly)) {
                _fakeFolder.syncEngine().account()->e2e()->_certificate = QSslCertificate(e2eTestFakeCert.readAll());
                e2eTestFakeCert.close();
            }
        }
        {
            QFile e2etestsfakecertpublickey(QStringLiteral("e2etestsfakecertpublickey.pem"));
            if (e2etestsfakecertpublickey.open(QFile::ReadOnly)) {
                _fakeFolder.syncEngine().account()->e2e()->_publicKey = QSslKey(e2etestsfakecertpublickey.readAll(), QSsl::KeyAlgorithm::Rsa, QSsl::EncodingFormat::Pem, QSsl::KeyType::PublicKey);
                e2etestsfakecertpublickey.close();
            }
        }
        {
            QFile e2etestsfakecertprivatekey(QStringLiteral("e2etestsfakecertprivatekey.pem"));
            if (e2etestsfakecertprivatekey.open(QFile::ReadOnly)) {
                _fakeFolder.syncEngine().account()->e2e()->_privateKey = e2etestsfakecertprivatekey.readAll();
                e2etestsfakecertprivatekey.close();
            }
        }

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
                        _parsedMetadataWithFileDrop.reset(new FolderMetadata(_fakeFolder.syncEngine().account(), FolderMetadata::RequiredMetadataVersion::Version1_2, jsonDoc.toJson()));
                        _parsedMetadataAfterProcessingFileDrop.reset(new FolderMetadata(_fakeFolder.syncEngine().account(), FolderMetadata::RequiredMetadataVersion::Version1_2, jsonDoc.toJson()));
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
        connect(updateFileDropMetadataJob, &UpdateFileDropMetadataJob::fileDropMetadataParsedAndAdjusted, this, [this](const FolderMetadata *const metadata) {
            if (!metadata || metadata->files().isEmpty() || metadata->fileDrop().isEmpty()) {
                return;
            }

            if (_parsedMetadataAfterProcessingFileDrop->files().size() != metadata->files().size()) {
                return;
            }

            if (_parsedMetadataAfterProcessingFileDrop->fileDrop() != metadata->fileDrop()) {
                return;
            }

            bool isAnyFileDropFileMissing = false;

            for (const auto &key : metadata->fileDrop().keys()) {
                if (std::find_if(metadata->files().constBegin(), metadata->files().constEnd(), [&key](const EncryptedFile &encryptedFile) {
                    return encryptedFile.encryptedFilename == key;
                }) == metadata->files().constEnd()) {
                    isAnyFileDropFileMissing = true;
                }
            }

            if (!isAnyFileDropFileMissing) {
                emit fileDropMetadataParsedAndAdjusted();
            }
        });
        QSignalSpy updateFileDropMetadataJobSpy(updateFileDropMetadataJob, &UpdateFileDropMetadataJob::finished);
        QSignalSpy fileDropMetadataParsedAndAdjustedSpy(this, &TestSecureFileDrop::fileDropMetadataParsedAndAdjusted);
        
        QVERIFY(updateFileDropMetadataJob->scheduleSelfOrChild());

        QVERIFY(updateFileDropMetadataJobSpy.wait(3000));

        QVERIFY(_parsedMetadataWithFileDrop);
        QVERIFY(_parsedMetadataWithFileDrop->isFileDropPresent());

        QVERIFY(_parsedMetadataAfterProcessingFileDrop);

        QVERIFY(_parsedMetadataAfterProcessingFileDrop->files().size() != _parsedMetadataWithFileDrop->files().size());

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
    }

signals:
    void fileDropMetadataParsedAndAdjusted();
};

QTEST_GUILESS_MAIN(TestSecureFileDrop)
#include "testsecurefiledrop.moc"
