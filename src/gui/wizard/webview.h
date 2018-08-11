#ifndef WEBVIEW_H
#define WEBVIEW_H

#include <QUrl>
#include <QWidget>

#include "ui_webview.h"

class QWebEngineView;
class QWebEngineProfile;
class QWebEnginePage;

namespace OCC {

class WebViewPageUrlRequestInterceptor;
class WebViewPageUrlSchemeHandler;
class WebEnginePage;

class WebView : public QWidget
{
    Q_OBJECT
public:
    WebView(QWidget *parent = 0);
    void setUrl(const QUrl &url);

signals:
    void urlCatched(const QString user, const QString pass, const QString host);

private:
    Ui_WebView _ui;

    QWebEngineView *_webview;
    QWebEngineProfile *_profile;
    WebEnginePage *_page;

    WebViewPageUrlRequestInterceptor *_interceptor;
    WebViewPageUrlSchemeHandler *_schemeHandler;
};

}

#endif // WEBVIEW_H
