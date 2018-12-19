#include "webflowcredentialsdialog.h"

#include <QVBoxLayout>
#include <QLabel>

#include "wizard/webview.h"

namespace OCC {

WebFlowCredentialsDialog::WebFlowCredentialsDialog(QWidget *parent)
    : QDialog(parent)
{
    _layout = new QVBoxLayout(this);

    //QString msg = tr("You have been logged out of %1 as user %2, please login again")
    //        .arg(_account->displayName(), _user);
    _infoLabel = new QLabel();
    _layout->addWidget(_infoLabel);

    _webView = new WebView();
    _layout->addWidget(_webView);

    _errorLabel = new QLabel();
    _errorLabel->hide();
    _layout->addWidget(_errorLabel);

    setLayout(_layout);

    connect(_webView, &WebView::urlCatched, this, &WebFlowCredentialsDialog::urlCatched);
}

void WebFlowCredentialsDialog::closeEvent(QCloseEvent* e) {
    Q_UNUSED(e);

    // Force calling WebView::~WebView() earlier so that _profile and _page are
    // deleted in the correct order.
    delete _webView;
}

void WebFlowCredentialsDialog::setUrl(const QUrl &url) {
    _webView->setUrl(url);
}

void WebFlowCredentialsDialog::setInfo(const QString &msg) {
    _infoLabel->setText(msg);
}

void WebFlowCredentialsDialog::setError(const QString &error) {
    if (error.isEmpty()) {
        _errorLabel->hide();
    } else {
        _errorLabel->setText(error);
        _errorLabel->show();
    }
}

}
