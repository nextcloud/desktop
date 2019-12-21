#include "webflowcredentialsdialog.h"

#include <QVBoxLayout>
#include <QLabel>

#include "theme.h"
#include "application.h"
#include "owncloudgui.h"
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

        // Connect styleChanged events to our widgets, so they can adapt (Dark-/Light-Mode switching)
        connect(this, &WebFlowCredentialsDialog::styleChanged, _flow2AuthWidget, &Flow2AuthWidget::slotStyleChanged);

        // allow Flow2 page to poll on window activation
        connect(this, &WebFlowCredentialsDialog::onActivate, _flow2AuthWidget, &Flow2AuthWidget::slotPollNow);
    } else {
        _webView = new WebView();
        _layout->addWidget(_webView);

        connect(_webView, &WebView::urlCatched, this, &WebFlowCredentialsDialog::urlCatched);
    }

    auto app = static_cast<Application *>(qApp);
    connect(app, &Application::isShowingSettingsDialog, this, &WebFlowCredentialsDialog::slotShowSettingsDialog);

    _errorLabel = new QLabel();
    _errorLabel->hide();
    _layout->addWidget(_errorLabel);

    WizardCommon::initErrorLabel(_errorLabel);

    setLayout(_layout);

    customizeStyle();
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
    // bring window to top
    slotShowSettingsDialog();

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

void WebFlowCredentialsDialog::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        customizeStyle();

        // Notify the other widgets (Dark-/Light-Mode switching)
        emit styleChanged();
        break;
    case QEvent::ActivationChange:
        if(isActiveWindow())
            emit onActivate();
        break;
    default:
        break;
    }

    QDialog::changeEvent(e);
}

void WebFlowCredentialsDialog::customizeStyle()
{
    // HINT: Customize dialog's own style here, if necessary in the future (Dark-/Light-Mode switching)
}

void WebFlowCredentialsDialog::slotShowSettingsDialog()
{
    // bring window to top but slightly delay, to avoid being hidden behind the SettingsDialog
    QTimer::singleShot(100, this, [this] {
        ownCloudGui::raiseDialog(this);
    });
}

} // namespace OCC
