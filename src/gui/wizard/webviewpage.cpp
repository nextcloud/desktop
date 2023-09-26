#include "webviewpage.h"

#include <QWebEngineUrlRequestJob>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QNetworkProxyFactory>
#include <QScreen>

#include "account.h"
#include "common/utility.h"
#include "creds/webflowcredentials.h"
#include "owncloudwizard.h"
#include "webview.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcWizardWebiewPage, "nextcloud.gui.wizard.webviewpage", QtInfoMsg)


WebViewPage::WebViewPage(QWidget *parent)
    : AbstractCredentialsWizardPage()
{
    _ocWizard = qobject_cast<OwncloudWizard *>(parent);

    qCInfo(lcWizardWebiewPage()) << "Time for a webview!";
    _webView = new WebView(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_webView, 1);
    setLayout(layout);

    connect(_webView, &WebView::urlCatched, this, &WebViewPage::urlCatched);

    //_useSystemProxy = QNetworkProxyFactory::usesSystemConfiguration();
}

WebViewPage::~WebViewPage() = default;
//{
//    QNetworkProxyFactory::setUseSystemConfiguration(_useSystemProxy);
//}

void WebViewPage::initializePage() {
    //QNetworkProxy::setApplicationProxy(QNetworkProxy::applicationProxy());

    QString url;
    if (_ocWizard->registration()) {
        url = "https://nextcloud.com/register";
    } else {
        url = Utility::trailingSlashPath(_ocWizard->ocUrl()) + "index.php/login/flow";
    }
    qCInfo(lcWizardWebiewPage()) << "Url to auth at: " << url;
    _webView->setUrl(QUrl(url));

    _originalWizardSize = _ocWizard->size();
    resizeWizard();
}

void WebViewPage::resizeWizard()
{
    // The webview needs a little bit more space
    auto wizardSizeChanged = tryToSetWizardSize(_originalWizardSize.width() * 2, _originalWizardSize.height() * 2);

    if (!wizardSizeChanged) {
        wizardSizeChanged = tryToSetWizardSize(static_cast<int>(_originalWizardSize.width() * 1.5), static_cast<int>(_originalWizardSize.height() * 1.5));
    }

    if (wizardSizeChanged) {
        _ocWizard->centerWindow();
    }
}

bool WebViewPage::tryToSetWizardSize(int width, int height)
{
    const auto window = _ocWizard->window();
    const auto screenGeometry = QGuiApplication::screenAt(window->pos())->geometry();
    const auto windowWidth = screenGeometry.width();
    const auto windowHeight = screenGeometry.height();

    if (width < windowWidth && height < windowHeight) {
        _ocWizard->resize(width, height);
        return true;
    }

    return false;
}

void WebViewPage::cleanupPage()
{
    _ocWizard->resize(_originalWizardSize);
    _ocWizard->centerWindow();
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
