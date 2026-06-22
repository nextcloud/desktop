/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
