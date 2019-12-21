#ifndef WEBFLOWCREDENTIALSDIALOG_H
#define WEBFLOWCREDENTIALSDIALOG_H

#include <QDialog>
#include <QUrl>

#include "accountfwd.h"

class QLabel;
class QVBoxLayout;

namespace OCC {

class WebView;
class Flow2AuthWidget;

class WebFlowCredentialsDialog : public QDialog
{
    Q_OBJECT
public:
    WebFlowCredentialsDialog(Account *account, bool useFlow2, QWidget *parent = nullptr);

    void setUrl(const QUrl &url);
    void setInfo(const QString &msg);
    void setError(const QString &error);

    bool isUsingFlow2() const {
        return _useFlow2;
    }

protected:
    void closeEvent(QCloseEvent * e) override;
    void changeEvent(QEvent *) override;

public slots:
    void slotShowSettingsDialog();

signals:
    void urlCatched(const QString user, const QString pass, const QString host);
    void onActivate();

private:
    bool _useFlow2;

    Flow2AuthWidget *_flow2AuthWidget;
    WebView *_webView;

    QLabel *_errorLabel;
    QLabel *_infoLabel;
    QVBoxLayout *_layout;
};

}

#endif // WEBFLOWCREDENTIALSDIALOG_H
