#include "webflowcredentialsdialog.h"

#include <QVBoxLayout>
#include <QLabel>

#include "theme.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/webview.h"
#include "wizard/flow2authwidget.h"

namespace OCC {

WebFlowCredentialsDialog::WebFlowCredentialsDialog(Account *account, bool useFlow2, QWidget *parent)
    : QDialog(parent),
      _useFlow2(useFlow2),
      _flow2AuthWidget(nullptr),
      _webView(nullptr)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    _layout = new QVBoxLayout(this);

    //QString msg = tr("You have been logged out of %1 as user %2, please login again")
    //        .arg(_account->displayName(), _user);
    _infoLabel = new QLabel();
    _layout->addWidget(_infoLabel);

    if (_useFlow2) {
        _flow2AuthWidget = new Flow2AuthWidget(account);
        _layout->addWidget(_flow2AuthWidget);

        connect(_flow2AuthWidget, &Flow2AuthWidget::urlCatched, this, &WebFlowCredentialsDialog::urlCatched);
    } else {
        _webView = new WebView();
        _layout->addWidget(_webView);

        connect(_webView, &WebView::urlCatched, this, &WebFlowCredentialsDialog::urlCatched);
    }

    _errorLabel = new QLabel();
    _errorLabel->hide();
    _layout->addWidget(_errorLabel);

    WizardCommon::initErrorLabel(_errorLabel);

    setLayout(_layout);
}

void WebFlowCredentialsDialog::closeEvent(QCloseEvent* e) {
    Q_UNUSED(e)

    if (_webView) {
        // Force calling WebView::~WebView() earlier so that _profile and _page are
        // deleted in the correct order.
        delete _webView;
    }

    if (_flow2AuthWidget)
        delete _flow2AuthWidget;
}

void WebFlowCredentialsDialog::setUrl(const QUrl &url) {
    if (_webView)
        _webView->setUrl(url);
}

void WebFlowCredentialsDialog::setInfo(const QString &msg) {
    _infoLabel->setText(msg);
}

void WebFlowCredentialsDialog::setError(const QString &error) {
    if (_useFlow2 && _flow2AuthWidget) {
        _flow2AuthWidget->setError(error);
        return;
    }

    if (error.isEmpty()) {
        _errorLabel->hide();
    } else {
        _errorLabel->setText(error);
        _errorLabel->show();
    }
}

}
