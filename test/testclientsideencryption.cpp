/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>

#include "clientsideencryption.h"

using namespace OCC;

class TestClientSideEncryption : public QObject
{
    Q_OBJECT

    QByteArray convertToOldStorageFormat(const QByteArray &data)
    {
        return data.split('|').join("fA==");
    }

private slots:
    void shouldEncryptPrivateKeys()
    {
        // GIVEN
        const auto encryptionKey = QByteArrayLiteral("foo");
        const auto privateKey = QByteArrayLiteral("bar");
        const auto originalSalt = QByteArrayLiteral("baz");

        // WHEN
        const auto cipher = EncryptionHelper::encryptPrivateKey(encryptionKey, privateKey, originalSalt);

        // THEN
        const auto parts = cipher.split('|');
        QCOMPARE(parts.size(), 3);

        const auto encryptedKey = QByteArray::fromBase64(parts[0]);
        const auto iv = QByteArray::fromBase64(parts[1]);
        const auto salt = QByteArray::fromBase64(parts[2]);

        // We're not here to check the merits of the encryption but at least make sure it's been
        // somewhat ciphered
        QVERIFY(!encryptedKey.isEmpty());
        QVERIFY(encryptedKey != privateKey);

        QVERIFY(!iv.isEmpty());
        QCOMPARE(salt, originalSalt);
    }

    void shouldDecryptPrivateKeys()
    {
        // GIVEN
        const auto encryptionKey = QByteArrayLiteral("foo");
        const auto originalPrivateKey = QByteArrayLiteral("bar");
        const auto originalSalt = QByteArrayLiteral("baz");
        const auto cipher = EncryptionHelper::encryptPrivateKey(encryptionKey, originalPrivateKey, originalSalt);

        // WHEN
        const auto privateKey = EncryptionHelper::decryptPrivateKey(encryptionKey, cipher);
        const auto salt = EncryptionHelper::extractPrivateKeySalt(cipher);

        // THEN
        QCOMPARE(privateKey, originalPrivateKey);
        QCOMPARE(salt, originalSalt);
    }

    void shouldDecryptPrivateKeysInOldStorageFormat()
    {
        // GIVEN
        const auto encryptionKey = QByteArrayLiteral("foo");
        const auto originalPrivateKey = QByteArrayLiteral("bar");
        const auto originalSalt = QByteArrayLiteral("baz");
        const auto cipher = convertToOldStorageFormat(EncryptionHelper::encryptPrivateKey(encryptionKey, originalPrivateKey, originalSalt));

        // WHEN
        const auto privateKey = EncryptionHelper::decryptPrivateKey(encryptionKey, cipher);
        const auto salt = EncryptionHelper::extractPrivateKeySalt(cipher);

        // THEN
        QCOMPARE(privateKey, originalPrivateKey);
        QCOMPARE(salt, originalSalt);
    }

    void shouldSymmetricEncryptStrings()
    {
        // GIVEN
        const auto encryptionKey = QByteArrayLiteral("foo");
        const auto data = QByteArrayLiteral("bar");

        // WHEN
        const auto cipher = EncryptionHelper::encryptStringSymmetric(encryptionKey, data);

        // THEN
        const auto parts = cipher.split('|');
        QCOMPARE(parts.size(), 2);

        const auto encryptedData = QByteArray::fromBase64(parts[0]);
        const auto iv = QByteArray::fromBase64(parts[1]);

        // We're not here to check the merits of the encryption but at least make sure it's been
        // somewhat ciphered
        QVERIFY(!encryptedData.isEmpty());
        QVERIFY(encryptedData != data);

        QVERIFY(!iv.isEmpty());
    }

    void shouldSymmetricDecryptStrings()
    {
        // GIVEN
        const auto encryptionKey = QByteArrayLiteral("foo");
        const auto originalData = QByteArrayLiteral("bar");
        const auto cipher = EncryptionHelper::encryptStringSymmetric(encryptionKey, originalData);

        // WHEN
        const auto data = EncryptionHelper::decryptStringSymmetric(encryptionKey, cipher);

        // THEN
        QCOMPARE(data, originalData);
    }

    void shouldSymmetricDecryptStringsInOldStorageFormat()
    {
        // GIVEN
        const auto encryptionKey = QByteArrayLiteral("foo");
        const auto originalData = QByteArrayLiteral("bar");
        const auto cipher = convertToOldStorageFormat(EncryptionHelper::encryptStringSymmetric(encryptionKey, originalData));

        // WHEN
        const auto data = EncryptionHelper::decryptStringSymmetric(encryptionKey, cipher);

        // THEN
        QCOMPARE(data, originalData);
    }
};

QTEST_APPLESS_MAIN(TestClientSideEncryption)
#include "testclientsideencryption.moc"
