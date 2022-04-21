#include "setupwizardwindow.h"

#include <QLabel>

#include "gui/owncloudgui.h"
#include "ui_setupwizardwindow.h"

namespace OCC::Wizard {

SetupWizardWindow::SetupWizardWindow(QWidget *parent)
    : QDialog(parent)
    , _ui(new ::Ui::SetupWizardWindow)
{
    _ui->setupUi(this);

    slotHideErrorMessageWidget();

    // cannot do this in Qt Designer
    _ui->contentWidget->layout()->setAlignment(Qt::AlignCenter);

    connect(_ui->cancelButton, &QPushButton::clicked, this, &SetupWizardWindow::reject);

    connect(_ui->nextButton, &QPushButton::clicked, this, &SetupWizardWindow::slotMoveToNextPage);

    connect(_ui->backButton, &QPushButton::clicked, this, [this]() {
        slotStartTransition();
        emit backButtonClicked(_ui->pagination->activePageIndex());
    });

    connect(_ui->pagination, &Pagination::paginationEntryClicked, this, [this](PageIndex clickedPageIndex) {
        slotStartTransition();
        emit paginationEntryClicked(_ui->pagination->activePageIndex(), clickedPageIndex);
    });

    _ui->transitionProgressIndicator->setFixedSize(32, 32);

    // handle user pressing enter/return key
    installEventFilter(this);
}

void SetupWizardWindow::displayPage(AbstractSetupWizardPage *page, PageIndex index)
{
    _transitioning = false;
    _ui->backButton->setEnabled(true);
    _ui->nextButton->setEnabled(true);

    if (index == 0) {
        _ui->backButton->setEnabled(false);
    } else if (index == _ui->pagination->entriesCount()) {
        _ui->nextButton->setEnabled(false);
    }

    if (index >= (_ui->pagination->entriesCount() - 1)) {
        _ui->nextButton->setText(tr("Finish"));
    } else {
        _ui->nextButton->setText(tr("Next >"));
    }

    _currentPage = page;
    slotReplaceContent(_currentPage);

    _ui->pagination->setActivePageIndex(index);
    _ui->pagination->setEnabled(true);

    connect(_ui->errorMessageDismissButton, &QPushButton::clicked, this, &SetupWizardWindow::slotHideErrorMessageWidget);

    // by default, set focus on the next button
    _ui->nextButton->setFocus();

    // bring to front if necessary
    ownCloudGui::raiseDialog(this);

    // this can optionally be overwritten by the page
    Q_EMIT _currentPage->pageDisplayed();
}

void SetupWizardWindow::slotStartTransition()
{
    _ui->transitionProgressIndicator->startAnimation();
    _ui->contentWidget->setCurrentWidget(_ui->transitionProgressIndicator);

    // until a new page is displayed by the controller, we want to prevent the user from initiating another page change
    _ui->backButton->setEnabled(false);
    _ui->nextButton->setEnabled(false);
    _ui->pagination->setEnabled(false);
    // also, we should assume the user has seen the error message in case one is shown
    slotHideErrorMessageWidget();
}

void SetupWizardWindow::slotReplaceContent(QWidget *newWidget)
{
    _ui->contentWidget->removeWidget(_currentContentWidget);

    _ui->contentWidget->addWidget(newWidget);
    _ui->contentWidget->setCurrentWidget(newWidget);

    _currentContentWidget = newWidget;
}

void SetupWizardWindow::slotHideErrorMessageWidget()
{
    _ui->errorMessageWidget->hide();
}

void SetupWizardWindow::showErrorMessage(const QString &errorMessage)
{
    _ui->errorMessageLabel->setText(errorMessage);
    _ui->errorMessageWidget->show();
}

void SetupWizardWindow::setPaginationEntries(const QStringList &paginationEntries)
{
    _ui->pagination->setEntries(paginationEntries);
}

bool SetupWizardWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!_transitioning && (obj == _currentPage.data() || obj == this)) {
        if (event->type() == QEvent::KeyPress) {
            auto keyEvent = dynamic_cast<QKeyEvent *>(event);

            switch (keyEvent->key()) {
            case Qt::Key_Enter:
                Q_FALLTHROUGH();
            case Qt::Key_Return:
                slotMoveToNextPage();
                return true;
            default:
                // no action required, give other handlers a chance
                break;
            }
        }
    }

    return QDialog::eventFilter(obj, event);
}

void SetupWizardWindow::disableNextButton()
{
    _ui->nextButton->setEnabled(false);
}

SetupWizardWindow::~SetupWizardWindow() noexcept
{
    delete _ui;
}

void SetupWizardWindow::slotMoveToNextPage()
{
    slotStartTransition();
    emit nextButtonClicked(_ui->pagination->activePageIndex());
}
}
