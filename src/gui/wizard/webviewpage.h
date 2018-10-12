#ifndef WEBVIEWPAGE_H
#define WEBVIEWPAGE_H

#include "wizard/abstractcredswizardpage.h"

namespace OCC {

class AbstractCredentials;
class OwncloudWizard;
class WebView;

class WebViewPage : public AbstractCredentialsWizardPage
{
    Q_OBJECT
public:
    WebViewPage(QWidget *parent = Q_NULLPTR);

    void initializePage() Q_DECL_OVERRIDE;
    int nextId() const Q_DECL_OVERRIDE;
    bool isComplete() const;

    AbstractCredentials* getCredentials() const;
    void setConnected();

signals:
    void connectToOCUrl(const QString&);

private slots:
    void urlCatched(QString user, QString pass, QString host);

private:
    OwncloudWizard *_ocWizard;
    WebView *_webView;

    QString _user;
    QString _pass;
};

}

#endif // WEBVIEWPAGE_H
