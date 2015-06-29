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

#ifndef PROTOCOLWIDGET_H
#define PROTOCOLWIDGET_H

#include <QDialog>
#include <QDateTime>
#include <QLocale>

#include "progressdispatcher.h"

#include "ui_protocolwidget.h"

class QPushButton;

namespace OCC {
class SyncResult;

namespace Ui {
  class ProtocolWidget;
}
class Application;

/**
 * @brief The ProtocolWidget class
 * @ingroup gui
 */
class ProtocolWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProtocolWidget(QWidget *parent = 0);
    ~ProtocolWidget();

public slots:
    void slotProgressInfo( const QString& folder, const ProgressInfo& progress );
    void slotOpenFile( QTreeWidgetItem* item, int );

protected slots:
    void copyToClipboard();
    void slotRetrySync();

protected:
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

signals:
    void guiLog(const QString&, const QString&);

private:
    void setSyncResultStatus(const SyncResult& result );
    void cleanIgnoreItems( const QString& folder );
    void computeResyncButtonEnabled();
    QString fixupFilename( const QString& name );


    QTreeWidgetItem* createCompletedTreewidgetItem(const QString &folder, const SyncFileItem &item );

    QString timeString(QDateTime dt, QLocale::FormatType format = QLocale::NarrowFormat) const;

    const int IgnoredIndicatorRole;
    Ui::ProtocolWidget *_ui;
    QPushButton *_retrySyncBtn;
    QPushButton *_copyBtn;
};

}
#endif // PROTOCOLWIDGET_H
