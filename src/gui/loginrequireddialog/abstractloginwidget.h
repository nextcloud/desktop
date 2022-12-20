#pragma once

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QList>
#include <QWidget>

namespace OCC {

class AbstractLoginWidget : public QWidget
{
    Q_OBJECT

Q_SIGNALS:
    /**
     * Emitted whenever the content widgets on the page have their content changed. Useful in cases where changes
     * cannot just be detected with an event filter, e.g., when a user pastes data using the context menu.
     */
    void contentChanged();

protected:
    explicit AbstractLoginWidget(QWidget *parent = nullptr);
};

} // OCC
