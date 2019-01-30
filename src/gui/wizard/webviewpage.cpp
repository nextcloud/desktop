#include "webviewpage.h"

#include <QWebEngineUrlRequestJob>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QNetworkProxyFactory>

#include "owncloudwizard.h"
#include "creds/webflowcredentials.h"
#include "webview.h"
#include "account.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcWizardWebiewPage, "gui.wizard.webviewpage", QtInfoMsg)


WebViewPage::WebViewPage(QWidget *parent)
    : AbstractCredentialsWizardPage()
{
    _ocWizard = qobject_cast<OwncloudWizard *>(parent);

    qCInfo(lcWizardWebiewPage()) << "Time for a webview!";
    _webView = new WebView(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(_webView);
    setLayout(layout);

    connect(_webView, &WebView::urlCatched, this, &WebViewPage::urlCatched);

    _useSystemProxy = QNetworkProxyFactory::usesSystemConfiguration();
}

WebViewPage::~WebViewPage() {
    QNetworkProxyFactory::setUseSystemConfiguration(_useSystemProxy);
}

void WebViewPage::initializePage() {
    QNetworkProxy::setApplicationProxy(QNetworkProxy::applicationProxy());

    QString url;
    if (_ocWizard->registration()) {
        url = "https://nextcloud.com/register";
    } else {
        url = _ocWizard->ocUrl();
        if (!url.endsWith('/')) {
            url += "/";
        }
        url += "index.php/login/flow";
    }
    qCInfo(lcWizardWebiewPage()) << "Url to auth at: " << url;
    _webView->setUrl(QUrl(url));
}

int WebViewPage::nextId() const {
    return WizardCommon::Page_AdvancedSetup;
}

bool WebViewPage::isComplete() const {
    return false;
}

AbstractCredentials* WebViewPage::getCredentials() const {
    return new WebFlowCredentials(_user, _pass, _ocWizard->_clientSslCertificate, _ocWizard->_clientSslKey);
}

void WebViewPage::setConnected() {
    qCInfo(lcWizardWebiewPage()) << "YAY! we are connected!";
}

void WebViewPage::urlCatched(QString user, QString pass, QString host) {
    qCInfo(lcWizardWebiewPage()) << "Got user: " << user << ", server: " << host;

    _user = user;
    _pass = pass;

    AccountPtr account = _ocWizard->account();
    account->setUrl(host);

    qCInfo(lcWizardWebiewPage()) << "URL: " << field("OCUrl").toString();
    emit connectToOCUrl(host);
}

}
