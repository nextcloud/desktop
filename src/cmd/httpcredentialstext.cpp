/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "httpcredentialstext.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <iostream>


namespace {

class EchoDisabler
{
public:
    EchoDisabler()
    {
#ifdef Q_OS_WIN
        hStdin = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(hStdin, &mode);
        SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
#else
        tcgetattr(STDIN_FILENO, &tios);
        termios tios_new = tios;
        tios_new.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &tios_new);
#endif
    }

    ~EchoDisabler()
    {
#ifdef Q_OS_WIN
        SetConsoleMode(hStdin, mode);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &tios);
#endif
    }

private:
#ifdef Q_OS_WIN
    DWORD mode = 0;
    HANDLE hStdin;
#else
    termios tios;
#endif
};

QString queryPassword(const QString &user)
{
    EchoDisabler disabler;
    std::cout << "Password for user " << qPrintable(user) << ": ";
    std::string s;
    std::getline(std::cin, s);
    return QString::fromStdString(s);
}
}

HttpCredentialsText::HttpCredentialsText(const QString &user, const QString &password)
    : OCC::HttpCredentials(OCC::DetermineAuthTypeJob::AuthType::Basic, user, password)
{
    if (user.isEmpty()) {
        qFatal("Invalid credentials: Username is empty");
    }
    if (password.isEmpty()) {
        qFatal("Invalid credentials: Password is empty");
    }
}

HttpCredentialsText *HttpCredentialsText::create(bool interactive, const QString &_user, const QString &_password)
{
    QString user = _user;
    QString password = _password;

    if (interactive) {
        if (user.isEmpty()) {
            std::cout << "Please enter user name: ";
            std::string s;
            std::getline(std::cin, s);
            user = QString::fromStdString(s);
        }
        if (password.isEmpty()) {
            password = queryPassword(user);
        }
    }
    return new HttpCredentialsText(user, password);
}

void HttpCredentialsText::askFromUser()
{
    // only called from account state
    Q_UNREACHABLE();
}
