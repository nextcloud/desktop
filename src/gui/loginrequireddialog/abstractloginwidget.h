#pragma once

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QList>
#include <QWidget>

namespace OCC {

class AbstractLoginWidget : public QWidget
{
protected:
    explicit AbstractLoginWidget(QWidget *parent = nullptr);
};

} // OCC
