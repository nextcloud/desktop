#include "clientsideencryption.h"
#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/engine.h>

#include <map>

#include <cstdio>

#include <QDebug>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QXmlStreamNamespaceDeclaration>
#include <QStack>

#include "wordlist.h"

QDebug operator<<(QDebug out, const std::string& str)
{
    out << QString::fromStdString(str);
    return out;
}

namespace OCC
{

Q_LOGGING_CATEGORY(lcCse, "sync.clientsideencryption", QtInfoMsg)
Q_LOGGING_CATEGORY(lcSignPublicKeyApiJob, "sync.networkjob.sendcsr", QtInfoMsg)
Q_LOGGING_CATEGORY(lcStorePrivateKeyApiJob, "sync.networkjob.storeprivatekey", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseJob, "sync.networkjob.clientsideencrypt", QtInfoMsg)

QString baseUrl(){
    return QStringLiteral("ocs/v2.php/apps/end_to_end_encryption/api/v1/");
}

QString baseDirectory() {
    return QDir::homePath() + QStringLiteral("/.nextcloud-keys/");
}

namespace {
    void handleErrors(void)
    {
        ERR_print_errors_fp(stdout); // This line is not printing anything.
        fflush(stdout);
    }

    int encrypt(unsigned char *plaintext,
                int plaintext_len,
                unsigned char *key,
                unsigned char *iv,
                unsigned char *ciphertext,
                unsigned char *tag)
    {
        EVP_CIPHER_CTX *ctx;
        int len;
        int ciphertext_len;

        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new())) {
			qCInfo(lcCse()) << "Error creating the Cipher.";
            handleErrors();
        }

        /* Initialise the encryption operation. */
        if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
			qCInfo(lcCse()) << "Error initializing the context with aes_256";
            handleErrors();
        }

        // We don't do padding
        EVP_CIPHER_CTX_set_padding(ctx, 0);

        /* Set IV length to 16 bytes */
        if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL)) {
            qCInfo(lcCse()) << "Error setting the iv length to 16 bytes. ";
            handleErrors();
        }

        /* Initialise key and IV */
        if(1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
            qCInfo(lcCse()) << "Error setting key and iv encryption";
            handleErrors();
        }

        /* Provide the message to be encrypted, and obtain the encrypted output.
        * EVP_EncryptUpdate can be called multiple times if necessary
        */
        if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
			qCInfo(lcCse()) << "Error encrypting the cipher ext"; // Current error is here.
            handleErrors();
        }
        ciphertext_len = len;

        /* Finalise the encryption. Normally ciphertext bytes may be written at
        * this stage, but this does not occur in GCM mode
        */
        if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
			qCInfo(lcCse()) << "Error finalizing the encryption";
            handleErrors();
        }
        ciphertext_len += len;

        /* Get the tag */
        if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
			qCInfo(lcCse()) << "Error Retrieving the tag";
            handleErrors();
        }

        /* Add tag to cypher text to be compatible with the Android implementation */
        memcpy(ciphertext + ciphertext_len, tag, 16);
        ciphertext_len += 16;

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        return ciphertext_len;
    }

    int decrypt(unsigned char *ciphertext,
                int ciphertext_len,
                unsigned char *tag,
                unsigned char *key,
                unsigned char *iv,
                unsigned char *plaintext)
    {
        EVP_CIPHER_CTX *ctx;
        int len;
        int plaintext_len;
        int ret;

        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new())) {
			qCInfo(lcCse()) << "Error Initializing the decrypt context";
            handleErrors();
        }

        /* Initialise the decryption operation. */
        if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
			qCInfo(lcCse()) << "Error initializing the decryption context";
            handleErrors();
        }

        /* Set IV length to 16 bytes */
        if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL)) {
			qCInfo(lcCse()) << "Error seting th iv length for the decrypt context";
            handleErrors();
        }

        /* Initialise key and IV */
        if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) {
			qCInfo(lcCse()) << "Error setting the key and iv for decryption";
            handleErrors();
        }

       /* Provide the message to be decrypted, and obtain the plaintext output.
        * EVP_DecryptUpdate can be called multiple times if necessary
        *
        * Do not try to decrypt the last 16 bytes. The tag is appended by Android.
        * So we ignore the last 16 bytes.
        */
        if(!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len - 16)) {
			qCInfo(lcCse()) << "Error decrypting the text";
            handleErrors();
        }
        plaintext_len = len;

        /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
        if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)) {
			qCInfo(lcCse()) << "Error setting the tag on the decrupt context";
            handleErrors();
        }

        /* Finalise the decryption. A positive return value indicates success,
        * anything else is a failure - the plaintext is not trustworthy.
        */
        ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        if(ret > 0)
        {
            /* Success */
            plaintext_len += len;
            return plaintext_len;
        }
        else
        {
			qCInfo(lcCse()) << "Error finalizing the decrypt";
            /* Verify failed */
            return -1;
        }
    }
}

class EncryptionHelper {
public:
    static QVector<unsigned char> generatePassword(QString wordlist, QVector<unsigned char> &salt);
};

QVector<unsigned char> EncryptionHelper::generatePassword(QString wordlist, QVector<unsigned char> &salt) {
    qCInfo(lcCse()) << "Start encryption key generation!";

    // TODO generate salt
    const unsigned char *_salt = (unsigned char *)"$4$YmBjm3hk$Qb74D5IUYwghUmzsMqeNFx5z0/8$";
    const int saltLen = 40;

    for (int i = 0; i < saltLen; i++) {
        salt.append(_salt[i]);
    }

    const int iterationCount = 1024;
    const int keyStrength = 256;
    const int keyLength = keyStrength/8;

    unsigned char secretKey[keyLength];

    int ret = PKCS5_PBKDF2_HMAC_SHA1(
        wordlist.toLocal8Bit().constData(),     // const char *password,
        wordlist.size(),                        // int password length,
        salt.constData(),                       // const unsigned char *salt,
        salt.size(),                            // int saltlen,
        iterationCount,                         // int iterations,
        keyLength,                              // int keylen,
        secretKey                               // unsigned char *out
    );

    if (ret != 1) {
        qCInfo(lcCse()) << "Failed to generate encryption key";
        // Error out?
    }

    qCInfo(lcCse()) << "Encryption key generated!";

    QVector<unsigned char> result;
    for (int i = 0; i < keyLength; i++) {
        result.append(secretKey[i]);
    }

    return result;
}

ClientSideEncryption::ClientSideEncryption()
{
}

void ClientSideEncryption::setAccount(AccountPtr account)
{
    _account = account;
}

void ClientSideEncryption::initialize()
{
    qCInfo(lcCse()) << "Initializing";
    if (!_account->capabilities().clientSideEncryptionAvaliable()) {
        qCInfo(lcCse()) << "No Client side encryption avaliable on server.";
        emit initializationFinished();
    }

    if (hasPrivateKey() && hasPublicKey()) {
        qCInfo(lcCse()) << "Public and private keys already downloaded";
        emit initializationFinished();
    }

    getPublicKeyFromServer();
}

QString publicKeyPath(AccountPtr account)
{
    return baseDirectory() + account->displayName() + ".pub";
}

QString privateKeyPath(AccountPtr account)
{
    return baseDirectory() + account->displayName() + ".rsa";
}

bool ClientSideEncryption::hasPrivateKey() const
{
    return QFileInfo(privateKeyPath(_account)).exists();
}

bool ClientSideEncryption::hasPublicKey() const
{
    return QFileInfo(publicKeyPath(_account)).exists();
}

void ClientSideEncryption::generateKeyPair()
{
    // AES/GCM/NoPadding,
    // metadataKeys with RSA/ECB/OAEPWithSHA-256AndMGF1Padding
    qCInfo(lcCse()) << "No public key, generating a pair.";
    const int rsaKeyLen = 2048;

    EVP_PKEY *localKeyPair = nullptr;

    // Init RSA
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);

    if(EVP_PKEY_keygen_init(ctx) <= 0) {
        qCInfo(lcCse()) << "Couldn't initialize the key generator";
        return;
    }

    if(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, rsaKeyLen) <= 0) {
        qCInfo(lcCse()) << "Couldn't initialize the key generator bits";
        return;
    }

    if(EVP_PKEY_keygen(ctx, &localKeyPair) <= 0) {
        qCInfo(lcCse()) << "Could not generate the key";
        return;
    }
    EVP_PKEY_CTX_free(ctx);
    qCInfo(lcCse()) << "Key correctly generated";
    qCInfo(lcCse()) << "Storing keys locally";

    QDir dir;
    if (!dir.mkpath(baseDirectory())) {
        qCInfo(lcCse()) << "Could not create the folder for the keys.";
        return;
    }

    auto privKeyPath = privateKeyPath(_account).toLocal8Bit();
    auto pubKeyPath = publicKeyPath(_account).toLocal8Bit();
    FILE *privKeyFile = fopen(privKeyPath.constData(), "w");
    FILE *pubKeyFile = fopen(pubKeyPath.constData(), "w");

    qCInfo(lcCse()) << "Private key filename" << privKeyPath;
    qCInfo(lcCse()) << "Public key filename" << pubKeyPath;

    //TODO: Verify if the key needs to be stored with a Cipher and Pass.
    if (!PEM_write_PrivateKey(privKeyFile, localKeyPair, NULL, NULL, 0, 0, NULL)) {
        qCInfo(lcCse()) << "Could not write the private key to a file.";
        return;
    }

    if (!PEM_write_PUBKEY(pubKeyFile, localKeyPair)) {
        qCInfo(lcCse()) << "Could not write the public key to a file.";
        return;
    }

    fclose(privKeyFile);
    fclose(pubKeyFile);

    generateCSR(localKeyPair);

    //TODO: Send to server.
    qCInfo(lcCse()) << "Keys generated correctly, sending to server.";
}

void ClientSideEncryption::generateCSR(EVP_PKEY *keyPair)
{
    // OpenSSL expects const char.
    auto cnArray = _account->davUser().toLocal8Bit();
    qCInfo(lcCse()) << "Getting the following array for the account Id" << cnArray;

    auto certParams = std::map<const char *, const char*>{
      {"C", "DE"},
      {"ST", "Baden-Wuerttemberg"},
      {"L", "Stuttgart"},
      {"O","Nextcloud"},
      {"CN", cnArray.constData()}
    };

    int             ret = 0;
    int             nVersion = 1;

    X509_REQ *x509_req = nullptr;
    auto out = BIO_new(BIO_s_mem());
    QByteArray output;
    char data[80];
    SignPublicKeyApiJob *job = nullptr;

    // 2. set version of x509 req
    x509_req = X509_REQ_new();
    ret = X509_REQ_set_version(x509_req, nVersion);

    // 3. set subject of x509 req
    auto x509_name = X509_REQ_get_subject_name(x509_req);

    using ucharp = const unsigned char *;
    for(const auto& v : certParams) {
        ret = X509_NAME_add_entry_by_txt(x509_name, v.first,  MBSTRING_ASC, (ucharp) v.second, -1, -1, 0);
        if (ret != 1) {
            qCInfo(lcCse()) << "Error Generating the Certificate while adding" << v.first << v.second;
            goto free_all;
        }
    }

    ret = X509_REQ_set_pubkey(x509_req, keyPair);
    if (ret != 1){
        qCInfo(lcCse()) << "Error setting the public key on the csr";
        goto free_all;
    }

    ret = X509_REQ_sign(x509_req, keyPair, EVP_sha1());    // return x509_req->signature->length
    if (ret <= 0){
        qCInfo(lcCse()) << "Error setting the public key on the csr";
        goto free_all;
    }

    ret = PEM_write_bio_X509_REQ(out, x509_req);
    do {
        ret = BIO_gets(out, data, 80);
        output += data;
        if (output.endsWith("-----END CERTIFICATE REQUEST-----")) {
            output = output.trimmed();
            break;
        }
    } while (ret > 0 );

    qCInfo(lcCse()) << "Returning the certificate";
    qCInfo(lcCse()) << output;

    job = new SignPublicKeyApiJob(_account, baseUrl() + "public-key", this);
    job->setCsr(output);

    connect(job, &SignPublicKeyApiJob::jsonReceived, [this, keyPair](const QJsonDocument& json, int retCode) {
        if (retCode == 200) {
            auto caps = json.object().value("ocs").toObject().value("data").toObject().value("public-key").toString();
            qCInfo(lcCse()) << "Public Key Returned" << caps;
            QFile file(publicKeyPath(_account) + ".sign");
            if (file.open(QIODevice::WriteOnly)) {
                QTextStream s(&file);
                s << caps;
            }
            file.close();
            qCInfo(lcCse()) << "public key saved, Encrypting Private Key.";
            encryptPrivateKey(keyPair);
        }
        qCInfo(lcCse()) << retCode;
    });
    job->start();

free_all:
    X509_REQ_free(x509_req);
    BIO_free_all(out);
}

void ClientSideEncryption::setTokenForFolder(const QByteArray& folderId, const QByteArray& token)
{
	_folder2token[folderId] = token;
}

QByteArray ClientSideEncryption::tokenForFolder(const QByteArray& folderId) const
{
	Q_ASSERT(_folder2token.contains(folderId));
	return _folder2token[folderId];
}

void ClientSideEncryption::encryptPrivateKey(EVP_PKEY *keyPair)
{
    // Write the Private File to a BIO
    // Retrieve the BIO contents, and encrypt it.
    // Send the encrypted key to the server.
    BIO* bio = BIO_new(BIO_s_mem());

    QString passPhrase = WordList::getUnifiedString(WordList::getRandomWords(12));
    qCInfo(lcCse()) << "Passphrase Generated:" << passPhrase;

    // Extract the Private key from the key pair.
    PEM_write_bio_PrivateKey(bio, keyPair, NULL, NULL, 0, 0, NULL);
    char data[80];
    QString output;
    int ret = 0;
    do {
        ret = BIO_gets(bio, data, 80);
        output += data;
        if (output.endsWith("-----END PRIVATE KEY-----")) {
            output = output.trimmed();
            break;
        }
    } while (ret > 0 );

    qCInfo(lcCse()) << "Private Key Extracted";
    qCInfo(lcCse()) << output;

    QVector<unsigned char> salt;
    auto secretKey = EncryptionHelper::generatePassword(passPhrase, salt);

    /**
      * NOTES: the key + iv have to be base64 encoded in the metadata.
      */

    //: FIRST TRY A SILLY PHRASE.
    //: Hardcoed IV, really bad.
    unsigned char *fakepass = (unsigned char*) "qwertyuiasdfghjkzxcvbnm,qwertyui";
    unsigned char *iv = (unsigned char *)"0123456789012345";

    const char *encryptTest = "a quick brown fox jumps over the lazy dog";
    // TODO: Find a way to
    unsigned char cryptedText[128];
		unsigned char tag[16];
		int cryptedText_len = encrypt(
        (unsigned char*) encryptTest,   //unsigned char *plaintext,
        strlen(encryptTest),            //        int plaintext_len,
        fakepass,                       //        unsigned char *key,
        iv,                             //        unsigned char *iv,
        cryptedText,                    //        unsigned char *ciphertext,
        tag                             //        unsigned char *tag
    );
		Q_UNUSED(cryptedText_len); //TODO: Fix this.
/*
    qCInfo(lcCse()) << "Encrypted Text" << QByteArray( (const char*) cryptedText, cryptedText_len);
    int decryptedText_len = decrypt(
        cryptedText,     //unsigned char *ciphertext,
        cryptedText_len, //int ciphertext_len,
        NULL,            //unsigned char *aad,
        0,               //int aad_len,
        tag,             //unsigned char *tag,
        fakepass,       //unsigned char *key,
        iv,              //unsigned char *iv,
        decryptedText    //unsigned char *plaintext
    );
    qCInfo(lcCse()) << "Decrypted Text" << QByteArray( (const char*) decryptedText, decryptedText_len);
*/

// Pretend that the private key is actually encrypted and send it to the server.
	auto job = new StorePrivateKeyApiJob(_account, baseUrl() + "private-key", this);
	job->setPrivateKey(QByteArray((const char*) cryptedText, 128));
	connect(job, &StorePrivateKeyApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
		Q_UNUSED(doc);
		switch(retCode) {
			case 200:
				qCInfo(lcCse()) << "Store private key working as expected.";
				emit initializationFinished();
				break;
			default:
				qCInfo(lcCse()) << "Store private key failed, return code:" << retCode;
		}
	});
	job->start();
}

void ClientSideEncryption::getPrivateKeyFromServer()
{
	qCInfo(lcCse()) << "Trying to store the private key on the server.";
}

void ClientSideEncryption::getPublicKeyFromServer()
{
    qCInfo(lcCse()) << "Retrieving public key from server";
    auto job = new JsonApiJob(_account, baseUrl() + "public-key", this);
    connect(job, &JsonApiJob::jsonReceived, [this](const QJsonDocument& doc, int retCode) {
			Q_UNUSED(doc);
			switch(retCode) {
            case 404: // no public key
                qCInfo(lcCse()) << "No public key on the server";
                generateKeyPair();
                break;
            case 400: // internal error
                qCInfo(lcCse()) << "Internal server error while requesting the public key, encryption aborted.";
                break;
            case 200: // ok
                qCInfo(lcCse()) << "Found Public key, requesting Private Key.";
                break;
        }
    });
    job->start();
}

void ClientSideEncryption::fetchFolderEncryptedStatus() {
	_refreshingEncryptionStatus = true;
	auto getEncryptedStatus = new GetFolderEncryptStatus(_account);
	connect(getEncryptedStatus, &GetFolderEncryptStatus::encryptStatusReceived,
					this, &ClientSideEncryption::folderEncryptedStatusFetched);
	connect(getEncryptedStatus, &GetFolderEncryptStatus::encryptStatusError,
					this, &ClientSideEncryption::folderEncryptedStatusError);
	getEncryptedStatus->start();
}

void ClientSideEncryption::folderEncryptedStatusFetched(const QMap<QString, bool>& result)
{
	_refreshingEncryptionStatus = false;
	qDebug() << "Retrieved correctly the encrypted status of the folders." << result;
}

void ClientSideEncryption::folderEncryptedStatusError(int error)
{
	_refreshingEncryptionStatus = false;
	qDebug() << "Failed to retrieve the status of the folders." << error;
}

SignPublicKeyApiJob::SignPublicKeyApiJob(const AccountPtr& account, const QString& path, QObject* parent)
: AbstractNetworkJob(account, path, parent)
{
}

void SignPublicKeyApiJob::setCsr(const QByteArray& csr)
{
    QByteArray data = "csr=";
    data += QUrl::toPercentEncoding(csr);
    _csr.setData(data);
}

void SignPublicKeyApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString>> params = {
        qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json"))
    };
    url.setQueryItems(params);

    qCInfo(lcSignPublicKeyApiJob) << "Sending the CSR" << _csr.data();
    sendRequest("POST", url, req, &_csr);
    AbstractNetworkJob::start();
}

bool SignPublicKeyApiJob::finished()
{
    qCInfo(lcStorePrivateKeyApiJob()) << "Sending CSR ended with"  << path() << errorString() << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}


StorePrivateKeyApiJob::StorePrivateKeyApiJob(const AccountPtr& account, const QString& path, QObject* parent)
: AbstractNetworkJob(account, path, parent)
{
}

void StorePrivateKeyApiJob::setPrivateKey(const QByteArray& privKey)
{
    QByteArray data = "privateKey=";
    data += QUrl::toPercentEncoding(privKey);
    _privKey.setData(data);
}

void StorePrivateKeyApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString>> params = {
        qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json"))
    };
    url.setQueryItems(params);

    qCInfo(lcStorePrivateKeyApiJob) << "Sending the private key" << _privKey.data();
    sendRequest("POST", url, req, &_privKey);
    AbstractNetworkJob::start();
}

bool StorePrivateKeyApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200)
        qCInfo(lcStorePrivateKeyApiJob()) << "Sending private key ended with"  << path() << errorString() << retCode;

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}

SetEncryptionFlagApiJob::SetEncryptionFlagApiJob(const AccountPtr& account, const QByteArray& fileId, QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("encrypted/") + fileId, parent), _fileId(fileId)
{
}

void SetEncryptionFlagApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());

    qCInfo(lcCseJob()) << "marking the file with id" << _fileId << "as encrypted";
    sendRequest("PUT", url, req);
    AbstractNetworkJob::start();
}

bool SetEncryptionFlagApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qCInfo(lcCse()) << "Encryption Flag Return" << reply()->readAll();
    if (retCode == 200) {
        emit success(_fileId);
    } else {
        qCInfo(lcCseJob()) << "Setting the encrypted flag failed with" << path() << errorString() << retCode;
        emit error(_fileId, retCode);
    }
    return true;
}

//TODO: Create an actuall encryption here.
auto metadataKeyEnc(const QByteArray& data) -> QByteArray
{
    return data;
}

auto metadataKeyDec(const QByteArray& data) -> QByteArray
{
    return data;
}

FolderMetadata::FolderMetadata(AccountPtr account, const QByteArray& metadata) : _account(account), _metadata(metadata)
{
    if (metadata.isEmpty()) {
        qCInfo(lcCse()) << "Setupping Empty Metadata";
        setupEmptyMetadata();
    } else {
        qCInfo(lcCse()) << "Metadata already exists, deal with it later.";
    }
}

// RSA/ECB/OAEPWithSHA-256AndMGF1Padding using private / public key.
std::string FolderMetadata::encryptMetadataKeys(const nlohmann::json& metadataKeys) const {
    std::string metadata = metadataKeys.dump();
    unsigned char *metadataPtr = (unsigned char *) metadata.c_str();
    size_t metadataPtrLen = metadata.size();

    int err = -1;
    const int rsaOeapPadding = 1;
    EVP_PKEY *key = nullptr;

    /*TODO: Verify if we need to setup a RSA engine.
     * by default it's RSA OEAP */
    ENGINE *eng = nullptr;

    auto path = publicKeyPath(_account);
    const char *pathC = qPrintable(path);

    FILE* pkeyFile = fopen(pathC, "r");
    if (!pkeyFile) {
        qCInfo(lcCse()) << "Could not open the public key";
        exit(1);
    }

    key = PEM_read_PUBKEY(pkeyFile, NULL, NULL, NULL);

    auto ctx = EVP_PKEY_CTX_new(key, eng);
    if (!ctx) {
        qCInfo(lcCse()) << "Could not initialize the pkey context.";
        exit(1);
    }

    err = EVP_PKEY_encrypt_init(ctx);
    if (err <= 0) {
        qCInfo(lcCse()) << "Error initilaizing the encryption.";
        exit(1);
    }

    err = EVP_PKEY_CTX_set_rsa_padding(ctx, rsaOeapPadding);
    if (err <= 0) {
        qCInfo(lcCse()) << "Error setting the encryption padding.";
        exit(1);
    }

    size_t outLen = 0;
    err = EVP_PKEY_encrypt(ctx, NULL, &outLen, metadataPtr, metadataPtrLen);
    if (err <= 0) {
        qCInfo(lcCse()) << "Error retrieving the size of the encrypted data";
        exit(1);
    } else {
        qCInfo(lcCse()) << "Encrption Length:" << outLen;
    }

    unsigned char *out = (uchar*) OPENSSL_malloc(outLen);
    if (!out) {
        qCInfo(lcCse()) << "Error requesting memory for the encrypted contents";
        exit(1);
    }

    err = EVP_PKEY_encrypt(ctx, out, &outLen, metadataPtr, metadataPtrLen);
    if (err <= 0) {
        qCInfo(lcCse()) << "Could not encrypt key." << err;
        exit(1);
    }

    // Transform the encrypted data into base64.
    const auto raw = QByteArray((const char*) out, outLen);
    const auto b64 = raw.toBase64();
    const auto ret = std::string(b64.constData(), b64.length());

    qCInfo(lcCse()) << raw.toBase64();
    return ret;
}

std::string FolderMetadata::genMetadataPass() const {
    const char* charmap = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const size_t charmapLength = strlen(charmap);
    const int bytes = 16;
    std::string result;
    result.reserve(bytes);
    generate_n(back_inserter(result), bytes, [&](){
      return charmap[rand() % charmapLength];
    });
    return result;
}

std::string FolderMetadata::decryptMetadataKeys(const std::string& encryptedMetadata) const
{
    qCInfo(lcCse()) << "Starting to decrypt the metadata key";
    unsigned char *out = nullptr;
    size_t outlen = 0;
    int err = -1;

    auto path = privateKeyPath(_account);
    auto pathC = qPrintable(path);
    auto pkeyFile = fopen(pathC, "r");
    auto key = PEM_read_PrivateKey(pkeyFile, NULL, NULL, NULL);
    if (!key) {
        qCInfo(lcCse()) << "Error reading private key";
    }

    // Data is base64 encoded.
    qCInfo(lcCse()) << "encryptedMetadata" << encryptedMetadata;
    auto raw = QByteArray(encryptedMetadata.c_str(), encryptedMetadata.length());
    auto b64d = QByteArray::fromBase64(raw);
    auto in = (unsigned char *) b64d.constData();
    size_t inlen = b64d.length();

    qCInfo(lcCse()) << "Encrypted metadata length: " << inlen;

    /* NB: assumes key in, inlen are already set up
    * and that key is an RSA private key
    */
    auto ctx = EVP_PKEY_CTX_new(key, nullptr);
    if (!ctx) {
        qCInfo(lcCse()) << "Could not create the PKEY context.";
        exit(1);
    }

    err = EVP_PKEY_decrypt_init(ctx);
    if (err <= 0) {
        qCInfo(lcCse()) << "Could not init the decryption of the metadata";
        exit(1);
    }

    err = EVP_PKEY_CTX_set_rsa_padding(ctx, 1); //TODO: Make this a class variable.
    if (err <= 0) {
        qCInfo(lcCse()) << "Could not set the RSA padding";
        exit(1);
    }

    err = EVP_PKEY_decrypt(ctx, NULL, &outlen, in, inlen);
    if (err <= 0) {
        qCInfo(lcCse()) << "Could not determine the buffer length";
        exit(1);
    } else {
        qCInfo(lcCse()) << "Size of output is: " << outlen;
    }

    out = (unsigned char *) OPENSSL_malloc(outlen);
    if (!out) {
        qCInfo(lcCse()) << "Could not alloc space for the decrypted metadata";
        exit(1);
    }

    err = EVP_PKEY_decrypt(ctx, out, &outlen, in, inlen);
    if (err <= 0) {
        qCInfo(lcCse()) << "Could not decrypt the metadata";
        exit(1);
    }

    qCInfo(lcCse()) << "Metadata decrypted successfully";
    const auto ret = std::string((char*) out, outlen);

    return ret;
}

// AES/GCM/NoPadding (128 bit key size)
std::string FolderMetadata::encryptJsonObject(const nlohmann::json& obj,const std::string& pass) const {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qCInfo(lcCse()) << "Coult not create encryption context, aborting.";
        exit(1);
    }

    if(!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
        qCInfo(lcCse()) << "Error initializing encryption aborting.";
        exit(1);
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);



    unsigned char *iv = (unsigned char *)"0123456789012345";
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL)) {
        qCInfo(lcCse()) << "Could not set IV length, aborting.";
        exit(1);
    }

    auto key = (const unsigned char*) pass.c_str();
    int err = EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);
    if (err != 1) {
        qCInfo(lcCse()) << "Error setting key and IV, aborting.";
        exit(1);
    }

    std::string metadata = obj.dump();
    int metadataLen = metadata.size();
    int outLen = 0;

    unsigned char *metadataPtr = (unsigned char *) metadata.c_str();

    qCInfo(lcCse()) << "Metadata to be encrypted" << metadata;
    qCInfo(lcCse()) << "Metadata length: " << metadataLen;

    /*
     * max len is metadtaLen + blocksize (16 bytes) -1
     * Add 16 bytes for the tag
     */
    auto out = (unsigned char *) OPENSSL_malloc(metadataLen + 16 + 16 - 1);

    err = EVP_EncryptUpdate(ctx, out, &outLen, metadataPtr, metadataLen);
    if (err != 1) {
        qCInfo(lcCse()) << "Error encrypting the metadata, aborting.";
        exit(1);
    }
    qCInfo(lcCse()) << "Successfully encrypted the  internal json blob.";
    int totalOutputLen = outLen;

    qCInfo(lcCse()) << "Current output length: " << totalOutputLen;
    err = EVP_EncryptFinal_ex(ctx, out + outLen, &outLen);
    if (err != 1) {
        qCInfo(lcCse()) << "Error finalyzing the encryption.";
    }
    totalOutputLen += outLen;
    qCInfo(lcCse()) << "Final output length: " << totalOutputLen;

    /* Get the tag */
    unsigned char tag[16];
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
        qCInfo(lcCse()) << "Error Retrieving the tag";
        handleErrors();
    }

    /* Add tag to cypher text to be compatible with the Android implementation */
    memcpy(out + totalOutputLen, tag, 16);
    totalOutputLen += 16;

    qCInfo(lcCse()) << "Final output length (with tag): " << totalOutputLen;

    // Transform the encrypted data into base64.
    const auto raw = QByteArray((const char*) out, totalOutputLen);
    const auto b64 = raw.toBase64();
    const auto ret = std::string(b64.constData(), b64.length());

    qCInfo(lcCse()) << raw.toBase64();
    return ret;
}

std::string FolderMetadata::decryptJsonObject(const std::string& encryptedMetadata, const std::string& pass) const
{
    // Jesus, should I encrypt here in 128kb chunks?
    // perhaps.

    // Data is base64 encoded.
    // TODO: Transform this bit into a function to remove duplicated code.
    qCInfo(lcCse()) << "encryptedMetadata" << encryptedMetadata;
    auto raw = QByteArray(encryptedMetadata.c_str(), encryptedMetadata.length());
    auto b64d = QByteArray::fromBase64(raw);
    auto in = (unsigned char *) b64d.constData();
    // The tag is appended but it is not part of the cipher text
    size_t inlen = b64d.length() - 16;
    auto tag = in + inlen;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qCInfo(lcCse()) << "Coult not create decryptioncontext, aborting.";
        exit(1);
    }

    /* Initialise the decryption operation. */
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
        qCInfo(lcCse()) << "Error initialializing the decryption, aborting.";
        exit(1);
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    unsigned char *iv = (unsigned char *)"0123456789012345";
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL)) {
        qCInfo(lcCse()) << "Could not set IV length, aborting.";
        exit(1);
    }

    auto key = (const unsigned char*) pass.c_str();
    int err = EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv);
    if (err != 1) {
        qCInfo(lcCse()) << "Error setting the key and iv, aborting.";
        exit(1);
    }

    int outlen = 0;

    /*
     * Max outlen is inlen + blocksize (16 bytes)
     */
    auto out = (unsigned char *) OPENSSL_malloc(inlen + 16);
    err = EVP_DecryptUpdate(ctx, out, &outlen, in, inlen);
    if (err != 1) {
        qCInfo(lcCse()) << "Error decrypting the json blob, aborting.";
        exit(1);
    }
    qCInfo(lcCse()) << "currently decrypted" << std::string( (char*) out, outlen);
    qCInfo(lcCse()) << "Current decrypt  length" << outlen;

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)) {
        qCInfo(lcCse()) << "Error setting the tag, aborting.";
        exit(1);
    }

    qCInfo(lcCse()) << "Tag: " << tag;

    int f_len = outlen;
    err = EVP_DecryptFinal_ex(ctx, out + outlen, &f_len);
    if (err != 1) {
        qCInfo(lcCse()) << "Error finalyzing the decryption, aborting.";
        exit(1);
    }

    qCInfo(lcCse()) << "Decryption finalized.";
    const auto ret = std::string((char*) out, outlen);
    return ret;
}

void FolderMetadata::setupEmptyMetadata() {
    using namespace nlohmann;
    std::string newMetadataPass = genMetadataPass();
    qCInfo(lcCse()) << "Key Generated for the Metadata" << newMetadataPass;

    json metadataKeyObj = {"0", newMetadataPass};
    json recepient = {"recipient", {}};

    auto b64String = encryptMetadataKeys(metadataKeyObj);
    auto sharingEncrypted = encryptJsonObject(recepient, newMetadataPass);

    json m = {
        {"metadata", {
            {"metadataKeys", b64String},
            {"sharing", sharingEncrypted},
            {"version",1}
        }},
        {"files", {
        }}
    };

    std::string result = m.dump();
    QString output = QString::fromStdString(result);
		_metadata = output.toLocal8Bit();
}

QByteArray FolderMetadata::encryptedMetadata() {
	return _metadata;
}

LockEncryptFolderApiJob::LockEncryptFolderApiJob(const AccountPtr& account, const QByteArray& fileId, QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("lock/") + fileId, parent), _fileId(fileId)
{
}

void LockEncryptFolderApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString>> params = {
        qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json"))
    };
    url.setQueryItems(params);

    qCInfo(lcCseJob()) << "locking the folder with id" << _fileId << "as encrypted";
    sendRequest("POST", url, req);
    AbstractNetworkJob::start();
}

bool LockEncryptFolderApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error locking file" << path() << errorString() << retCode;
        emit error(_fileId, retCode);
        return true;
    }

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    auto obj = json.object().toVariantMap();
    auto token = obj["ocs"].toMap()["data"].toMap()["token"].toByteArray();
    qCInfo(lcCse()) << "got json:" << token;

    //TODO: Parse the token and submit.
    emit success(_fileId, token);
    return true;
}


UnlockEncryptFolderApiJob::UnlockEncryptFolderApiJob(const AccountPtr& account,
                                                 const QByteArray& fileId,
                                                 const QByteArray& token,
                                                 QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("lock/") + fileId, parent), _fileId(fileId), _token(token)
{
}

void UnlockEncryptFolderApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setRawHeader("token", _token);

    QUrl url = Utility::concatUrlPath(account()->url(), path());
    sendRequest("DELETE", url, req);

    AbstractNetworkJob::start();
    qCInfo(lcCseJob()) << "Starting the request to unlock.";
}

bool UnlockEncryptFolderApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error unlocking file" << path() << errorString() << retCode;
        qCInfo(lcCseJob()) << "Full Error Log" << reply()->readAll();
        emit error(_fileId, retCode);
        return true;
    }
    emit success(_fileId);
    return true;
}

StoreMetaDataApiJob::StoreMetaDataApiJob(const AccountPtr& account,
                                                 const QByteArray& fileId,
                                                 const QByteArray& b64Metadata,
                                                 QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("meta-data/") + fileId, parent), _fileId(fileId), _b64Metadata(b64Metadata)
{
}

void StoreMetaDataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString>> params = {
        qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")),
    };
    url.setQueryItems(params);

    QByteArray data = QByteArray("metaData=") + _b64Metadata;
    auto buffer = new QBuffer(this);
    buffer->setData(data);

    qCInfo(lcCseJob()) << "sending the metadata for the fileId" << _fileId << "as encrypted";
    sendRequest("POST", url, req, buffer);
    AbstractNetworkJob::start();
}

bool StoreMetaDataApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (retCode != 200) {
			qCInfo(lcCseJob()) << "error sending the metadata" << path() << errorString() << retCode;
			emit error(_fileId, retCode);
		}

		qCInfo(lcCseJob()) << "Metadata submited to the server successfully";
		emit success(_fileId);
    return true;
}

GetMetadataApiJob::GetMetadataApiJob(const AccountPtr& account,
                                    const QByteArray& fileId,
                                    QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("meta-data/") + fileId, parent), _fileId(fileId)
{
}

void GetMetadataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString>> params = {
        qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")),
    };
    url.setQueryItems(params);

    qCInfo(lcCseJob()) << "Requesting the metadata for the fileId" << _fileId << "as encrypted";
    sendRequest("GET", url, req);
    AbstractNetworkJob::start();
}

bool GetMetadataApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200)
        qCInfo(lcCseJob()) << "error requesting the metadata" << path() << errorString() << retCode;

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}

GetFolderEncryptStatus::GetFolderEncryptStatus(const AccountPtr& account, QObject *parent)
	: OCC::AbstractNetworkJob(account, QStringLiteral("remote.php/webdav"), parent)
{
}

void GetFolderEncryptStatus::start()
{
	QNetworkRequest req;
	req.setPriority(QNetworkRequest::HighPriority);
	req.setRawHeader("OCS-APIREQUEST", "true");
	req.setRawHeader("Content-Type", "application/xml");

	QByteArray xml = "<d:propfind xmlns:d=\"DAV:\"> <d:prop xmlns:nc=\"http://nextcloud.org/ns\"> <nc:is-encrypted/> </d:prop> </d:propfind>";
	QBuffer *buf = new QBuffer(this);
	buf->setData(xml);
	buf->open(QIODevice::ReadOnly);
	sendRequest("PROPFIND", Utility::concatUrlPath(account()->url(), path()), req, buf);

	AbstractNetworkJob::start();
}

bool GetFolderEncryptStatus::finished()
{
    qCInfo(lcCse()) << "GetFolderEncryptStatus of" << reply()->request().url() << "finished with status"
                          << reply()->error()
                          << (reply()->error() == QNetworkReply::NoError ? QLatin1String("") : errorString());

    int http_result_code = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

		if (http_result_code == 207) {
        // Parse DAV response
        QXmlStreamReader reader(reply());
        reader.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration("d", "DAV:"));

				/* Example Xml
				<?xml version="1.0"?>
					<d:multistatus xmlns:d="DAV:" xmlns:s="http://sabredav.org/ns" xmlns:oc="http://owncloud.org/ns" xmlns:nc="http://nextcloud.org/ns">
						<d:response>
							<d:href>/remote.php/webdav/</d:href>
							<d:propstat>
								<d:prop>
									<nc:is-encrypted>0</nc:is-encrypted>
								</d:prop>
								<d:status>HTTP/1.1 200 OK</d:status>
							</d:propstat>
						</d:response>
					</d:multistatus>
				*/

				QString currFile;
				int currEncryptedStatus = -1;
				QMap<QString, bool> folderStatus;
        while (!reader.atEnd()) {
            auto type = reader.readNext();
            if (type == QXmlStreamReader::StartElement) {
								if (reader.name() == QLatin1String("href")) {
									currFile = reader.readElementText(QXmlStreamReader::SkipChildElements);
								}
                if (reader.name() == QLatin1String("is-encrypted")) {
									currEncryptedStatus = (bool) reader.readElementText(QXmlStreamReader::SkipChildElements).toInt();
								}
            }

            if (!currFile.isEmpty() && currEncryptedStatus != -1) {
							folderStatus.insert(currFile, currEncryptedStatus);
							currFile.clear();
							currEncryptedStatus = -1;
						}
        }

        emit encryptStatusReceived(folderStatus);
    } else {
        qCWarning(lcCse()) << "*not* successful, http result code is" << http_result_code
                                 << (http_result_code == 302 ? reply()->header(QNetworkRequest::LocationHeader).toString() : QLatin1String(""));
        emit encryptStatusError(http_result_code);
				// emit finishedWithError(reply());
    }
    return true;
}

}
