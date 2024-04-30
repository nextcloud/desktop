/*
 * Copyright (C) 2024 by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#include "syncenginetestutils.h"
#include "clientsideencryption.h"
#include "foldermetadata.h"
#include <array>
#include <QtTest>

namespace
{
   const std::array<QString, 2> fakeFiles{"fakefile.txt", "fakefile1.txt"};
   const std::array<QString, 2> fakeFilesFileDrop{"fakefiledropped.txt", "fakefiledropped1.txt"};
};

using namespace OCC;

class TestSecureFileDrop : public QObject
{
    Q_OBJECT

    QScopedPointer<FakeQNAM> _fakeQnam;
    AccountPtr _account;

    QScopedPointer<FolderMetadata> _parsedMetadataWithFileDrop;
    QScopedPointer<FolderMetadata> _parsedMetadataAfterProcessingFileDrop;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        QVariantMap capabilities;
        capabilities[QStringLiteral("end-to-end-encryption")] = QVariantMap{{QStringLiteral("enabled"), true}, {QStringLiteral("api-version"), "2.0"}};

        _account = Account::create();
        const QUrl url("http://example.de");
        _fakeQnam.reset(new FakeQNAM({}));
        const auto cred = new FakeCredentials{_fakeQnam.data()};
        cred->setUserName("test");
        _account->setCredentials(cred);
        _account->setUrl(url);
        _account->setCapabilities(capabilities);

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

        _account->e2e()->_certificate = cert;
        _account->e2e()->_publicKey = publicKey;
        _account->e2e()->_privateKey = privateKey;

        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/",  FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        for (const auto &fakeFileName : fakeFiles) {
            FolderMetadata::EncryptedFile encryptedFile;
            encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
            encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
            encryptedFile.originalFilename = fakeFileName;
            encryptedFile.mimetype = "application/octet-stream";
            encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
            metadata->addEncryptedFile(encryptedFile);
        }

        QJsonObject fakeFileDropPart;

        QJsonArray fileDropUsers;
        for (const auto &folderUser : metadata->_folderUsers) {
            QJsonObject fileDropUser;
            fileDropUser.insert("userId", folderUser.userId);
            fileDropUser.insert("encryptedFiledropKey", QString::fromUtf8(folderUser.encryptedMetadataKey.toBase64()));
            fileDropUsers.push_back(fileDropUser);
        }

        for (const auto &fakeFileName : fakeFilesFileDrop) {
            FolderMetadata::EncryptedFile encryptedFile;
            encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
            encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
            encryptedFile.originalFilename = fakeFileName;
            encryptedFile.mimetype = "application/octet-stream";
            encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);

            QJsonObject fakeFileDropEntry;
            fakeFileDropEntry.insert("ciphertext", "");

            QJsonObject fakeFileDropMetadataObject;
            fakeFileDropMetadataObject.insert("filename", encryptedFile.originalFilename);
            fakeFileDropMetadataObject.insert("mimetype", QString::fromUtf8(encryptedFile.mimetype));
            fakeFileDropMetadataObject.insert("nonce", QString::fromUtf8(encryptedFile.initializationVector.toBase64()));
            fakeFileDropMetadataObject.insert("key", QString::fromUtf8(encryptedFile.encryptionKey.toBase64()));
            fakeFileDropMetadataObject.insert("authenticationTag", QString::fromUtf8(QByteArrayLiteral("123").toBase64()));
            QJsonDocument fakeFileDropMetadata;
            fakeFileDropMetadata.setObject(fakeFileDropMetadataObject);


            QByteArray authenticationTag;
            const auto initializationVector = EncryptionHelper::generateRandom(16);
            const auto cipherTextEncrypted = EncryptionHelper::gzipThenEncryptData(metadata->_metadataKeyForEncryption,
                                                                                   fakeFileDropMetadata.toJson(QJsonDocument::JsonFormat::Compact),
                                                                                   initializationVector,
                                                                                   authenticationTag);
            fakeFileDropEntry.insert("ciphertext", QString::fromUtf8(cipherTextEncrypted.toBase64()));
            fakeFileDropEntry.insert("nonce", QString::fromUtf8(initializationVector.toBase64()));
            fakeFileDropEntry.insert("authenticationTag", QString::fromUtf8(authenticationTag.toBase64()));
            fakeFileDropEntry.insert("users", fileDropUsers);

            fakeFileDropPart.insert(encryptedFile.encryptedFilename, fakeFileDropEntry);
        }
        metadata->setFileDrop(fakeFileDropPart);

        auto encryptedMetadata = metadata->encryptedMetadata();
        encryptedMetadata.replace("\"", "\\\""); 
        const auto signature = metadata->metadataSignature();
        QJsonDocument ocsDoc =
            QJsonDocument::fromJson(QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(encryptedMetadata)).toUtf8());
        _parsedMetadataWithFileDrop.reset(new FolderMetadata(_account, "/",  ocsDoc.toJson(), RootEncryptedFolderInfo::makeDefault(), signature));

        QSignalSpy metadataWithFileDropSetupCompleteSpy(_parsedMetadataWithFileDrop.data(), &FolderMetadata::setupComplete);
        metadataWithFileDropSetupCompleteSpy.wait();
        QCOMPARE(metadataWithFileDropSetupCompleteSpy.count(), 1);
        QVERIFY(_parsedMetadataWithFileDrop->isValid());

        QCOMPARE(_parsedMetadataWithFileDrop->_fileDropEntries.count(), fakeFilesFileDrop.size());
    }

    void testMoveFileDropMetadata()
    {
        QVERIFY(_parsedMetadataWithFileDrop->isFileDropPresent());
        QVERIFY(_parsedMetadataWithFileDrop->moveFromFileDropToFiles());

        auto encryptedMetadata = _parsedMetadataWithFileDrop->encryptedMetadata();
        encryptedMetadata.replace("\"", "\\\"");
        const auto signature = _parsedMetadataWithFileDrop->metadataSignature();
        QJsonDocument ocsDoc =
            QJsonDocument::fromJson(QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(encryptedMetadata)).toUtf8());
        
        _parsedMetadataAfterProcessingFileDrop.reset(new FolderMetadata(_account, "/", ocsDoc.toJson(), RootEncryptedFolderInfo::makeDefault(), signature));

        QSignalSpy metadataAfterProcessingFileDropSetupCompleteSpy(_parsedMetadataAfterProcessingFileDrop.data(), &FolderMetadata::setupComplete);
        metadataAfterProcessingFileDropSetupCompleteSpy.wait();
        QCOMPARE(metadataAfterProcessingFileDropSetupCompleteSpy.count(), 1);
        QVERIFY(_parsedMetadataAfterProcessingFileDrop->isValid());

        for (const auto &fakeFileName : fakeFilesFileDrop) {
            const auto foundInEncryptedFiles = std::find_if(std::cbegin(_parsedMetadataAfterProcessingFileDrop->_files), std::cend(_parsedMetadataAfterProcessingFileDrop->_files), [fakeFileName](const auto &encryptedFile) {
                return encryptedFile.originalFilename == fakeFileName;
            });
            QVERIFY(foundInEncryptedFiles != std::cend(_parsedMetadataAfterProcessingFileDrop->_files));
        }
    }
};

QTEST_GUILESS_MAIN(TestSecureFileDrop)
#include "testsecurefiledrop.moc"
