/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef LOGBROWSER_H
#define LOGBROWSER_H

#include <QCheckBox>
#include <QPlainTextEdit>
#include <QTextStream>
#include <QFile>
#include <QObject>
#include <QList>
#include <QDateTime>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace OCC {

/**
 * @brief The LogBrowser class
 * @ingroup gui
 */
class LogBrowser : public QDialog
{
    Q_OBJECT
public:
    explicit LogBrowser(QWidget *parent = nullptr);
    ~LogBrowser() override;

protected:
    void closeEvent(QCloseEvent *) override;

protected Q_SLOTS:
    void togglePermanentLogging(bool enabled);
};

} // namespace

#endif // LOGBROWSER_H
