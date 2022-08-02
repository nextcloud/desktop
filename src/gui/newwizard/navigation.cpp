#include "navigation.h"

#include <QDebug>
#include <QRadioButton>

namespace OCC::Wizard {

Navigation::Navigation(QWidget *parent)
    : QWidget(parent)
{
    // this class manages its own layout
    setLayout(new QHBoxLayout(this));
}

void Navigation::setEntries(const QList<SetupWizardState> &newEntries)
{
    // TODO: more advanced implementation (reuse existing buttons within layout)
    // current active page is also lost that way
    removeAllItems();

    for (const auto state : newEntries) {
        const QString text = Utility::enumToDisplayName(state);

        auto newButton = new QRadioButton(text, this);

        _entries.insert(state, newButton);
        layout()->addWidget(newButton);

        connect(newButton, &QRadioButton::clicked, this, [this, state]() {
            emit paginationEntryClicked(state);
        });
    }

    enableOrDisableButtons();
}

// needed to clean up widgets we added to the layout
Navigation::~Navigation() noexcept
{
    removeAllItems();
}

void Navigation::removeAllItems()
{
    qDeleteAll(_entries);
}

void Navigation::enableOrDisableButtons()
{
    for (const auto state : _entries.keys()) {
        auto button = _entries.value(state);

        const auto enabled = [&state, this]() {
            if (_enabled) {
                return state < _activeState;
            }

            return false;
        }();

        // TODO: use custom QRadioButton which doesn't need to be disabled to not be clickable
        // can only jump to pages we have visited before
        // to avoid resetting the current page, we don't want to enable the active page either
        button->setEnabled(enabled);
    }
}

void Navigation::setActiveState(SetupWizardState newState)
{
    _activeState = newState;

    for (const auto key : _entries.keys()) {
        auto button = _entries.value(key);
        button->setChecked(key == _activeState);
    }

    enableOrDisableButtons();
}

}
