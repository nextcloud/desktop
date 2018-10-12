#ifndef WEBFLOWCREDENTIALSDIALOG_H
#define WEBFLOWCREDENTIALSDIALOG_H

#include <QDialog>
#include <QUrl>

class QLabel;
class QVBoxLayout;

namespace OCC {

class WebView;

class WebFlowCredentialsDialog : public QDialog
{
    Q_OBJECT
public:
    WebFlowCredentialsDialog(QWidget *parent = Q_NULLPTR);

    void setUrl(const QUrl &url);
    void setInfo(const QString &msg);
    void setError(const QString &error);

signals:
    void urlCatched(const QString user, const QString pass, const QString host);

private:
    WebView *_webView;
    QLabel *_errorLabel;
    QLabel *_infoLabel;
    QVBoxLayout *_layout;
};

}

#endif // WEBFLOWCREDENTIALSDIALOG_H
