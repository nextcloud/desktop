#include "pagination.h"

#include <QDebug>
#include <QRadioButton>

namespace OCC::Wizard {

Pagination::Pagination(QWidget *parent)
    : QWidget(parent)
    , _activePageIndex(0)
    , _enabled(true)
{
    // this class manages its own layout
    setLayout(new QHBoxLayout(this));
}

void Pagination::setEntries(const QStringList &newEntries)
{
    // TODO: more advanced implementation (reuse existing buttons within layout)
    // current active page is also lost that way
    removeAllItems();

    for (PageIndex i = 0; i < newEntries.count(); ++i) {
        auto entry = newEntries[i];

        auto newButton = new QRadioButton(entry, this);

        _entries.append(newButton);
        layout()->addWidget(newButton);

        connect(newButton, &QRadioButton::clicked, this, [this, i]() {
            emit paginationEntryClicked(i);
        });
    }

    enableOrDisableButtons();
}

// needed to clean up widgets we added to the layout
Pagination::~Pagination() noexcept
{
    removeAllItems();
}

void Pagination::removeAllItems()
{
    qDeleteAll(_entries);
}

PageIndex Pagination::entriesCount() const
{
    return _entries.size();
}

void Pagination::enableOrDisableButtons()
{
    for (PageIndex i = 0; i < _entries.count(); ++i) {
        const auto enabled = [=]() {
            if (_enabled) {
                return i < _activePageIndex;
            }

            return false;
        }();

        // TODO: use custom QRadioButton which doesn't need to be disabled to not be clickable
        // can only jump to pages we have visited before
        // to avoid resetting the current page, we don't want to enable the active page either
        _entries.at(i)->setEnabled(enabled);
    }
}

void Pagination::setActivePageIndex(PageIndex activePageIndex)
{
    _activePageIndex = activePageIndex;

    for (PageIndex i = 0; i < _entries.count(); ++i) {
        // we don't want to store those buttons in this object's state
        auto button = qobject_cast<QRadioButton *>(layout()->itemAt(i)->widget());
        button->setChecked(i == activePageIndex);
    }

    enableOrDisableButtons();
}

PageIndex Pagination::activePageIndex()
{
    return _activePageIndex;
}

}
