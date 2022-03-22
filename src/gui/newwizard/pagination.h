#pragma once

#include <QHBoxLayout>
#include <QRadioButton>
#include <QString>
#include <QWidget>

namespace OCC::Wizard {

using PageIndex = QStringList::size_type;

/**
 * Renders pagination entries as radio buttons in a horizontal layout.
 */
class Pagination : public QWidget
{
    Q_OBJECT

public:
    explicit Pagination(QWidget *parent = nullptr);

    ~Pagination() noexcept override;

    void setEntries(const QStringList &newEntries);

    [[nodiscard]] PageIndex entriesCount() const;

    PageIndex activePageIndex();

Q_SIGNALS:
    void paginationEntryClicked(PageIndex clickedPageIndex);

public Q_SLOTS:
    void setActivePageIndex(PageIndex activePageIndex);

private:
    void removeAllItems();
    void enableOrDisableButtons();

    QList<QRadioButton *> _entries;
    PageIndex _activePageIndex;
    bool _enabled;
};

}
