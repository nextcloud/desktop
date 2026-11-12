/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "clientsideencryption.h"
#include "foldermetadata.h"
#include "syncenginetestutils.h"
#include <QtTest>
#include <array>

namespace
{
const std::array<QString, 2> fakeFiles{"fakefile.txt", "fakefile1.txt"};
const std::array<QString, 2> fakeFilesFileDrop{"fakefiledropped.txt", "fakefiledropped1.txt"};
};

using namespace OCC;
using namespace Qt::StringLiterals;

class TestSecureFileDrop : public QObject
{
    Q_OBJECT

    QScopedPointer<FakeQNAM> _fakeQnam;
    AccountPtr _account;

    QScopedPointer<FolderMetadata> _parsedMetadataWithFileDrop;
    QScopedPointer<FolderMetadata> _parsedMetadataAfterProcessingFileDrop;

    [[nodiscard]] FolderMetadata::EncryptedFile encryptedFile(const QString &name, const QString &encryptedName) const
    {
        return {encryptedName.toUtf8().leftJustified(16, 'k', true), "application/octet-stream", QByteArray(16, 'n'), QByteArray(16, 't'), encryptedName, name};
    }

    void appendFileDropEntry(FolderMetadata &metadata, const FolderMetadata::EncryptedFile &file)
    {
        const auto fileJson = QJsonObject{{"filename", file.originalFilename},
                                          {"mimetype", QString::fromUtf8(file.mimetype)},
                                          {"key", QString::fromUtf8(file.encryptionKey.toBase64())},
                                          {"nonce", QString::fromUtf8(file.initializationVector.toBase64())},
                                          {"authenticationTag", QString::fromUtf8(file.authenticationTag.toBase64())}};
        const auto fileDropKey = QByteArray(16, 'd');
        auto entry = FolderMetadata::FileDropEntry{};
        entry.encryptedFilename = file.encryptedFilename;
        entry.nonce = QByteArray(16, 'v');
        entry.cipherText =
            EncryptionHelper::gzipThenEncryptData(fileDropKey, QJsonDocument(fileJson).toJson(QJsonDocument::Compact), entry.nonce, entry.authenticationTag)
                .toBase64();
        entry.currentUser = {_account->davUser(), fileDropKey};
        metadata._fileDropEntries.append(entry);

        auto fileDrop = metadata._fileDrop;
        fileDrop.insert(entry.encryptedFilename,
                        QJsonObject{{"ciphertext", QString::fromUtf8(entry.cipherText)},
                                    {"nonce", QString::fromUtf8(entry.nonce.toBase64())},
                                    {"authenticationTag", QString::fromUtf8(entry.authenticationTag.toBase64())}});
        metadata.setFileDrop(fileDrop);
    }

private Q_SLOTS:
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

        _account->e2e()->setCertificate(cert);
        _account->e2e()->setPrivateKey(privateKey);

        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/folder", FolderMetadata::FolderType::Root));
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
            QVERIFY(metadata->addEncryptedFile(encryptedFile));
        }

        QJsonObject fakeFileDropPart;

        QJsonArray fileDropUsers;
        for (const auto &folderUser : std::as_const(metadata->_folderUsers)) {
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
            const auto cipherTextEncrypted = EncryptionHelper::gzipThenEncryptData(metadata->_binaryMetadataKeyForEncryption,
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
        _parsedMetadataWithFileDrop.reset(
            new FolderMetadata(_account, "/folder", ocsDoc.toJson(), RootEncryptedFolderInfo::makeDefault(), signature, FolderMetadata::FolderType::Root));

        QSignalSpy metadataWithFileDropSetupCompleteSpy(_parsedMetadataWithFileDrop.data(), &FolderMetadata::setupComplete);
        metadataWithFileDropSetupCompleteSpy.wait();
        QCOMPARE(metadataWithFileDropSetupCompleteSpy.count(), 1);
        QEXPECT_FAIL("", "to be fixed later or removed entirely", Abort);
        QVERIFY(_parsedMetadataWithFileDrop->isValid());

        QCOMPARE(_parsedMetadataWithFileDrop->_fileDropEntries.count(), fakeFilesFileDrop.size());
    }

    void testMoveFileDropMetadata()
    {
        QEXPECT_FAIL("", "to be fixed later or removed entirely", Abort);
        QVERIFY(_parsedMetadataWithFileDrop->isFileDropPresent());
        QVERIFY(_parsedMetadataWithFileDrop->moveFromFileDropToFiles());

        auto encryptedMetadata = _parsedMetadataWithFileDrop->encryptedMetadata();
        encryptedMetadata.replace("\"", "\\\"");
        const auto signature = _parsedMetadataWithFileDrop->metadataSignature();
        QJsonDocument ocsDoc =
            QJsonDocument::fromJson(QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(encryptedMetadata)).toUtf8());

        _parsedMetadataAfterProcessingFileDrop.reset(
            new FolderMetadata(_account, "/folder", ocsDoc.toJson(), RootEncryptedFolderInfo::makeDefault(), signature, FolderMetadata::FolderType::Root));

        QSignalSpy metadataAfterProcessingFileDropSetupCompleteSpy(_parsedMetadataAfterProcessingFileDrop.data(), &FolderMetadata::setupComplete);
        metadataAfterProcessingFileDropSetupCompleteSpy.wait();
        QCOMPARE(metadataAfterProcessingFileDropSetupCompleteSpy.count(), 1);
        QVERIFY(_parsedMetadataAfterProcessingFileDrop->isValid());

        for (const auto &fakeFileName : fakeFilesFileDrop) {
            const auto foundInEncryptedFiles = std::find_if(std::cbegin(_parsedMetadataAfterProcessingFileDrop->_files),
                                                            std::cend(_parsedMetadataAfterProcessingFileDrop->_files),
                                                            [fakeFileName](const auto &encryptedFile) {
                                                                return encryptedFile.originalFilename == fakeFileName;
                                                            });
            QVERIFY(foundInEncryptedFiles != std::cend(_parsedMetadataAfterProcessingFileDrop->_files));
        }
    }

    void testMoveFileDropPreservesFiles_data()
    {
        QTest::addColumn<QStringList>("existingNames");
        QTest::addColumn<QStringList>("fileDropNames");
        QTest::addColumn<QStringList>("expectedNames");
        QTest::addColumn<bool>("existingIsDirectory");

        QTest::newRow("unique name") << QStringList{u"other.txt"_s} << QStringList{u"test1.txt"_s} << QStringList{u"other.txt"_s, u"test1.txt"_s} << false;
        QTest::newRow("existing file") << QStringList{u"test1.txt"_s} << QStringList{u"test1.txt"_s} << QStringList{u"test1.txt"_s, u"test1 (2).txt"_s}
                                       << false;
        QTest::newRow("duplicate batch") << QStringList{} << QStringList{u"test1.txt"_s, u"test1.txt"_s} << QStringList{u"test1.txt"_s, u"test1 (2).txt"_s}
                                         << false;
        QTest::newRow("occupied suffix") << QStringList{u"test1.txt"_s, u"test1 (2).txt"_s} << QStringList{u"test1.txt"_s, u"test1.txt"_s}
                                         << QStringList{u"test1.txt"_s, u"test1 (2).txt"_s, u"test1 (3).txt"_s, u"test1 (4).txt"_s} << false;
        QTest::newRow("existing directory") << QStringList{u"test1.txt"_s} << QStringList{u"test1.txt"_s} << QStringList{u"test1.txt"_s, u"test1 (2).txt"_s}
                                            << true;
        QTest::newRow("extensionless") << QStringList{u"README"_s} << QStringList{u"README"_s} << QStringList{u"README"_s, u"README (2)"_s} << false;
        QTest::newRow("dotfile") << QStringList{u".hidden"_s} << QStringList{u".hidden"_s} << QStringList{u".hidden"_s, u".hidden (2)"_s} << false;
        QTest::newRow("multiple extensions") << QStringList{u"archive.tar.gz"_s} << QStringList{u"archive.tar.gz"_s}
                                             << QStringList{u"archive.tar.gz"_s, u"archive.tar (2).gz"_s} << false;
        QTest::newRow("already numbered") << QStringList{u"test1 (2).txt"_s} << QStringList{u"test1 (2).txt"_s}
                                          << QStringList{u"test1 (2).txt"_s, u"test1 (2) (2).txt"_s} << false;
    }

    void testMoveFileDropPreservesFiles()
    {
        QFETCH(QStringList, existingNames);
        QFETCH(QStringList, fileDropNames);
        QFETCH(QStringList, expectedNames);
        QFETCH(bool, existingIsDirectory);

        auto metadata = FolderMetadata(_account, u"/folder"_s, FolderMetadata::FolderType::Root);
        QSignalSpy setupCompleteSpy(&metadata, &FolderMetadata::setupComplete);
        QVERIFY(setupCompleteSpy.wait());
        QVERIFY(metadata.isValid());

        auto originalFiles = QVector<FolderMetadata::EncryptedFile>{};
        for (const auto &name : existingNames) {
            auto file = encryptedFile(name, u"existing-%1"_s.arg(originalFiles.size()));
            if (existingIsDirectory) {
                file.mimetype = "httpd/unix-directory";
            }
            QVERIFY(metadata.addEncryptedFile(file));
            originalFiles.append(file);
        }
        for (const auto &name : fileDropNames) {
            const auto file = encryptedFile(name, u"drop-%1"_s.arg(originalFiles.size()));
            appendFileDropEntry(metadata, file);
            originalFiles.append(file);
        }

        QVERIFY(metadata.isFileDropPresent());
        QVERIFY(metadata.moveFromFileDropToFiles());
        QVERIFY(!metadata.isFileDropPresent());
        QVERIFY(metadata._fileDrop.isEmpty());

        const auto files = metadata.files();
        QCOMPARE(files.size(), expectedNames.size());
        for (auto i = 0; i < files.size(); ++i) {
            QCOMPARE(files.at(i).originalFilename, expectedNames.at(i));
            QCOMPARE(files.at(i).encryptedFilename, originalFiles.at(i).encryptedFilename);
            QCOMPARE(files.at(i).encryptionKey, originalFiles.at(i).encryptionKey);
            QCOMPARE(files.at(i).initializationVector, originalFiles.at(i).initializationVector);
            QCOMPARE(files.at(i).authenticationTag, originalFiles.at(i).authenticationTag);
            QCOMPARE(files.at(i).mimetype, originalFiles.at(i).mimetype);
        }

        const auto serializedMetadata = QJsonDocument::fromJson(metadata.encryptedMetadata()).object();
        QVERIFY(!serializedMetadata.isEmpty());
        QVERIFY(!serializedMetadata.contains("filedrop"));
        const auto encryptedMetadata = serializedMetadata.value("metadata").toObject();
        const auto ciphertext = encryptedMetadata.value("ciphertext").toString().toUtf8().split('|').first();
        const auto nonce = QByteArray::fromBase64(encryptedMetadata.value("nonce").toString().toUtf8());
        const auto decryptedMetadata =
            EncryptionHelper::decryptThenUnGzipData(metadata.binaryMetadataKeyForEncryption(), QByteArray::fromBase64(ciphertext), nonce);
        QVERIFY(!decryptedMetadata.isEmpty());
        const auto contents = QJsonDocument::fromJson(decryptedMetadata).object();
        const auto serializedFiles = contents.value("files").toObject();
        const auto serializedFolders = contents.value("folders").toObject();
        QCOMPARE(serializedFiles.size() + serializedFolders.size(), files.size());
        for (const auto &file : files) {
            if (file.isDirectory()) {
                QCOMPARE(serializedFolders.value(file.encryptedFilename).toString(), file.originalFilename);
                continue;
            }
            const auto serializedFile = serializedFiles.value(file.encryptedFilename).toObject();
            QCOMPARE(serializedFile.value("filename").toString(), file.originalFilename);
            QCOMPARE(QByteArray::fromBase64(serializedFile.value("key").toString().toUtf8()), file.encryptionKey);
            QCOMPARE(QByteArray::fromBase64(serializedFile.value("nonce").toString().toUtf8()), file.initializationVector);
            QCOMPARE(QByteArray::fromBase64(serializedFile.value("authenticationTag").toString().toUtf8()), file.authenticationTag);
            QCOMPARE(serializedFile.value("mimetype").toString().toUtf8(), file.mimetype);
        }
    }

    void testMoveEmptyFileDrop()
    {
        auto metadata = FolderMetadata(_account, u"/folder"_s, FolderMetadata::FolderType::Root);
        QSignalSpy setupCompleteSpy(&metadata, &FolderMetadata::setupComplete);
        QVERIFY(setupCompleteSpy.wait());
        const auto file = encryptedFile(u"test1.txt"_s, u"existing"_s);
        QVERIFY(metadata.addEncryptedFile(file));

        QVERIFY(!metadata.moveFromFileDropToFiles());
        QCOMPARE(metadata.files().size(), 1);
        QCOMPARE(metadata.files().first().originalFilename, file.originalFilename);
        QCOMPARE(metadata.files().first().encryptedFilename, file.encryptedFilename);
    }

    void testInvalidFileDropIsPreserved_data()
    {
        QTest::addColumn<QString>("originalFilename");
        QTest::addColumn<QByteArray>("mimetype");
        QTest::addColumn<bool>("corruptCiphertext");
        QTest::addColumn<bool>("validEntryFirst");

        QTest::newRow("empty filename") << QString{} << QByteArray("application/octet-stream") << false << false;
        QTest::newRow("invalid filename") << u"../test1.txt"_s << QByteArray("application/octet-stream") << false << false;
        QTest::newRow("missing mimetype") << u"test1.txt"_s << QByteArray{} << false << false;
        QTest::newRow("decryption failure") << u"test1.txt"_s << QByteArray("application/octet-stream") << true << false;
        QTest::newRow("valid then invalid entry") << u"../test1.txt"_s << QByteArray("application/octet-stream") << false << true;
    }

    void testInvalidFileDropIsPreserved()
    {
        QFETCH(QString, originalFilename);
        QFETCH(QByteArray, mimetype);
        QFETCH(bool, corruptCiphertext);
        QFETCH(bool, validEntryFirst);

        auto metadata = FolderMetadata(_account, u"/folder"_s, FolderMetadata::FolderType::Root);
        QSignalSpy setupCompleteSpy(&metadata, &FolderMetadata::setupComplete);
        QVERIFY(setupCompleteSpy.wait());
        const auto existing = encryptedFile(u"test1.txt"_s, u"existing"_s);
        QVERIFY(metadata.addEncryptedFile(existing));
        if (validEntryFirst) {
            appendFileDropEntry(metadata, encryptedFile(u"test1.txt"_s, u"valid"_s));
        }
        auto dropped = encryptedFile(originalFilename, u"dropped"_s);
        dropped.mimetype = mimetype;
        appendFileDropEntry(metadata, dropped);
        if (corruptCiphertext) {
            metadata._fileDropEntries.first().cipherText = "invalid ciphertext";
        }
        const auto originalFileDrop = metadata._fileDrop;
        const auto originalFileDropEntryCount = metadata._fileDropEntries.size();

        for (auto attempt = 0; attempt < 2; ++attempt) {
            QVERIFY(!metadata.moveFromFileDropToFiles());
            QVERIFY(metadata.isFileDropPresent());
            QCOMPARE(metadata._fileDrop, originalFileDrop);
            QCOMPARE(metadata._fileDropEntries.size(), originalFileDropEntryCount);
            QCOMPARE(metadata.files().size(), 1);
            QCOMPARE(metadata.files().first().originalFilename, existing.originalFilename);
            QCOMPARE(metadata.files().first().encryptedFilename, existing.encryptedFilename);
            QCOMPARE(metadata.files().first().encryptionKey, existing.encryptionKey);
            QCOMPARE(metadata.files().first().initializationVector, existing.initializationVector);
            QCOMPARE(metadata.files().first().authenticationTag, existing.authenticationTag);
            QCOMPARE(metadata.files().first().mimetype, existing.mimetype);
        }
    }
};

QTEST_GUILESS_MAIN(TestSecureFileDrop)
#include "testsecurefiledrop.moc"
