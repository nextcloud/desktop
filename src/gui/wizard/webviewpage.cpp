#include "webviewpage.h"

#include <QWebEngineUrlRequestJob>
#include <QProgressBar>

#include "creds/httpcredentialsgui.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcWizardWebiewPage, "gui.wizard.webviewpage", QtInfoMsg)


class WebViewPageUrlRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    WebViewPageUrlRequestInterceptor(QObject *parent = 0);
    void interceptRequest(QWebEngineUrlRequestInfo &info);
};

class WebViewPageUrlSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    WebViewPageUrlSchemeHandler(QObject *parent = 0);
    void requestStarted(QWebEngineUrlRequestJob *request);

Q_SIGNALS:
    void urlCatched(QString user, QString pass, QString host);
};


WebViewPage::WebViewPage(QWidget *parent)
    : AbstractCredentialsWizardPage(),
      _ui()
{
    _ui.setupUi(this);
    _ocWizard = qobject_cast<OwncloudWizard *>(parent);

    _webview = new QWebEngineView(this);
    _profile = new QWebEngineProfile(this);
    _page = new QWebEnginePage(_profile);
    _interceptor = new WebViewPageUrlRequestInterceptor(this);
    _schemeHandler = new WebViewPageUrlSchemeHandler(this);

    _profile->setHttpUserAgent(Utility::userAgentString());
    _profile->setRequestInterceptor(_interceptor);
    _profile->installUrlSchemeHandler("nc", _schemeHandler);

    _webview->setPage(_page);
    _ui.verticalLayout->addWidget(_webview);
    _webview->show();

    connect(_webview, &QWebEngineView::loadProgress, _ui.progressBar, &QProgressBar::setValue);
    connect(_schemeHandler, &WebViewPageUrlSchemeHandler::urlCatched, this, &WebViewPage::urlCatched);
}

void WebViewPage::initializePage() {
    auto url = _ocWizard->ocUrl();
    url += "/index.php/login/flow";
    qCInfo(lcWizardWebiewPage()) << "Url to auth at: " << url;;
    _page->setUrl(QUrl(url));
}

int WebViewPage::nextId() const {
    return WizardCommon::Page_AdvancedSetup;
}

bool WebViewPage::isComplete() const {
    return false;
}

AbstractCredentials* WebViewPage::getCredentials() const {
    return new HttpCredentialsGui(_user, _pass, _ocWizard->_clientSslCertificate, _ocWizard->_clientSslKey);
}

void WebViewPage::setConnected() {
    qCInfo(lcWizardWebiewPage()) << "YAY! we are connected!";
}

void WebViewPage::urlCatched(QString user, QString pass, QString host) {
    qCInfo(lcWizardWebiewPage()) << "Got user: " << user << ", password: " << pass << ", server: " << host;

    _user = user;
    _pass = pass;

    qCInfo(lcWizardWebiewPage()) << "URL: " << field("OCUrl").toString();
    emit connectToOCUrl(field("OCUrl").toString().simplified());
}

WebViewPageUrlRequestInterceptor::WebViewPageUrlRequestInterceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent) {

}

void WebViewPageUrlRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    info.setHttpHeader("OCS-APIREQUEST", "true");
}

WebViewPageUrlSchemeHandler::WebViewPageUrlSchemeHandler(QObject *parent)
    : QWebEngineUrlSchemeHandler(parent) {

}

void WebViewPageUrlSchemeHandler::requestStarted(QWebEngineUrlRequestJob *request) {
    QUrl url = request->requestUrl();

    QString path = url.path().mid(1);
    QStringList parts = path.split("&");

    QString server;
    QString user;
    QString password;

    for (QString part : parts) {
        if (part.startsWith("server:")) {
            server = part.mid(7);
        } else if (part.startsWith("user:")) {
            user = part.mid(5);
        } else if (part.startsWith("password:")) {
            password = part.mid(9);
        }
    }

    qCInfo(lcWizardWebiewPage()) << "Got user: " << user << ", password: " << password << ", server: " << server;

    emit urlCatched(user, password, server);
}

}

#include "webviewpage.moc"
