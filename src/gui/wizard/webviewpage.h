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
    WebViewPage(QWidget *parent = nullptr);
    ~WebViewPage();

    void initializePage() override;
    void cleanupPage() override;
    int nextId() const override;
    bool isComplete() const override;

    AbstractCredentials* getCredentials() const override;
    void setConnected();

signals:
    void connectToOCUrl(const QString&);

private slots:
    void urlCatched(QString user, QString pass, QString host);

private:
    void resizeWizard();
    bool tryToSetWizardSize(int width, int height);

    OwncloudWizard *_ocWizard;
    WebView *_webView;

    QString _user;
    QString _pass;

    bool _useSystemProxy;

    QSize _originalWizardSize;
};

}

#endif // WEBVIEWPAGE_H
