/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "syncenginetestutils.h"
#include "clientsideencryption.h"
#include "foldermetadata.h"
#include "common/checksums.h"
#include <QtTest>

using namespace OCC;

class TestClientSideEncryptionV2 : public QObject
{
    Q_OBJECT

    QScopedPointer<FakeQNAM> _fakeQnam;
    QScopedPointer<FolderMetadata> _parsedMetadataWithFileDrop;
    QScopedPointer<FolderMetadata> _parsedMetadataAfterProcessingFileDrop;

    AccountPtr _account;
    AccountPtr _secondAccount;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        QVariantMap fakeCapabilities;
        fakeCapabilities[QStringLiteral("end-to-end-encryption")] = QVariantMap{
            {QStringLiteral("enabled"), true},
            {QStringLiteral("api-version"), "2.0"}
        };
        const QUrl fakeUrl("http://example.de");

        {
            _account = Account::create();
            _fakeQnam.reset(new FakeQNAM({}));
            const auto cred = new FakeCredentials{_fakeQnam.data()};
            cred->setUserName("test");
            _account->setCredentials(cred);
            _account->setUrl(fakeUrl);
            _account->setCapabilities(fakeCapabilities);
        }
        {
            // make a second fake account so we can share metadata to it later
            _secondAccount = Account::create();
            _fakeQnam.reset(new FakeQNAM({}));
            const auto credSecond = new FakeCredentials{_fakeQnam.data()};
            credSecond->setUserName("sharee");
            _secondAccount->setCredentials(credSecond);
            _secondAccount->setUrl(fakeUrl);
            _secondAccount->setCapabilities(fakeCapabilities);        
        }

        QSslCertificate cert;
        QSslKey publicKey;
        QByteArray privateKey;

        {
            QFile e2eTestFakeCert(QStringLiteral("e2etestsfakecert.pem"));
            QVERIFY(e2eTestFakeCert.open(QFile::ReadOnly));
            cert = QSslCertificate(e2eTestFakeCert.readAll());
        }
        {
            QFile e2etestsfakecertpublickey(QStringLiteral("e2etestsfakecertpublickey.pem"));
            QVERIFY(e2etestsfakecertpublickey.open(QFile::ReadOnly));
            publicKey = QSslKey(e2etestsfakecertpublickey.readAll(), QSsl::KeyAlgorithm::Rsa, QSsl::EncodingFormat::Pem, QSsl::KeyType::PublicKey);
            e2etestsfakecertpublickey.close();
        }
        {
            QFile e2etestsfakecertprivatekey(QStringLiteral("e2etestsfakecertprivatekey.pem"));
            QVERIFY(e2etestsfakecertprivatekey.open(QFile::ReadOnly));
            privateKey = e2etestsfakecertprivatekey.readAll();
        }

        QVERIFY(!cert.isNull());
        QVERIFY(!publicKey.isNull());
        QVERIFY(!privateKey.isEmpty());

        _account->e2e()->setCertificate(cert);
        _account->e2e()->setPrivateKey(privateKey);

        _secondAccount->e2e()->setCertificate(cert);
        _secondAccount->e2e()->setPrivateKey(privateKey);
        
    }

    void testInitializeNewRootFolderMetadataThenEncryptAndDecrypt()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto fakeFileName = "fakefile.txt";

        FolderMetadata::EncryptedFile encryptedFile;
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = fakeFileName;
        encryptedFile.mimetype = "application/octet-stream";
        encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
        metadata->addEncryptedFile(encryptedFile);

        const auto encryptedMetadata = metadata->encryptedMetadata();
        QVERIFY(!encryptedMetadata.isEmpty());

        const auto signature = metadata->metadataSignature();
        QVERIFY(!signature.isEmpty());

        const auto metaDataDoc = QJsonDocument::fromJson(encryptedMetadata);
        const auto folderUsers = metaDataDoc["users"].toArray();
        QVERIFY(!folderUsers.isEmpty());

        auto isCurrentUserPresentAndCanDecrypt = false;
        for (auto it = folderUsers.constBegin(); it != folderUsers.constEnd(); ++it) {
            const auto folderUserObject = it->toObject();
            const auto userId = folderUserObject.value("userId").toString();

            if (userId != _account->davUser()) {
                continue;
            }

            const auto certificatePem = folderUserObject.value("certificate").toString().toUtf8();
            const auto certificate = QSslCertificate{certificatePem};
            const auto encryptedMetadataKey = QByteArray::fromBase64(folderUserObject.value("encryptedMetadataKey").toString().toUtf8());

            if (!encryptedMetadataKey.isEmpty()) {
                const auto decryptedMetadataKey = metadata->decryptDataWithPrivateKey(encryptedMetadataKey, _account->e2e()->certificateSha256Fingerprint());
                if (decryptedMetadataKey.isEmpty()) {
                    break;
                }
                
                const auto metadataObj = metaDataDoc.object()["metadata"].toObject();

                const auto cipherTextEncrypted = metadataObj["ciphertext"].toString().toLocal8Bit();

                // for compatibility, the format is "cipheredpart|initializationVector", so we need to extract the "cipheredpart"
                const auto cipherTextPartExtracted = cipherTextEncrypted.split('|').at(0);

                const auto nonce = QByteArray::fromBase64(metadataObj["nonce"].toString().toLocal8Bit());

                const auto cipherTextDecrypted =
                    EncryptionHelper::decryptThenUnGzipData(decryptedMetadataKey, QByteArray::fromBase64(cipherTextPartExtracted), nonce);
                if (cipherTextDecrypted.isEmpty()) {
                    break;
                }

                const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);
                const auto files = cipherTextDocument.object()["files"].toObject();

                if (files.isEmpty()) {
                    break;
                }

                const auto parsedEncryptedFile = metadata->parseEncryptedFileFromJson(files.keys().first(), files.value(files.keys().first()));

                QCOMPARE(parsedEncryptedFile.originalFilename, fakeFileName);

                isCurrentUserPresentAndCanDecrypt = true;
                break;
            }
        }
        QEXPECT_FAIL("", "to be fixed later or removed entirely", Continue);
        QVERIFY(isCurrentUserPresentAndCanDecrypt);

        auto encryptedMetadataCopy = encryptedMetadata;
        encryptedMetadataCopy.replace("\"", "\\\""); 

        QJsonDocument ocsDoc = QJsonDocument::fromJson(QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(encryptedMetadataCopy)).toUtf8());
        

        QScopedPointer<FolderMetadata> metadataFromJson(new FolderMetadata(_account, "/",
            ocsDoc.toJson(),
            RootEncryptedFolderInfo::makeDefault(), signature));
        QSignalSpy metadataSetupExistingCompleteSpy(metadataFromJson.data(), &FolderMetadata::setupComplete);
        metadataSetupExistingCompleteSpy.wait();
        QCOMPARE(metadataSetupExistingCompleteSpy.count(), 1);
        QEXPECT_FAIL("", "to be fixed later or removed entirely", Continue);
        QVERIFY(metadataFromJson->isValid());
    }

    void testFolderMetadataWithEmptySignatureDecryptFails()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto encryptedMetadata = metadata->encryptedMetadata();
        QVERIFY(!encryptedMetadata.isEmpty());

        const auto signature = metadata->metadataSignature();
        QVERIFY(!signature.isEmpty());

        auto encryptedMetadataCopy = encryptedMetadata;
        encryptedMetadataCopy.replace("\"", "\\\"");

        const QJsonDocument ocsDoc = QJsonDocument::fromJson(QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}")
                                                           .arg(QString::fromUtf8(encryptedMetadataCopy)).toUtf8());

        const QByteArray emptySignature = {};
        QScopedPointer<FolderMetadata> metadataFromJson(new FolderMetadata(_account, "/",
                                                                           ocsDoc.toJson(),
                                                                           RootEncryptedFolderInfo::makeDefault(),
                                                                           emptySignature));

        QSignalSpy metadataSetupExistingCompleteSpy(metadataFromJson.data(), &FolderMetadata::setupComplete);
        metadataSetupExistingCompleteSpy.wait();
        QCOMPARE(metadataSetupExistingCompleteSpy.count(), 1);

        QVERIFY(metadataFromJson->metadataSignature().isEmpty());
        QVERIFY(metadataFromJson->metadataKeyForDecryption().isEmpty());
        QVERIFY(!metadataFromJson->isValid());
    }

    void testE2EeFolderMetadataSharing()
    {
        // instantiate empty metadata, add a file, and share with a second user "sharee"
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto fakeFileName = "fakefile.txt";

        FolderMetadata::EncryptedFile encryptedFile;
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = fakeFileName;
        encryptedFile.mimetype = "application/octet-stream";
        encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
        metadata->addEncryptedFile(encryptedFile);

        QVERIFY(metadata->addUser(_secondAccount->davUser(), _secondAccount->e2e()->getCertificate(), FolderMetadata::CertificateType::SoftwareNextcloudCertificate));

        QVERIFY(metadata->removeUser(_secondAccount->davUser()));

        QVERIFY(metadata->addUser(_secondAccount->davUser(), _secondAccount->e2e()->getCertificate(), FolderMetadata::CertificateType::SoftwareNextcloudCertificate));

        const auto encryptedMetadata = metadata->encryptedMetadata();
        QVERIFY(!encryptedMetadata.isEmpty());

        const auto signature = metadata->metadataSignature();
        QVERIFY(!signature.isEmpty());

        const auto metaDataDoc = QJsonDocument::fromJson(encryptedMetadata);
        const auto folderUsers = metaDataDoc["users"].toArray();
        QVERIFY(!folderUsers.isEmpty());

        // make sure metadata setup was a success and we can parse and decrypt it with a second account "sharee"
        auto isShareeUserPresentAndCanDecrypt = false;
        for (auto it = folderUsers.constBegin(); it != folderUsers.constEnd(); ++it) {
            const auto folderUserObject = it->toObject();
            const auto userId = folderUserObject.value("userId").toString();

            if (userId != _secondAccount->davUser()) {
                continue;
            }

            const auto certificatePem = folderUserObject.value("certificate").toString().toUtf8();
            const auto certificate = QSslCertificate{certificatePem};
            const auto encryptedMetadataKey = QByteArray::fromBase64(folderUserObject.value("encryptedMetadataKey").toString().toUtf8());

            if (!encryptedMetadataKey.isEmpty()) {
                const auto decryptedMetadataKey = metadata->decryptDataWithPrivateKey(encryptedMetadataKey, _account->e2e()->certificateSha256Fingerprint());
                if (decryptedMetadataKey.isEmpty()) {
                    break;
                }

                const auto metadataObj = metaDataDoc.object()["metadata"].toObject();

                const auto cipherTextEncrypted = metadataObj["ciphertext"].toString().toLocal8Bit();

                // for compatibility, the format is "cipheredpart|initializationVector", so we need to extract the "cipheredpart"
                const auto cipherTextPartExtracted = cipherTextEncrypted.split('|').at(0);

                const auto nonce = QByteArray::fromBase64(metadataObj["nonce"].toString().toLocal8Bit());

                const auto cipherTextDecrypted =
                    EncryptionHelper::decryptThenUnGzipData(decryptedMetadataKey, QByteArray::fromBase64(cipherTextPartExtracted), nonce);
                if (cipherTextDecrypted.isEmpty()) {
                    break;
                }

                const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);
                const auto files = cipherTextDocument.object()["files"].toObject();

                if (files.isEmpty()) {
                    break;
                }

                const auto parsedEncryptedFile = metadata->parseEncryptedFileFromJson(files.keys().first(), files.value(files.keys().first()));

                QCOMPARE(parsedEncryptedFile.originalFilename, fakeFileName);

                isShareeUserPresentAndCanDecrypt = true;
                break;
            }
        }
        QEXPECT_FAIL("", "to be fixed later or removed entirely", Abort);
        QVERIFY(isShareeUserPresentAndCanDecrypt);

        // now, setup existing metadata for the second user "sharee", add a file, and get encrypted JSON again
        auto encryptedMetadataCopy = encryptedMetadata;
        encryptedMetadataCopy.replace("\"", "\\\"");

        QJsonDocument ocsDoc =
            QJsonDocument::fromJson(QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(encryptedMetadataCopy)).toUtf8());

        QScopedPointer<FolderMetadata> metadataFromJsonForSecondUser(new FolderMetadata(_secondAccount, "/", ocsDoc.toJson(), RootEncryptedFolderInfo::makeDefault(), signature));
        QSignalSpy metadataSetupExistingCompleteSpy(metadataFromJsonForSecondUser.data(), &FolderMetadata::setupComplete);
        metadataSetupExistingCompleteSpy.wait();
        QCOMPARE(metadataSetupExistingCompleteSpy.count(), 1);
        QEXPECT_FAIL("", "to be fixed later or removed entirely", Continue);
        QVERIFY(metadataFromJsonForSecondUser->isValid());

        const auto fakeFileNameFromSecondUser = "fakefileFromSecondUser.txt";
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = fakeFileNameFromSecondUser;
        encryptedFile.mimetype = "application/octet-stream";
        encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
        metadataFromJsonForSecondUser->addEncryptedFile(encryptedFile);

        auto encryptedMetadataFromSecondUser = metadataFromJsonForSecondUser->encryptedMetadata();
        encryptedMetadataFromSecondUser.replace("\"", "\\\"");

        const auto signatureAfterSecondUserModification = metadataFromJsonForSecondUser->metadataSignature();
        QVERIFY(!signatureAfterSecondUserModification.isEmpty());

        QJsonDocument ocsDocFromSecondUser = QJsonDocument::fromJson(
            QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(encryptedMetadataFromSecondUser)).toUtf8());

        QScopedPointer<FolderMetadata> metadataFromJsonForFirstUserToCheckCrossSharing(new FolderMetadata(_account, "/",
                                                                                                          ocsDocFromSecondUser.toJson(),
                                                                                                          RootEncryptedFolderInfo::makeDefault(),
                                                                                                          signatureAfterSecondUserModification));
        QSignalSpy metadataSetupForCrossSharingCompleteSpy(metadataFromJsonForFirstUserToCheckCrossSharing.data(), &FolderMetadata::setupComplete);
        metadataSetupForCrossSharingCompleteSpy.wait();
        QCOMPARE(metadataSetupForCrossSharingCompleteSpy.count(), 1);
        QVERIFY(metadataFromJsonForFirstUserToCheckCrossSharing->isValid());

        // now, check if the first user can decrypt metadata and get the file info added by the second user "sharee"
        const auto encryptedMetadataForFirstUserCrossSharing = metadataFromJsonForFirstUserToCheckCrossSharing->encryptedMetadata();
        QVERIFY(!encryptedMetadataForFirstUserCrossSharing.isEmpty());

        const auto metaDataDocForFirstUserCrossSharing = QJsonDocument::fromJson(encryptedMetadataForFirstUserCrossSharing);
        const auto folderUsersForFirstUserCrossSharing = metaDataDocForFirstUserCrossSharing["users"].toArray();
        QVERIFY(!folderUsers.isEmpty());

        // make sure metadata setup was a success and we can parse and decrypt it with a second account "sharee"
        auto isFirstUserPresentAndCanDecrypt = false;
        for (auto it = folderUsersForFirstUserCrossSharing.constBegin(); it != folderUsersForFirstUserCrossSharing.constEnd(); ++it) {
            const auto folderUserObject = it->toObject();
            const auto userId = folderUserObject.value("userId").toString();

            if (userId != _secondAccount->davUser()) {
                continue;
            }

            const auto certificatePem = folderUserObject.value("certificate").toString().toUtf8();
            const auto certificate = QSslCertificate{certificatePem};
            const auto encryptedMetadataKey = QByteArray::fromBase64(folderUserObject.value("encryptedMetadataKey").toString().toUtf8());

            if (!encryptedMetadataKey.isEmpty()) {
                const auto decryptedMetadataKey = metadata->decryptDataWithPrivateKey(encryptedMetadataKey, _account->e2e()->certificateSha256Fingerprint());
                if (decryptedMetadataKey.isEmpty()) {
                    break;
                }

                const auto metadataObj = metaDataDocForFirstUserCrossSharing.object()["metadata"].toObject();

                const auto cipherTextEncrypted = metadataObj["ciphertext"].toString().toLocal8Bit();

                // for compatibility, the format is "cipheredpart|initializationVector", so we need to extract the "cipheredpart"
                const auto cipherTextPartExtracted = cipherTextEncrypted.split('|').at(0);

                const auto nonce = QByteArray::fromBase64(metadataObj["nonce"].toString().toLocal8Bit());

                const auto cipherTextDecrypted =
                    EncryptionHelper::decryptThenUnGzipData(decryptedMetadataKey, QByteArray::fromBase64(cipherTextPartExtracted), nonce);
                if (cipherTextDecrypted.isEmpty()) {
                    break;
                }

                const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);
                const auto files = cipherTextDocument.object()["files"].toObject();

                if (files.isEmpty()) {
                    break;
                }

                FolderMetadata::EncryptedFile foundFile;
                for (auto it = files.constBegin(), end = files.constEnd(); it != end; ++it) {
                    const auto parsedEncryptedFile = metadata->parseEncryptedFileFromJson(it.key(), it.value());
                    if (!parsedEncryptedFile.originalFilename.isEmpty() && parsedEncryptedFile.originalFilename == fakeFileNameFromSecondUser) {
                        foundFile = parsedEncryptedFile;
                    }
                }
                QCOMPARE(foundFile.originalFilename, fakeFileNameFromSecondUser);

                isFirstUserPresentAndCanDecrypt = true;
                break;
            }
        }
        QVERIFY(isFirstUserPresentAndCanDecrypt);
    }

    void testRejectsKeyChecksumRemoval()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        FolderMetadata::EncryptedFile encryptedFile;
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = "fakefile.txt";
        encryptedFile.mimetype = "application/octet-stream";
        encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
        metadata->addEncryptedFile(encryptedFile);

        QVERIFY(metadata->addUser(_secondAccount->davUser(), _secondAccount->e2e()->getCertificate(), FolderMetadata::CertificateType::SoftwareNextcloudCertificate));

        const auto previousChecksums = metadata->keyChecksums();
        QVERIFY(previousChecksums.size() > 1);

        const auto currentChecksum = calcSha256(metadata->metadataKeyForEncryption());
        QVERIFY(previousChecksums.contains(currentChecksum));

        auto reducedChecksums = previousChecksums;
        for (const auto &checksum : previousChecksums) {
            if (checksum != currentChecksum) {
                reducedChecksums.remove(checksum);
                break;
            }
        }
        QCOMPARE(reducedChecksums.size(), previousChecksums.size() - 1);

        metadata->_keyChecksums = reducedChecksums;

        const auto tamperedMetadata = metadata->encryptedMetadata();
        const auto tamperedSignature = metadata->metadataSignature();
        QVERIFY(!tamperedMetadata.isEmpty());
        QVERIFY(!tamperedSignature.isEmpty());

        auto tamperedMetadataCopy = tamperedMetadata;
        tamperedMetadataCopy.replace("\"", "\\\"");
        const QJsonDocument ocsDoc = QJsonDocument::fromJson(
            QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(tamperedMetadataCopy)).toUtf8());

        const auto rootInfo = RootEncryptedFolderInfo(QStringLiteral("/"),
                                                      metadata->metadataKeyForEncryption(),
                                                      metadata->metadataKeyForEncryption(),
                                                      previousChecksums);

        QScopedPointer<FolderMetadata> metadataFromJson(new FolderMetadata(_account, "/", ocsDoc.toJson(), rootInfo, tamperedSignature));
        QSignalSpy metadataSetupExistingCompleteSpy(metadataFromJson.data(), &FolderMetadata::setupComplete);
        metadataSetupExistingCompleteSpy.wait();
        QCOMPARE(metadataSetupExistingCompleteSpy.count(), 1);
        QVERIFY(!metadataFromJson->isValid());
    }

    void testRejectsCounterRollback()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        FolderMetadata::EncryptedFile encryptedFile;
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = "fakefile.txt";
        encryptedFile.mimetype = "application/octet-stream";
        encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
        metadata->addEncryptedFile(encryptedFile);

        const auto encryptedMetadata = metadata->encryptedMetadata();
        const auto signature = metadata->metadataSignature();
        QVERIFY(!encryptedMetadata.isEmpty());
        QVERIFY(!signature.isEmpty());

        auto encryptedMetadataCopy = encryptedMetadata;
        encryptedMetadataCopy.replace("\"", "\\\"");
        const QJsonDocument ocsDoc = QJsonDocument::fromJson(
            QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(encryptedMetadataCopy)).toUtf8());

        const auto previousCounter = metadata->newCounter() + 1;
        const auto rootInfo = RootEncryptedFolderInfo(QStringLiteral("/"), {}, {}, metadata->keyChecksums(), previousCounter);

        QScopedPointer<FolderMetadata> metadataFromJson(new FolderMetadata(_account, "/", ocsDoc.toJson(), rootInfo, signature));
        QSignalSpy metadataSetupExistingCompleteSpy(metadataFromJson.data(), &FolderMetadata::setupComplete);
        metadataSetupExistingCompleteSpy.wait();
        QCOMPARE(metadataSetupExistingCompleteSpy.count(), 1);
        QVERIFY(!metadataFromJson->isValid());
    }
};

QTEST_GUILESS_MAIN(TestClientSideEncryptionV2)
#include "testclientsideencryptionv2.moc"
