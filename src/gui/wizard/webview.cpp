#include "webview.h"

#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>
#include <QDesktopServices>
#include <QProgressBar>
#include <QLoggingCategory>
#include <QLocale>
#include <QWebEngineCertificateError>
#include <QMessageBox>

#include "common/utility.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcWizardWebiew, "gui.wizard.webview", QtInfoMsg)


class WebViewPageUrlRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    WebViewPageUrlRequestInterceptor(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
};

class WebViewPageUrlSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    WebViewPageUrlSchemeHandler(QObject *parent = nullptr);
    void requestStarted(QWebEngineUrlRequestJob *request) override;

Q_SIGNALS:
    void urlCatched(QString user, QString pass, QString host);
};

class WebEnginePage : public QWebEnginePage {
public:
    WebEnginePage(QWebEngineProfile *profile, QObject* parent = nullptr);
    QWebEnginePage * createWindow(QWebEnginePage::WebWindowType type) override;
    void setUrl(const QUrl &url);

protected:
    bool certificateError(const QWebEngineCertificateError &certificateError) override;

private:
    QUrl _rootUrl;
};

// We need a separate class here, since we cannot simply return the same WebEnginePage object
// this leads to a strage segfault somewhere deep inside of the QWebEngine code
class ExternalWebEnginePage : public QWebEnginePage {
public:
    ExternalWebEnginePage(QWebEngineProfile *profile, QObject* parent = nullptr);
    bool acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame) override;
};

WebView::WebView(QWidget *parent)
    : QWidget(parent),
      _ui()
{
    _ui.setupUi(this);

    _webview = new QWebEngineView(this);
    _profile = new QWebEngineProfile(this);
    _page = new WebEnginePage(_profile);
    _interceptor = new WebViewPageUrlRequestInterceptor(this);
    _schemeHandler = new WebViewPageUrlSchemeHandler(this);

    const QString userAgent(Utility::userAgentString());
    _profile->setHttpUserAgent(userAgent);
    QWebEngineProfile::defaultProfile()->setHttpUserAgent(userAgent);
    _profile->setRequestInterceptor(_interceptor);
    _profile->installUrlSchemeHandler("nc", _schemeHandler);

    /*
     * Set a proper accept langauge to the language of the client
     * code from: http://code.qt.io/cgit/qt/qtbase.git/tree/src/network/access/qhttpnetworkconnection.cpp
     */
    {
        QString systemLocale = QLocale::system().name().replace(QChar::fromLatin1('_'),QChar::fromLatin1('-'));
        QString acceptLanguage;
        if (systemLocale == QLatin1String("C")) {
            acceptLanguage = QString::fromLatin1("en,*");
        } else if (systemLocale.startsWith(QLatin1String("en-"))) {
            acceptLanguage = systemLocale + QLatin1String(",*");
        } else {
            acceptLanguage = systemLocale + QLatin1String(",en,*");
        }
        _profile->setHttpAcceptLanguage(acceptLanguage);
    }

    _webview->setPage(_page);
    _ui.verticalLayout->addWidget(_webview);

    connect(_webview, &QWebEngineView::loadProgress, _ui.progressBar, &QProgressBar::setValue);
    connect(_schemeHandler, &WebViewPageUrlSchemeHandler::urlCatched, this, &WebView::urlCatched);
}

void WebView::setUrl(const QUrl &url) {
    _page->setUrl(url);
}

WebView::~WebView() {
    /*
     * The Qt implmentation deletes children in the order they are added to the
     * object tree, so in this case _page is deleted after _profile, which
     * violates the assumption that _profile should exist longer than
     * _page [1]. Here I delete _page manually so that _profile can be safely
     * deleted later.
     *
     * [1] https://doc.qt.io/qt-5/qwebenginepage.html#QWebEnginePage-1
     */
    delete _page;
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

    user = QUrl::fromPercentEncoding(user.toUtf8());
    password = QUrl::fromPercentEncoding(password.toUtf8());

    user = user.replace(QChar('+'), QChar(' '));
    password = password.replace(QChar('+'), QChar(' '));

    if (!server.startsWith("http://") && !server.startsWith("https://")) {
        server = "https://" + server;
    }
    qCInfo(lcWizardWebiew()) << "Got user: " << user << ", server: " << server;

    emit urlCatched(user, password, server);
}


WebEnginePage::WebEnginePage(QWebEngineProfile *profile, QObject* parent) : QWebEnginePage(profile, parent) {

}

QWebEnginePage * WebEnginePage::createWindow(QWebEnginePage::WebWindowType type) {
    ExternalWebEnginePage *view = new ExternalWebEnginePage(this->profile());
    return view;
}

void WebEnginePage::setUrl(const QUrl &url) {
    QWebEnginePage::setUrl(url);
    _rootUrl = url;
}

bool WebEnginePage::certificateError(const QWebEngineCertificateError &certificateError) {
    if (certificateError.error() == QWebEngineCertificateError::CertificateAuthorityInvalid &&
        certificateError.url().host() == _rootUrl.host()) {
        return true;
    }

    /**
     * TODO properly improve this.
     * The certificate should be displayed.
     *
     * Or rather we should do a request with the QNAM and see if it works (then it is in the store).
     * This is just a quick fix for now.
     */
    QMessageBox messageBox;
    messageBox.setText(tr("Invalid certificate detected"));
    messageBox.setInformativeText(tr("The host \"%1\" provided an invalid certitiface. Continue?").arg(certificateError.url().host()));
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
    messageBox.setDefaultButton(QMessageBox::No);

    int ret = messageBox.exec();

    return ret == QMessageBox::Yes;
}

ExternalWebEnginePage::ExternalWebEnginePage(QWebEngineProfile *profile, QObject* parent) : QWebEnginePage(profile, parent) {

}


bool ExternalWebEnginePage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame)
{
    QDesktopServices::openUrl(url);
    return false;
}

}

#include "webview.moc"
