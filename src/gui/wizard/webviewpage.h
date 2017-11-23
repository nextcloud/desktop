#ifndef WEBVIEWPAGE_H
#define WEBVIEWPAGE_H

#include <QWizard>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlSchemeHandler>

#include "wizard/abstractcredswizardpage.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"

#include "ui_webviewpage.h"

namespace OCC {

class WebViewPageUrlRequestInterceptor;
class WebViewPageUrlSchemeHandler;

class WebViewPage : public AbstractCredentialsWizardPage
{
    Q_OBJECT
public:
    WebViewPage(QWidget *parent = 0);

    void initializePage() Q_DECL_OVERRIDE;
    int nextId() const Q_DECL_OVERRIDE;
    bool isComplete() const;

    AbstractCredentials* getCredentials() const;
    void setConnected();

Q_SIGNALS:
    void connectToOCUrl(const QString&);

private slots:
    void urlCatched(QString user, QString pass, QString host);

private:
    Ui_WebViewPage _ui;
    OwncloudWizard *_ocWizard;

    QWebEngineView *_webview;
    QWebEngineProfile *_profile;
    QWebEnginePage *_page;

    WebViewPageUrlRequestInterceptor *_interceptor;
    WebViewPageUrlSchemeHandler *_schemeHandler;

    QString _user;
    QString _pass;
};

}

#endif // WEBVIEWPAGE_H
