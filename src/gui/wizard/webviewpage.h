#ifndef WEBVIEWPAGE_H
#define WEBVIEWPAGE_H

#include <QWizard>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineUrlRequestInterceptor>

#include "wizard/abstractcredswizardpage.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"

#include "ui_webviewpage.h"

namespace OCC {

class WebViewPageUrlRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    WebViewPageUrlRequestInterceptor(QObject *parent = 0);
    void interceptRequest(QWebEngineUrlRequestInfo &info);
};

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

private:
    Ui_WebViewPage _ui;
    OwncloudWizard *_ocWizard;

    QWebEngineView *_webview;
    QWebEngineProfile *_profile;
    QWebEnginePage *_page;
    WebViewPageUrlRequestInterceptor *_interceptor;
};

}

#endif // WEBVIEWPAGE_H
