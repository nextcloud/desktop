#include "webview.h"

#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>
#include <QProgressBar>
#include <QLoggingCategory>
#include <QLocale>

#include "common/utility.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcWizardWebiew, "gui.wizard.webview", QtInfoMsg)


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


WebView::WebView(QWidget *parent)
    : QWidget(parent),
      _ui()
{
    _ui.setupUi(this);

    _webview = new QWebEngineView(this);
    _profile = new QWebEngineProfile(this);
    _page = new QWebEnginePage(_profile);
    _interceptor = new WebViewPageUrlRequestInterceptor(this);
    _schemeHandler = new WebViewPageUrlSchemeHandler(this);

    _profile->setHttpUserAgent(Utility::userAgentString());
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

    qCInfo(lcWizardWebiew()) << "Got user: " << user << ", server: " << server;

    emit urlCatched(user, password, server);
}

}

#include "webview.moc"
