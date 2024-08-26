#include "setupwizardwidget.h"
#include "ui_setupwizardwidget.h"

#include "gui/application.h"
#include "gui/guiutility.h"
#include "gui/owncloudgui.h"
#include "gui/settingsdialog.h"
#include "resources/template.h"
#include "theme.h"

#include <QLabel>
#include <QMessageBox>
#include <QStyleFactory>

using namespace std::chrono_literals;

namespace {

using namespace OCC;

QString replaceCssColors()
{
    return Resources::Template::renderTemplateFromFile(QStringLiteral(":/client/resources/wizard/style.qss"),
        {
            {QStringLiteral("WIZARD_BACKGROUND_COLOR"), Theme::instance()->wizardHeaderBackgroundColor().name()}, //
            {QStringLiteral("WIZARD_FONT_COLOR"), Theme::instance()->wizardHeaderTitleColor().name()} //
        });
}

}

namespace OCC::Wizard {

Q_LOGGING_CATEGORY(lcSetupWizardWidget, "gui.setupwizard.window")

SetupWizardWidget::SetupWizardWidget(SettingsDialog *parent)
    : QWidget(parent)
    , _ui(new ::Ui::SetupWizardWidget)
{
    setWindowFlag(Qt::WindowCloseButtonHint, false);

    _ui->setupUi(this);
    _ui->backButton->setText(Utility::isMac() ? tr("Back") : tr("< &Back"));
    _ui->backButton->setAccessibleName(tr("Back"));

    slotHideErrorMessageWidget();

    connect(_ui->cancelButton, &QPushButton::clicked, this, [this] {
        auto messageBox = new QMessageBox(QMessageBox::Warning, tr("Cancel Setup"), tr("Do you really want to cancel the account setup?"),
            QMessageBox::Yes | QMessageBox::No, ocApp()->gui()->settingsDialog());
        messageBox->setAttribute(Qt::WA_DeleteOnClose);
        connect(messageBox, &QMessageBox::accepted, this, [this] {
            // call the base implementation
            Q_EMIT rejected();
        });
        ownCloudGui::raise();
        messageBox->open();
    });

    connect(_ui->nextButton, &QPushButton::clicked, this, &SetupWizardWidget::slotMoveToNextPage);

    connect(_ui->backButton, &QPushButton::clicked, this, [this]() {
        slotStartTransition();
        Q_EMIT backButtonClicked();
    });

    connect(_ui->navigation, &Navigation::paginationEntryClicked, this, [this](SetupWizardState clickedState) {
        slotStartTransition();
        Q_EMIT navigationEntryClicked(clickedState);
    });

    // different styles (e.g., 'Windows', 'Fusion') may require different approaches in the stylesheet
    // therefore we want to force a standard style on all platforms
    // this further makes sure the wizard (well, its contents) looks exactly the same on all platforms
    // Fusion should be available everywhere
    auto fusionStyle = QStyleFactory::create(QStringLiteral("Fusion"));
    if (OC_ENSURE(fusionStyle != nullptr)) {
        _ui->contentWidget->setStyle(fusionStyle);
    } else {
        qCDebug(lcSetupWizardWidget) << "Could not set up default style, wizard contents will be shown using default style";
    }

    loadStylesheet();

    _ui->transitionProgressIndicator->setFixedSize(32, 32);
    _ui->transitionProgressIndicator->setColor(Theme::instance()->wizardHeaderTitleColor());
}

void SetupWizardWidget::loadStylesheet()
{
    _ui->contentWidget->setStyleSheet(replaceCssColors());
}

void SetupWizardWidget::displayPage(AbstractSetupWizardPage *page, SetupWizardState state)
{
    _transitioning = false;
    _ui->backButton->setEnabled(true);
    _ui->nextButton->setEnabled(true);

    if (state == SetupWizardState::FirstState) {
        _ui->backButton->setEnabled(false);
    } else if (state == SetupWizardState::FinalState) {
        _ui->nextButton->setEnabled(false);
    }

    if (state == SetupWizardState::FinalState) {
        _ui->nextButton->setText(Utility::isMac() ? tr("Done") : tr("&Finish"));
        _ui->nextButton->setAccessibleName(Utility::isMac() ? tr("Done") : tr("Finish"));
    } else {
        _ui->nextButton->setText(Utility::isMac() ? tr("Continue") : tr("&Next >"));
        _ui->nextButton->setAccessibleName(Utility::isMac() ? tr("Continue") : tr("Next"));
    }

    _currentPage = page;
    slotReplaceContent(_currentPage);

    // initial check whether to enable the next button right away
    slotUpdateNextButton();

    _ui->navigation->setActiveState(state);
    _ui->navigation->setEnabled(true);

    connect(_ui->errorMessageDismissButton, &QPushButton::clicked, this, &SetupWizardWidget::slotHideErrorMessageWidget);

    // by default, set focus on the next button
    _ui->nextButton->setFocus();

    // bring to front if necessary
    ownCloudGui::raise();

    connect(_currentPage, &AbstractSetupWizardPage::contentChanged, this, &SetupWizardWidget::slotUpdateNextButton);

    QTimer::singleShot(0, [this]() {
        // this can optionally be overwritten by the page
        Q_EMIT _currentPage->pageDisplayed();
    });
}

void SetupWizardWidget::slotStartTransition()
{
    _transitioning = true;

    _ui->transitionProgressIndicator->startAnimation();
    _ui->contentWidget->setCurrentWidget(_ui->transitionPage);

    // until a new page is displayed by the controller, we want to prevent the user from initiating another page change
    _ui->backButton->setEnabled(false);
    _ui->nextButton->setEnabled(false);
    _ui->navigation->setEnabled(false);
    // also, we should assume the user has seen the error message in case one is shown
    slotHideErrorMessageWidget();
}

void SetupWizardWidget::slotReplaceContent(QWidget *newWidget)
{
    _ui->contentWidget->removeWidget(_currentContentWidget);

    _ui->contentWidget->addWidget(newWidget);
    _ui->contentWidget->setCurrentWidget(newWidget);

    _currentContentWidget = newWidget;

    // inheriting the style sheet from content widget doesn't work in all cases
    _currentContentWidget->setStyleSheet(_ui->contentWidget->styleSheet());
}

void SetupWizardWidget::slotHideErrorMessageWidget()
{
    _ui->errorMessageWidget->hide();
}

void SetupWizardWidget::showErrorMessage(const QString &errorMessage)
{
    _ui->errorMessageLabel->setText(QStringLiteral("<b>%1</b>").arg(errorMessage));
    _ui->errorMessageLabel->setWordWrap(true);
    _ui->errorMessageWidget->show();
}

void SetupWizardWidget::setNavigationEntries(const QList<SetupWizardState> &entries)
{
    _ui->navigation->setEntries(entries);
}

void SetupWizardWidget::slotUpdateNextButton()
{
    _ui->nextButton->setEnabled(_currentPage->validateInput());
}

void SetupWizardWidget::disableNextButton()
{
    _ui->nextButton->setEnabled(false);
}

SetupWizardWidget::~SetupWizardWidget() noexcept
{
    delete _ui;
}

void SetupWizardWidget::slotMoveToNextPage()
{
    if (OC_ENSURE(_currentPage->validateInput())) {
        slotStartTransition();
        Q_EMIT nextButtonClicked();
    }
}
}
