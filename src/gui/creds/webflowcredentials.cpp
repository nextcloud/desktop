#include "webflowcredentials.h"

#include <keychain.h>

#include "accessmanager.h"
#include "account.h"
#include "theme.h"

using namespace QKeychain;

namespace OCC {

WebFlowCredentials::WebFlowCredentials()
    : _ready(false)
{

}

WebFlowCredentials::WebFlowCredentials(const QString &user, const QString &password, const QSslCertificate &certificate, const QSslKey &key)
    : _user(user)
    , _password(password)
    , _clientSslKey(key)
    , _clientSslCertificate(certificate)
    , _ready(true)
{

}

QString WebFlowCredentials::authType() const {
    return QString::fromLatin1("webflow");
}

QString WebFlowCredentials::user() const {
    return _user;
}

QNetworkAccessManager *WebFlowCredentials::createQNAM() const {
    AccessManager *qnam = new AccessManager();
    return qnam;
}

bool WebFlowCredentials::ready() const {

}

void WebFlowCredentials::fetchFromKeychain() {

}
void WebFlowCredentials::askFromUser() {

}

bool WebFlowCredentials::stillValid(QNetworkReply *reply) {

}

void WebFlowCredentials::persist() {
    //TODO: Add ssl cert and key storing
    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(keychainKey(_account->url().toString(), _user, _account->id()));
    job->setTextData(_password);
    job->start();
}

void WebFlowCredentials::invalidateToken() {

}

void WebFlowCredentials::forgetSensitiveData(){

}

}
