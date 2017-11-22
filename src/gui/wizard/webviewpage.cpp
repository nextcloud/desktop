#include "webviewpage.h"

#include "creds/httpcredentialsgui.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcWizardWebiewPage, "gui.wizard.webviewpage", QtInfoMsg)

WebViewPage::WebViewPage(QWidget *parent)
    : AbstractCredentialsWizardPage(),
      _ui()
{
    _ocWizard = qobject_cast<OwncloudWizard *>(parent);

    _ui.setupUi(this);



    _webview = new QWebEngineView(this);
    _profile = new QWebEngineProfile(this);
    _page = new QWebEnginePage(_profile);
    _interceptor = new WebViewPageUrlRequestInterceptor(this);

    _profile->setHttpUserAgent(Utility::userAgentString());
    _profile->setRequestInterceptor(_interceptor);

    _webview->setPage(_page);
    _ui.verticalLayout->addWidget(_webview);
    _webview->show();

    connect(_webview, SIGNAL(loadProgress(int)), _ui.progressBar, SLOT(setValue(int)));

}

void WebViewPage::initializePage() {
    auto url = _ocWizard->ocUrl();
    url += "/index.php/login/flow";
    qCInfo(lcWizardWebiewPage()) << "Url to auth at: " << url;;
    _page->setUrl(QUrl(url));
}

int WebViewPage::nextId() const {
    return WizardCommon::Page_AdvancedSetup;
}

bool WebViewPage::isComplete() const {
    return false;
}

AbstractCredentials* WebViewPage::getCredentials() const {
    return new HttpCredentialsGui();
}

void WebViewPage::setConnected() {

}

WebViewPageUrlRequestInterceptor::WebViewPageUrlRequestInterceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent) {

}

void WebViewPageUrlRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    info.setHttpHeader("OCS-APIREQUEST", "true");
}

}
