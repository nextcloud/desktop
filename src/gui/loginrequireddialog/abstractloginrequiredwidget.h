#pragma once

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QList>
#include <QWidget>

namespace OCC {

class AbstractLoginRequiredWidget : public QWidget
{
public:
    /**
     * Buttons to add to the parent dialog's lower right corner.
     * @return list of buttons
     */
    virtual QList<QPair<QAbstractButton *, QDialogButtonBox::ButtonRole>> buttons() = 0;

protected:
    explicit AbstractLoginRequiredWidget(QWidget *parent = nullptr);
};

} // OCC
