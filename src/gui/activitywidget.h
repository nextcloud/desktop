/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef ACTIVITYWIDGET_H
#define ACTIVITYWIDGET_H

#include <QDialog>
#include <QDateTime>
#include <QLocale>
#include <QAbstractListModel>

#include "progressdispatcher.h"
#include "owncloudgui.h"

#include "ui_activitywidget.h"

class QPushButton;

namespace OCC {

namespace Ui {
  class ActivityWidget;
}
class Application;

/**
 * @brief The ActivityListModel
 * @ingroup gui
 */

class ActivityListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ActivityListModel(QWidget *parent=0);

    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;

};

/**
 * @brief The ActivityWidget class
 * @ingroup gui
 */
class ActivityWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ActivityWidget(QWidget *parent = 0);
    ~ActivityWidget();
    QSize sizeHint() const { return ownCloudGui::settingsDialogSize(); }

public slots:
    void slotOpenFile();

protected slots:
    void copyToClipboard();

protected:

signals:
    void guiLog(const QString&, const QString&);

private:
    QString timeString(QDateTime dt, QLocale::FormatType format) const;
    Ui::ActivityWidget *_ui;
    QPushButton *_copyBtn;
};

}
#endif // ActivityWIDGET_H
