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
#include <QDebug>
#include <QSettings>

#include "mirall/mirallconfigfile.h"
#include "mirall/logger.h"

namespace Mirall {

// ==============================================================================

LogWidget::LogWidget(QWidget *parent)
    :QPlainTextEdit(parent)
{
    setReadOnly( true );
    QFont font;
    font.setFamily(QLatin1String("Courier New"));
    font.setFixedPitch(true);
    document()->setDefaultFont( font );

    MirallConfigFile cfg;
    int lines = cfg.maxLogLines();
    // qDebug() << "#        ##  Have " << lines << " Loglines!";
    document()->setMaximumBlockCount( lines );
}

// ==============================================================================

LogBrowser::LogBrowser(QWidget *parent) :
    QDialog(parent),
    _logWidget( new LogWidget(parent) ),
    _doFileFlush(false),
    _logstream(0)
{
    setObjectName("LogBrowser"); // for save/restoreGeometry()
    setWindowTitle(tr("Log Output"));
    setMinimumWidth(600);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    // mainLayout->setMargin(0);

    mainLayout->addWidget( _logWidget );

    QHBoxLayout *toolLayout = new QHBoxLayout;
    mainLayout->addLayout( toolLayout );

    // Search input field
    QLabel *lab = new QLabel(tr("&Search: "));
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

    // Direct connection for log comming from this thread, and queued for the one in a different thread
    connect(Logger::instance(), SIGNAL(newLog(QString)),this,SLOT(slotNewLog(QString)), Qt::AutoConnection);

    MirallConfigFile cfg;
    cfg.restoreGeometry(this);
}

LogBrowser::~LogBrowser()
{
}

void LogBrowser::slotNewLog( const QString& msg )
{
    if( _logWidget->isVisible() ) {
        _logWidget->appendPlainText( msg );
    }

    if( _logstream ) {
        (*_logstream) << msg << endl;
        if( _doFileFlush ) _logstream->flush();
    }
}

void LogBrowser::setLogFile( const QString & name, bool flush )
{
    if( _logstream ) {
        _logFile.close();
    }
    _logFile.setFileName( name );

    if(!_logFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(
                    this,
                    tr("Error"),
                    QString(tr("<nobr>File '%1'<br/>cannot be opened for writing.<br/><br/>"
                               "The log output can <b>not</b> be saved!</nobr>"))
                    .arg(name));
        return;
    }
    _doFileFlush = flush;
    _logstream.reset(new QTextStream( &_logFile ));
}

void LogBrowser::slotFind()
{
    QString searchText = _findTermEdit->text();

    if( searchText.isEmpty() ) return;

    search( searchText );
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
            QMessageBox::critical(this, tr("Error"), tr("Could not write to log file ")+ saveFile);
        }
    }
    _saveBtn->setEnabled(true);

}

void LogBrowser::slotClearLog()
{
    _logWidget->clear();
}

void LogBrowser::closeEvent(QCloseEvent *event)
{
    MirallConfigFile cfg;
    cfg.saveGeometry(this);
    QWidget::closeEvent(event);
}

} // namespace
