#include "webviewpage.h"

#include <QWebEngineUrlRequestJob>
#include <QProgressBar>
#include <QVBoxLayout>

#include "owncloudwizard.h"
#include "creds/webflowcredentials.h"
#include "webview.h"

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
}

void WebViewPage::initializePage() {
    auto url = _ocWizard->ocUrl();
    url += "/index.php/login/flow";
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

    qCInfo(lcWizardWebiewPage()) << "URL: " << field("OCUrl").toString();
    emit connectToOCUrl(field("OCUrl").toString().simplified());
}

}
