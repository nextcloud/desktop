/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "window.h"
#include "ui_window.h"

#include <QLineEdit>
#include <QSyntaxHighlighter>

class LogWindowHighlighter : public QSyntaxHighlighter
{
public:
    LogWindowHighlighter(QTextDocument *parent = 0);

protected:
    void highlightBlock(const QString &text) Q_DECL_OVERRIDE;
    void highlightHelper(const QString& text, const QTextCharFormat &format, const QString &exp);
};

Window::Window(QIODevice *dev, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Window)
  , device(dev)
{
    ui->setupUi(this);
    connect(ui->inputEdit->lineEdit(), SIGNAL(returnPressed()), SLOT(handleReturn()));
    addDefaultItems();
    QFont f("Courier");
    f.setStyleHint(QFont::Monospace);
    ui->outputs->setFont(f);
    new LogWindowHighlighter(ui->outputs->document());
}

Window::~Window()
{
    delete ui;
}

void Window::receive()
{
    QByteArray ba = device->readAll();
    ui->outputs->insertPlainText(QString::fromLatin1(ba));
}

void Window::receiveError(QLocalSocket::LocalSocketError error)
{
    qDebug() << "Error connecting to socket:" << error;
}

void Window::handleReturn()
{
    QString cmd = ui->inputEdit->currentText()+QLatin1Char('\n');
    ui->outputs->insertPlainText(cmd);
    device->write(cmd.toLatin1());
    ui->inputEdit->lineEdit()->clear();
}

void Window::addDefaultItems()
{
    QStringList commands;
    commands << "RETRIEVE_FOLDER_STATUS:" << "RETRIEVE_FILE_STATUS:";
    ui->inputEdit->addItems(commands);
}


LogWindowHighlighter::LogWindowHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{

}

void LogWindowHighlighter::highlightBlock(const QString &text)
{
    QTextCharFormat responseKeywordFormat;
    responseKeywordFormat.setFontWeight(QFont::Bold);
    responseKeywordFormat.setForeground(Qt::darkMagenta);
    QString responsePattern = "(UPDATE_VIEW|BROADCAST:|STATUS:)";
    highlightHelper(text, responseKeywordFormat, responsePattern);

    QTextCharFormat messageKeywordFormat;
    messageKeywordFormat.setFontWeight(QFont::Bold);
    messageKeywordFormat.setForeground(Qt::darkYellow);
    QString messagePattern = "(RETRIEVE_(FOLDER|FILE)_STATUS):";
    highlightHelper(text, messageKeywordFormat, messagePattern);
}

void LogWindowHighlighter::highlightHelper(const QString& text, const QTextCharFormat &format,
                                           const QString &pattern)
{
    QRegExp rex(pattern);
    int index = text.indexOf(rex);
    if (index >= 0) {
        int len = rex.matchedLength();
        setFormat(index, len, format);
        int lastMatchedEnd=index+len;
        int secondDoubleDotIndex = text.indexOf(':', lastMatchedEnd);
        if (secondDoubleDotIndex >= 0) {
            QTextCharFormat boldFormat;
            boldFormat.setFontWeight(QFont::Bold);
            setFormat(lastMatchedEnd, secondDoubleDotIndex-lastMatchedEnd, boldFormat);
        }
    }

}
