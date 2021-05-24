#ifndef WEBFLOWCREDENTIALSDIALOG_H
#define WEBFLOWCREDENTIALSDIALOG_H

#include <QDialog>
#include <QUrl>

#include "accountfwd.h"
#include "creds/flow2auth.h"

class QLabel;
class QVBoxLayout;

namespace OCC {

class HeaderBanner;
class WebView;
class Flow2AuthWidget;

class WebFlowCredentialsDialog : public QDialog
{
    Q_OBJECT
public:
    WebFlowCredentialsDialog(Account *account, QWidget *parent = nullptr);

    void setInfo(const QString &msg);
    void setError(const QString &error);

protected:
    void closeEvent(QCloseEvent * e) override;
    void changeEvent(QEvent *) override;

public slots:
    void slotFlow2AuthResult(Flow2Auth::Result, const QString &errorString, const QString &user, const QString &appPassword);
    void slotShowSettingsDialog();

signals:
    void urlCatched(const QString user, const QString pass, const QString host);
    void styleChanged();
    void onActivate();
    void onClose();

private:
    void customizeStyle();

    Flow2AuthWidget *_flow2AuthWidget;

    QLabel *_errorLabel;
    QLabel *_infoLabel;
    QVBoxLayout *_layout;
    QVBoxLayout *_containerLayout;
    HeaderBanner *_headerBanner;
};

} // namespace OCC

#endif // WEBFLOWCREDENTIALSDIALOG_H
