/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "syncenginetestutils.h"
#include "clientsideencryption.h"
#include "foldermetadata.h"
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

private:
    // decrypts the "metadata.ciphertext" blob of a freshly generated FolderMetadata's
    // JSON and returns the plaintext inner object (counter/files/folders/keyChecksums)
    static QJsonObject decryptCipherTextObject(FolderMetadata *metadata, const QByteArray &encryptedMetadata)
    {
        const auto metaDataDoc = QJsonDocument::fromJson(encryptedMetadata);
        const auto metadataObj = metaDataDoc["metadata"].toObject();
        const auto cipherTextEncrypted = metadataObj["ciphertext"].toString().toLocal8Bit();
        // for compatibility, the format is "cipheredpart|initializationVector", so we need to extract the "cipheredpart"
        const auto cipherTextPartExtracted = cipherTextEncrypted.split('|').at(0);
        const auto nonce = QByteArray::fromBase64(metadataObj["nonce"].toString().toLocal8Bit());
        const auto cipherTextDecrypted =
            EncryptionHelper::decryptThenUnGzipData(metadata->binaryMetadataKeyForEncryption(), QByteArray::fromBase64(cipherTextPartExtracted), nonce);
        return QJsonDocument::fromJson(cipherTextDecrypted).object();
    }

    static QVector<QByteArray> certificatePemsFromMetadata(const QJsonDocument &metaDataDoc)
    {
        QVector<QByteArray> certificatePems;
        const auto folderUsers = metaDataDoc["users"].toArray();
        for (auto it = folderUsers.constBegin(); it != folderUsers.constEnd(); ++it) {
            certificatePems.push_back(it->toObject().value("certificate").toString().toUtf8());
        }
        return certificatePems;
    }

private Q_SLOTS:
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
        QVERIFY(metadata->addEncryptedFile(encryptedFile));

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
            RootEncryptedFolderInfo::makeDefault(), signature, FolderMetadata::FolderType::Root));
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
                                                                           emptySignature,
                                                                           FolderMetadata::FolderType::Root));

        QSignalSpy metadataSetupExistingCompleteSpy(metadataFromJson.data(), &FolderMetadata::setupComplete);
        metadataSetupExistingCompleteSpy.wait();
        QCOMPARE(metadataSetupExistingCompleteSpy.count(), 1);

        QVERIFY(metadataFromJson->metadataSignature().isEmpty());
        QVERIFY(metadataFromJson->binaryMetadataKeyForDecryption().isEmpty());
        QVERIFY(!metadataFromJson->isValid());
    }

    // Guards the "broken empty signature" hypothesis for
    // https://github.com/nextcloud/desktop/issues/10086: encrypting a
    // brand-new, still-empty folder must produce metadata whose signature
    // is not just non-empty but actually verifies against the folder's
    // certificates, and whose keyChecksums entry is populated in the
    // ciphertext that gets shipped to the server/other clients (e.g. iOS).
    void testNewEmptyRootFolderMetadataIsCorrect()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());
        QVERIFY(metadata->files().isEmpty());

        const auto encryptedMetadata = metadata->encryptedMetadata();
        QVERIFY(!encryptedMetadata.isEmpty());

        const auto signature = metadata->metadataSignature();
        QVERIFY(!signature.isEmpty());
        QVERIFY(!metadata->keyChecksums().isEmpty());

        const auto metaDataDoc = QJsonDocument::fromJson(encryptedMetadata);
        const auto folderUsers = metaDataDoc["users"].toArray();
        QVERIFY(!folderUsers.isEmpty());

        // a non-empty but bogus/mismatched signature must not be able to pass this check
        QVector<QByteArray> certificatePems;
        for (auto it = folderUsers.constBegin(); it != folderUsers.constEnd(); ++it) {
            certificatePems.push_back(it->toObject().value("certificate").toString().toUtf8());
        }

        const auto metadataForSignature = FolderMetadata::prepareMetadataForSignature(metaDataDoc);
        QVERIFY(_account->e2e()->verifySignatureCryptographicMessageSyntax(
            QByteArray::fromBase64(signature), metadataForSignature.toBase64(), certificatePems));

        // keyChecksums as it actually ends up inside the encrypted ciphertext shipped to the server/other clients
        const auto metadataObj = metaDataDoc.object()["metadata"].toObject();
        const auto cipherTextEncrypted = metadataObj["ciphertext"].toString().toLocal8Bit();
        // for compatibility, the format is "cipheredpart|initializationVector", so we need to extract the "cipheredpart"
        const auto cipherTextPartExtracted = cipherTextEncrypted.split('|').at(0);
        const auto nonce = QByteArray::fromBase64(metadataObj["nonce"].toString().toLocal8Bit());
        const auto cipherTextDecrypted =
            EncryptionHelper::decryptThenUnGzipData(metadata->binaryMetadataKeyForEncryption(), QByteArray::fromBase64(cipherTextPartExtracted), nonce);
        QVERIFY(!cipherTextDecrypted.isEmpty());

        const auto cipherTextObject = QJsonDocument::fromJson(cipherTextDecrypted).object();
        QVERIFY(cipherTextObject["files"].toObject().isEmpty());
        QVERIFY(cipherTextObject["folders"].toObject().isEmpty());
        QVERIFY(!cipherTextObject["keyChecksums"].toArray().isEmpty());
    }

    // Per the server's lib/LockManager.php::lockFile(): "if ($storedCounter >= $e2eCounter)
    // throw NotPermittedException", where getCounter() defaults to 0 for a folder that has
    // no metadata yet. The very first counter a client sends therefore must be > 0, i.e. 1.
    void testNewFolderCounterIsOneOnFirstUpload()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        QCOMPARE(metadata->newCounter(), quint64(1));

        const auto encryptedMetadata = metadata->encryptedMetadata();
        QVERIFY(!encryptedMetadata.isEmpty());

        const auto cipherTextObject = decryptCipherTextObject(metadata.data(), encryptedMetadata);
        QCOMPARE(cipherTextObject["counter"].toVariant().toULongLong(), 1ULL);
    }

    // Reproduces the counter-reuse hypothesis for https://github.com/nextcloud/desktop/issues/10086:
    // _counter (foldermetadata.h:233) is only ever refreshed by re-parsing metadata that came back
    // from the server (foldermetadata.cpp:287); nothing on the upload-success path
    // (EncryptedFolderMetadataHandler::slotUploadMetadataSuccess) advances it. So a second call to
    // encryptedMetadata() on the same in-memory instance -- e.g. two local edits before a re-fetch --
    // resends the exact same counter, which the server's strict "storedCounter >= e2eCounter" check
    // (lib/LockManager.php) would reject outright.
    void testRepeatedEncryptedMetadataCallsAdvanceCounter()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto firstMetadata = metadata->encryptedMetadata();
        QVERIFY(!firstMetadata.isEmpty());
        const auto firstCounter = decryptCipherTextObject(metadata.data(), firstMetadata)["counter"].toVariant().toULongLong();

        FolderMetadata::EncryptedFile encryptedFile;
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = "fakefile.txt";
        encryptedFile.mimetype = "application/octet-stream";
        encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
        QVERIFY(metadata->addEncryptedFile(encryptedFile));

        const auto secondMetadata = metadata->encryptedMetadata();
        QVERIFY(!secondMetadata.isEmpty());
        const auto secondCounter = decryptCipherTextObject(metadata.data(), secondMetadata)["counter"].toVariant().toULongLong();

        QEXPECT_FAIL("", "counter is only refreshed by re-parsing server metadata (foldermetadata.cpp:287); a second "
                          "write from the same in-memory instance resends the same counter and would be rejected by "
                          "the server's LockManager (nextcloud/desktop#10086)", Continue);
        QVERIFY(secondCounter > firstCounter);
    }

    // The correct counterpart to testRepeatedEncryptedMetadataCallsAdvanceCounter: once _counter is
    // refreshed the way a real server round-trip would refresh it (foldermetadata.cpp:287), the next
    // generated counter correctly picks up where the previous upload left off.
    void testCounterAdvancesAfterReparsingServerMetadata()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto firstMetadata = metadata->encryptedMetadata();
        QVERIFY(!firstMetadata.isEmpty());
        const auto firstCounter = decryptCipherTextObject(metadata.data(), firstMetadata)["counter"].toVariant().toULongLong();
        QCOMPARE(firstCounter, 1ULL);

        // simulate what setupExistingMetadata() does when re-parsing metadata fetched back from the server
        metadata->_counter = firstCounter;
        QCOMPARE(metadata->newCounter(), firstCounter + 1);

        FolderMetadata::EncryptedFile encryptedFile;
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = "fakefile.txt";
        encryptedFile.mimetype = "application/octet-stream";
        encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
        QVERIFY(metadata->addEncryptedFile(encryptedFile));

        const auto secondMetadata = metadata->encryptedMetadata();
        QVERIFY(!secondMetadata.isEmpty());
        const auto secondCounter = decryptCipherTextObject(metadata.data(), secondMetadata)["counter"].toVariant().toULongLong();
        QCOMPARE(secondCounter, firstCounter + 1);
    }

    // Reproduces the keyChecksums hypothesis for https://github.com/nextcloud/desktop/issues/10086.
    // The RFC's client verification checklist is unconditional: "check that hash of metadata-key is
    // in keyChecksums" ... "If any of the following checks fail, the client needs to refuse further
    // sync". verifyMetadataKey() (foldermetadata.cpp:1270) instead treats an empty keyChecksums set
    // as automatically valid -- the line above it is the original author's own comment: "_keyChecksums
    // should not be empty, fix this by taking a proper _keyChecksums from the topLevelFolder".
    void testEmptyKeyChecksumsMustNotBeTreatedAsValid()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        // exercise verifyMetadataKey() as it behaves for v2.0+ metadata; below Version2_0 it
        // unconditionally returns true (foldermetadata.cpp:1262), which is not what's under test here
        metadata->_existingMetadataVersion = FolderMetadata::MetadataVersion::Version2_0;

        // sanity: the folder's own, legitimately-generated metadata key does verify
        QVERIFY(metadata->verifyMetadataKey(metadata->binaryMetadataKeyForEncryption()));

        // simulate a folder whose local keyChecksums history is empty (e.g. a legacy folder that
        // never had one populated)
        metadata->_keyChecksums.clear();

        const auto foreignMetadataKey =
            EncryptionHelper::generateRandom(static_cast<int>(metadata->binaryMetadataKeyForEncryption().size()));
        QEXPECT_FAIL("", "verifyMetadataKey() treats an empty keyChecksums set as valid (foldermetadata.cpp:1270), "
                          "contradicting the RFC's unconditional check; flagged by the code's own comment as unfixed "
                          "(nextcloud/desktop#10086)", Continue);
        QVERIFY(!metadata->verifyMetadataKey(foreignMetadataKey));
    }

    // The signature must protect the encrypted payload that actually carries the counter,
    // keyChecksums and files/folders: any tampering with "metadata.ciphertext" after signing
    // must be caught by a re-verification, even though the tamperer can't see the plaintext.
    void testSignatureDetectsTamperedCipherText()
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

        const auto metaDataDoc = QJsonDocument::fromJson(encryptedMetadata);
        const auto certificatePems = certificatePemsFromMetadata(metaDataDoc);

        // sanity: the original, untouched metadata verifies
        QVERIFY(_account->e2e()->verifySignatureCryptographicMessageSyntax(
            QByteArray::fromBase64(signature), FolderMetadata::prepareMetadataForSignature(metaDataDoc).toBase64(), certificatePems));

        auto tamperedObject = metaDataDoc.object();
        auto tamperedMetadataObject = tamperedObject["metadata"].toObject();
        auto cipherTextChars = tamperedMetadataObject["ciphertext"].toString().toUtf8();
        QVERIFY(cipherTextChars.size() > 10);
        const auto flipIndex = cipherTextChars.size() / 2;
        cipherTextChars[flipIndex] = (cipherTextChars[flipIndex] == 'A') ? 'B' : 'A';
        tamperedMetadataObject["ciphertext"] = QString::fromUtf8(cipherTextChars);
        tamperedObject["metadata"] = tamperedMetadataObject;
        const QJsonDocument tamperedDoc(tamperedObject);

        QVERIFY(!_account->e2e()->verifySignatureCryptographicMessageSyntax(
            QByteArray::fromBase64(signature), FolderMetadata::prepareMetadataForSignature(tamperedDoc).toBase64(), certificatePems));
    }

    // Per the RFC: "Create metadata JSON in a compact format - Remove the filedrop part" before
    // signing. prepareMetadataForSignature() (foldermetadata.cpp:985) does exactly that, so adding
    // or mutating "filedrop" content after the fact must NOT invalidate an existing signature.
    void testSignatureIgnoresFiledropByDesign()
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

        const auto metaDataDoc = QJsonDocument::fromJson(encryptedMetadata);
        const auto certificatePems = certificatePemsFromMetadata(metaDataDoc);

        auto withFiledropObject = metaDataDoc.object();
        withFiledropObject.insert(QStringLiteral("filedrop"),
                                   QJsonObject{{QStringLiteral("some-encrypted-name"), QJsonObject{{QStringLiteral("ciphertext"), QStringLiteral("bogus")}}}});
        const QJsonDocument withFiledropDoc(withFiledropObject);

        QVERIFY(_account->e2e()->verifySignatureCryptographicMessageSyntax(
            QByteArray::fromBase64(signature), FolderMetadata::prepareMetadataForSignature(withFiledropDoc).toBase64(), certificatePems));
    }

    // Documents a conformance gap against the RFC's mandatory verification checklist, which also
    // requires clients to "check if cert of all users is issued by the CA". verifySignatureCryptographicMessageSyntax()
    // (clientsideencryption.cpp) passes CMS_NO_SIGNER_CERT_VERIFY and never performs that check, so a
    // signature made with a certificate that fails standard chain verification still verifies here.
    void testSignatureVerificationSkipsCaIssuanceCheck()
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

        // the certificate used here (and for the software-storage E2EE certificate in production)
        // is not chain-verifiable against a trusted root
        QVERIFY(!QSslCertificate::verify({_account->e2e()->getCertificate()}).isEmpty());

        const auto metaDataDoc = QJsonDocument::fromJson(encryptedMetadata);
        const auto certificatePems = certificatePemsFromMetadata(metaDataDoc);

        QVERIFY(_account->e2e()->verifySignatureCryptographicMessageSyntax(
            QByteArray::fromBase64(signature), FolderMetadata::prepareMetadataForSignature(metaDataDoc).toBase64(), certificatePems));
    }

    // Structural conformance check on the outer (unencrypted) metadata JSON for a brand-new empty
    // folder: version tag, and the "cipheredpart|IV" shape of "metadata.ciphertext".
    void testEmptyFolderMetadataOuterStructure()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto encryptedMetadata = metadata->encryptedMetadata();
        QVERIFY(!encryptedMetadata.isEmpty());

        const auto metaDataDoc = QJsonDocument::fromJson(encryptedMetadata);
        QCOMPARE(metaDataDoc["version"].toString(), QString::number(_account->capabilities().clientSideEncryptionVersion(), 'f', 1));

        const auto metadataObj = metaDataDoc["metadata"].toObject();
        const auto cipherTextParts = metadataObj["ciphertext"].toString().split(QLatin1Char('|'));
        QCOMPARE(cipherTextParts.size(), 2);
        QVERIFY(!QByteArray::fromBase64(cipherTextParts[0].toUtf8()).isEmpty());
        QVERIFY(!QByteArray::fromBase64(cipherTextParts[1].toUtf8()).isEmpty());
        QVERIFY(!metadataObj["nonce"].toString().isEmpty());
        QVERIFY(!metadataObj["authenticationTag"].toString().isEmpty());
    }

    // AES-GCM security invariant: reusing a nonce/IV with the same key is catastrophic. Two
    // independent calls to encryptedMetadata() for the same (still-empty) folder must use a fresh
    // nonce each time, even though the plaintext content hasn't changed.
    void testRepeatedEncryptedMetadataUsesFreshNonce()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto firstMetadataObj = QJsonDocument::fromJson(metadata->encryptedMetadata())["metadata"].toObject();
        const auto secondMetadataObj = QJsonDocument::fromJson(metadata->encryptedMetadata())["metadata"].toObject();

        QVERIFY(!firstMetadataObj["nonce"].toString().isEmpty());
        QVERIFY(!secondMetadataObj["nonce"].toString().isEmpty());
        QVERIFY(firstMetadataObj["nonce"].toString() != secondMetadataObj["nonce"].toString());
        QVERIFY(firstMetadataObj["ciphertext"].toString() != secondMetadataObj["ciphertext"].toString());
    }

    void testOriginalFilenameValidation_data()
    {
        QTest::addColumn<QString>("originalFilename");
        QTest::addColumn<bool>("isValid");

        QTest::newRow("plain file") << QStringLiteral("document.txt") << true;
        QTest::newRow("plain folder") << QStringLiteral("Documents") << true;
        QTest::newRow("hidden file") << QStringLiteral(".hidden") << true;
        QTest::newRow("empty") << QString() << false;
        QTest::newRow("current directory") << QStringLiteral(".") << false;
        QTest::newRow("parent directory") << QStringLiteral("..") << false;
        QTest::newRow("relative traversal") << QStringLiteral("../../poc_dir") << false;
        QTest::newRow("embedded slash") << QStringLiteral("folder/file.txt") << false;
        QTest::newRow("absolute path") << QStringLiteral("/tmp/poc") << false;
        QTest::newRow("backslash") << QStringLiteral("folder\\file.txt") << false;
        QTest::newRow("null byte") << QStringLiteral("file") + QChar(0) + QStringLiteral("name") << false;
    }

    void testOriginalFilenameValidation()
    {
        QFETCH(QString, originalFilename);
        QFETCH(bool, isValid);

        QCOMPARE(FolderMetadata::isOriginalFilenameValid(originalFilename), isValid);
    }

    void testParseEncryptedFileFromJsonRejectsUnsafeOriginalFilename_data()
    {
        testOriginalFilenameValidation_data();
    }

    void testParseEncryptedFileFromJsonRejectsUnsafeOriginalFilename()
    {
        QFETCH(QString, originalFilename);
        QFETCH(bool, isValid);

        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto fileJson = QJsonObject{
            {QStringLiteral("filename"), originalFilename},
            {QStringLiteral("key"), QString::fromUtf8(QByteArrayLiteral("key").toBase64())},
            {QStringLiteral("mimetype"), QStringLiteral("application/octet-stream")},
            {QStringLiteral("nonce"), QString::fromUtf8(QByteArrayLiteral("nonce").toBase64())},
            {QStringLiteral("authenticationTag"), QString::fromUtf8(QByteArrayLiteral("tag").toBase64())},
        };

        const auto parsedEncryptedFile = metadata->parseEncryptedFileFromJson(QStringLiteral("encrypted-name"), fileJson);
        QCOMPARE(!parsedEncryptedFile.originalFilename.isEmpty(), isValid);
        if (isValid) {
            QCOMPARE(parsedEncryptedFile.originalFilename, originalFilename);
        }
    }

    void testAddEncryptedFileRejectsUnsafeOriginalFilename()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        FolderMetadata::EncryptedFile encryptedFile;
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = QStringLiteral("folder\\file.txt");
        encryptedFile.mimetype = "application/octet-stream";
        encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);

        QVERIFY(!metadata->addEncryptedFile(encryptedFile));
        QVERIFY(metadata->files().isEmpty());
    }

    void testSetupExistingMetadataRejectsUnsafeOriginalFilename()
    {
        QScopedPointer<FolderMetadata> metadata(new FolderMetadata(_account, "/", FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupCompleteSpy(metadata.data(), &FolderMetadata::setupComplete);
        metadataSetupCompleteSpy.wait();
        QCOMPARE(metadataSetupCompleteSpy.count(), 1);
        QVERIFY(metadata->isValid());

        const auto initialEncryptedMetadata = metadata->encryptedMetadata();
        QVERIFY(!initialEncryptedMetadata.isEmpty());

        const auto encryptedFilename = QStringLiteral("encrypted-name");
        const auto fileJson = QJsonObject{
            {QStringLiteral("filename"), QStringLiteral("folder\\file.txt")},
            {QStringLiteral("key"), QString::fromUtf8(EncryptionHelper::generateRandom(16).toBase64())},
            {QStringLiteral("mimetype"), QStringLiteral("application/octet-stream")},
            {QStringLiteral("nonce"), QString::fromUtf8(EncryptionHelper::generateRandom(16).toBase64())},
            {QStringLiteral("authenticationTag"), QString::fromUtf8(EncryptionHelper::generateRandom(16).toBase64())},
        };

        QJsonArray keyChecksums;
        for (auto it = metadata->_keyChecksums.constBegin(), end = metadata->_keyChecksums.constEnd(); it != end; ++it) {
            keyChecksums.push_back(QJsonValue::fromVariant(*it));
        }

        const auto cipherText = QJsonObject{
            {QStringLiteral("counter"), QJsonValue::fromVariant(metadata->newCounter())},
            {QStringLiteral("files"), QJsonObject{{encryptedFilename, fileJson}}},
            {QStringLiteral("folders"), QJsonObject{}},
            {QStringLiteral("keyChecksums"), keyChecksums},
        };
        const auto cipherTextDoc = QJsonDocument(cipherText);

        QByteArray authenticationTag;
        const auto nonce = EncryptionHelper::generateRandom(16);
        const auto encryptedCipherText = EncryptionHelper::gzipThenEncryptData(metadata->binaryMetadataKeyForEncryption(),
                                                                               cipherTextDoc.toJson(QJsonDocument::Compact),
                                                                               nonce,
                                                                               authenticationTag).toBase64()
            + QByteArrayLiteral("|") + nonce.toBase64();

        auto metadataDoc = QJsonDocument::fromJson(initialEncryptedMetadata);
        auto metaObject = metadataDoc.object();
        auto metadataObject = metaObject[QStringLiteral("metadata")].toObject();
        metadataObject.insert(QStringLiteral("ciphertext"), QString::fromUtf8(encryptedCipherText));
        metadataObject.insert(QStringLiteral("nonce"), QString::fromUtf8(nonce.toBase64()));
        metadataObject.insert(QStringLiteral("authenticationTag"), QString::fromUtf8(authenticationTag.toBase64()));
        metaObject.insert(QStringLiteral("metadata"), metadataObject);
        metadataDoc.setObject(metaObject);

        const auto signature = _account->e2e()->generateSignatureCryptographicMessageSyntax(FolderMetadata::prepareMetadataForSignature(metadataDoc).toBase64()).toBase64();
        QVERIFY(!signature.isEmpty());

        const auto ocsDoc = QJsonDocument(QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{
                {QStringLiteral("data"), QJsonObject{
                    {QStringLiteral("meta-data"), QString::fromUtf8(metadataDoc.toJson(QJsonDocument::Compact))},
                }},
            }},
        });

        QScopedPointer<FolderMetadata> metadataFromJson(new FolderMetadata(_account,
                                                                           "/",
                                                                           ocsDoc.toJson(),
                                                                           RootEncryptedFolderInfo::makeDefault(),
                                                                           signature,
                                                                           FolderMetadata::FolderType::Root));
        QSignalSpy metadataSetupExistingCompleteSpy(metadataFromJson.data(), &FolderMetadata::setupComplete);
        metadataSetupExistingCompleteSpy.wait();
        QCOMPARE(metadataSetupExistingCompleteSpy.count(), 1);
        QVERIFY(!metadataFromJson->isValid());
        QVERIFY(metadataFromJson->files().isEmpty());
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
        QVERIFY(metadata->addEncryptedFile(encryptedFile));

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

        QScopedPointer<FolderMetadata> metadataFromJsonForSecondUser(new FolderMetadata(_secondAccount, "/", ocsDoc.toJson(), RootEncryptedFolderInfo::makeDefault(), signature, FolderMetadata::FolderType::Root));
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
        QVERIFY(metadataFromJsonForSecondUser->addEncryptedFile(encryptedFile));

        auto encryptedMetadataFromSecondUser = metadataFromJsonForSecondUser->encryptedMetadata();
        encryptedMetadataFromSecondUser.replace("\"", "\\\"");

        const auto signatureAfterSecondUserModification = metadataFromJsonForSecondUser->metadataSignature();
        QVERIFY(!signatureAfterSecondUserModification.isEmpty());

        QJsonDocument ocsDocFromSecondUser = QJsonDocument::fromJson(
            QStringLiteral("{\"ocs\": {\"data\": {\"meta-data\": \"%1\"}}}").arg(QString::fromUtf8(encryptedMetadataFromSecondUser)).toUtf8());

        QScopedPointer<FolderMetadata> metadataFromJsonForFirstUserToCheckCrossSharing(new FolderMetadata(_account,
                                                                                                          "/",
                                                                                                          ocsDocFromSecondUser.toJson(),
                                                                                                          RootEncryptedFolderInfo::makeDefault(),
                                                                                                          signatureAfterSecondUserModification,
                                                                                                          FolderMetadata::FolderType::Root));
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
};

QTEST_GUILESS_MAIN(TestClientSideEncryptionV2)
#include "testclientsideencryptionv2.moc"
