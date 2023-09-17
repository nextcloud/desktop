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
    ~WebViewPage() override;

    void initializePage() override;
    void cleanupPage() override;
    [[nodiscard]] int nextId() const override;
    [[nodiscard]] bool isComplete() const override;

    [[nodiscard]] AbstractCredentials* getCredentials() const override;
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

    bool _useSystemProxy = false;

    QSize _originalWizardSize;
};

}

#endif // WEBVIEWPAGE_H
