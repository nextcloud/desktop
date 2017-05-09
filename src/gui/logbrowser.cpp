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

#include "logbrowser.h"

#include "stdio.h"
#include <iostream>

#include <QDialogButtonBox>
#include <QTextDocument>
#include <QLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QDir>
#include <QTextStream>
#include <QMessageBox>
#include <QCoreApplication>
#include <QSettings>
#include <QAction>

#include "configfile.h"
#include "logger.h"

namespace OCC {

// ==============================================================================

LogWidget::LogWidget(QWidget *parent)
    :QPlainTextEdit(parent)
{
    setReadOnly( true );
    QFont font;
    font.setFamily(QLatin1String("Courier New"));
    font.setFixedPitch(true);
    document()->setDefaultFont( font );
}

// ==============================================================================

LogBrowser::LogBrowser(QWidget *parent) :
    QDialog(parent),
    _logWidget( new LogWidget(parent) )
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setObjectName("LogBrowser"); // for save/restoreGeometry()
    setWindowTitle(tr("Log Output"));
    setMinimumWidth(600);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    // mainLayout->setMargin(0);

    mainLayout->addWidget( _logWidget );

    QHBoxLayout *toolLayout = new QHBoxLayout;
    mainLayout->addLayout( toolLayout );

    // Search input field
    QLabel *lab = new QLabel(tr("&Search:") + " ");
    _findTermEdit = new QLineEdit;
    lab->setBuddy( _findTermEdit );
    toolLayout->addWidget(lab);
    toolLayout->addWidget( _findTermEdit );

    // find button
    QPushButton *findBtn = new QPushButton;
    findBtn->setText( tr("&Find") );
    connect( findBtn, SIGNAL(clicked()), this, SLOT(slotFind()));
    toolLayout->addWidget( findBtn );

    // stretch
    toolLayout->addStretch(1);
    _statusLabel = new QLabel;
    toolLayout->addWidget( _statusLabel );
    toolLayout->addStretch(5);

    // Debug logging
    _logDebugCheckBox = new QCheckBox(tr("&Capture debug messages") + " ");
    connect(_logDebugCheckBox, SIGNAL(stateChanged(int)), SLOT(slotDebugCheckStateChanged(int)));
    toolLayout->addWidget( _logDebugCheckBox );

    QDialogButtonBox *btnbox = new QDialogButtonBox;
    QPushButton *closeBtn = btnbox->addButton( QDialogButtonBox::Close );
    connect(closeBtn,SIGNAL(clicked()),this,SLOT(close()));

    mainLayout->addWidget( btnbox );

    // clear button
    _clearBtn = new QPushButton;
    _clearBtn->setText( tr("Clear") );
    _clearBtn->setToolTip( tr("Clear the log display.") );
    btnbox->addButton(_clearBtn, QDialogButtonBox::ActionRole);
    connect( _clearBtn, SIGNAL(clicked()), this, SLOT(slotClearLog()));

    // save Button
    _saveBtn = new QPushButton;
    _saveBtn->setText( tr("S&ave") );
    _saveBtn->setToolTip(tr("Save the log file to a file on disk for debugging."));
    btnbox->addButton(_saveBtn, QDialogButtonBox::ActionRole);
    connect( _saveBtn, SIGNAL(clicked()),this, SLOT(slotSave()));

    setLayout( mainLayout );

    setModal(false);

    Logger::instance()->setLogWindowActivated(true);
    // Direct connection for log coming from this thread, and queued for the one in a different thread
    connect(Logger::instance(), SIGNAL(logWindowLog(QString)),this,SLOT(slotNewLog(QString)), Qt::AutoConnection);

    QAction *showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, SIGNAL(triggered()), SLOT(close()));
    addAction(showLogWindow);

    ConfigFile cfg;
    cfg.restoreGeometry(this);
    int lines = cfg.maxLogLines();
    _logWidget->document()->setMaximumBlockCount( lines );

}

LogBrowser::~LogBrowser()
{
}

void LogBrowser::showEvent(QShowEvent *)
{
    // This could have been changed through the --logdebug argument passed through the single application.
    _logDebugCheckBox->setCheckState(Logger::instance()->logDebug() ? Qt::Checked : Qt::Unchecked);
}

void LogBrowser::closeEvent(QCloseEvent *)
{
    ConfigFile cfg;
    cfg.saveGeometry(this);
}


void LogBrowser::slotNewLog( const QString& msg )
{
    if( _logWidget->isVisible() ) {
        _logWidget->appendPlainText( msg );
    }
}


void LogBrowser::slotFind()
{
    QString searchText = _findTermEdit->text();

    if( searchText.isEmpty() ) return;

    search( searchText );
}

void LogBrowser::slotDebugCheckStateChanged(int checkState)
{
    Logger::instance()->setLogDebug(checkState == Qt::Checked);
}

void LogBrowser::search( const QString& str )
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    _logWidget->moveCursor(QTextCursor::Start);
    QColor color = QColor(Qt::gray).lighter(130);
    _statusLabel->clear();

    while(_logWidget->find(str))
    {
        QTextEdit::ExtraSelection extra;
        extra.format.setBackground(color);

        extra.cursor = _logWidget->textCursor();
        extraSelections.append(extra);
    }

    QString stat = QString::fromLatin1("Search term %1 with %2 search results.").arg(str).arg(extraSelections.count());
    _statusLabel->setText(stat);

    _logWidget->setExtraSelections(extraSelections);
}

void LogBrowser::slotSave()
{
    _saveBtn->setEnabled(false);

    QString saveFile = QFileDialog::getSaveFileName( this, tr("Save log file"), QDir::homePath() );

    if( ! saveFile.isEmpty() ) {
        QFile file(saveFile);

        if (file.open(QIODevice::WriteOnly)) {
            QTextStream stream(&file);
            stream << _logWidget->toPlainText();
            file.close();
        } else {
            QMessageBox::critical(this, tr("Error"), tr("Could not write to log file %1").arg(saveFile));
        }
    }
    _saveBtn->setEnabled(true);

}

void LogBrowser::slotClearLog()
{
    _logWidget->clear();
}

} // namespace
