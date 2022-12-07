/*
 * Copyright (C) by Erik Verbruggen <erik@verbruggen.consulting>
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

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <iostream>
#include <vector>

#include <libsync/filesystem.h>


/*
 * IMPORTANT: the commands below do NOT read any data from files when modifying them!
 *
 * When a file is dehydrated, reading will cause a re-hydration (download), and many tests check
 * for the number of network requests.
 */

using namespace std;

namespace {
bool writeToFile(std::string_view command, const QString &fileName, QFile::OpenMode mode, const QByteArray &data)
{
    QFile f(fileName);
    if (!f.open(mode)) {
        cerr << "Error: cannot open file '" << qPrintable(fileName) << "' for " << command << " command: "
             << qPrintable(f.errorString()) << endl;
        return false;
    }
    const auto written = f.write(data);

    if (mode & QFile::Append) {
        if (!f.seek(f.size())) {
            cerr << "Error: cannot seek to EOF in '" << qPrintable(fileName) << "' for " << command << " command" << endl;
            return false;
        }
    }

    if (written != data.size()) {
        cerr << "Error: wrote " << written << " bytes to '" << qPrintable(fileName) << "' instead of requested " << data.size() << " bytes" << endl;
        return false;
    }
    f.close();
    return true;
}
}

/**
 * @brief The abstract Command class. You know, from the pattern.
 */
class Command
{
public:
    Command(const QString &fileName)
        : _fileName(fileName)
    {
    }
    virtual ~Command() = 0;

    virtual bool execute(QDir &rootDir) const = 0;

protected:
    static QString parseFileName(QStringListIterator &it)
    {
        return it.next();
    }

    const QString _fileName;
};

Command::~Command() { }

class SetMtimeCommand : public Command
{
public:
    static constexpr string_view name = "mtime";

    SetMtimeCommand(const QString &fileName, qlonglong secs)
        : Command(fileName)
        , _secs(secs)
    {
    }
    ~SetMtimeCommand() override { }

    bool execute(QDir &rootDir) const override
    {
        auto modTime = QDateTime::fromSecsSinceEpoch(_secs);
        cerr << name << ", file: " << qPrintable(_fileName) << ", secs: " << _secs << endl;
        return OCC::FileSystem::setModTime(rootDir.filePath(_fileName), OCC::Utility::qDateTimeToTime_t(modTime));
    }

    static Command *parse(QStringListIterator &it)
    {
        QString fileName = parseFileName(it);
        if (fileName.isEmpty()) {
            cerr << "Error: invalid filename for " << name << " command" << endl;
            return {};
        }

        QString secsStr = it.next();
        bool ok = false;
        auto secs = secsStr.toLongLong(&ok);
        if (!ok) {
            cerr << "Error: '" << qPrintable(secsStr) << "' is not a valid number (" << name << ")" << endl;
            return {};
        }

        return new SetMtimeCommand(fileName, secs);
    }

private:
    const qlonglong _secs;
};

class SetContentsCommand : public Command
{
public:
    static constexpr string_view name = "contents";

    SetContentsCommand(const QString &fileName, qlonglong count, char ch)
        : Command(fileName)
        , _count(count)
        , _ch(ch)
    {
    }
    ~SetContentsCommand() override { }

    bool execute(QDir &rootDir) const override
    {
        cerr << name << endl;
        int count = _count == -1 ? 32 : _count;
        return writeToFile(name, rootDir.filePath(_fileName), QIODevice::WriteOnly | QIODevice::Truncate, QByteArray(count, _ch));
    }

    static Command *parse(QStringListIterator &it)
    {
        QString fileName = parseFileName(it);
        if (fileName.isEmpty()) {
            cerr << "Error: invalid filename for " << name << " command" << endl;
            return {};
        }

        QString countStr = it.next();
        bool ok = false;
        auto count = countStr.toLongLong(&ok);
        if (!ok) {
            cerr << "Error: '" << qPrintable(countStr) << "' is not a valid number (" << name << ")" << endl;
            return {};
        }

        QString charStr = it.next();
        if (charStr.size() != 1) {
            cerr << "Error: content for " << name << " command should be 1 character in size" << endl;
        }

        return new SetContentsCommand(fileName, count, charStr.at(0).toLatin1());
    }

private:
    const qlonglong _count;
    const char _ch;
};

class RenameCommand : public Command
{
public:
    static constexpr string_view name = "rename";

    RenameCommand(const QString &fileName, const QString &newName)
        : Command(fileName)
        , _newName(newName)
    {
    }
    ~RenameCommand() override { }

    bool execute(QDir &rootDir) const override
    {
        cerr << name << endl;
        if (!rootDir.exists(_fileName)) {
            cerr << "File does not exist: " << qPrintable(rootDir.absoluteFilePath(_fileName)) << endl;
            return false;
        }

        bool success = rootDir.rename(_fileName, _newName);
        if (!success) {
            cerr << "Rename of " << qPrintable(_fileName) << " failed" << endl;
        }
        return success;
    }

    static Command *parse(QStringListIterator &it)
    {
        QString fileName = parseFileName(it);
        if (fileName.isEmpty()) {
            cerr << "Error: invalid filename for " << name << " command" << endl;
            return {};
        }

        QString newName = it.next();
        if (newName.isEmpty()) {
            cerr << "Error: invalid new name for " << name << " command" << endl;
            return {};
        }

        return new RenameCommand(fileName, newName);
    }

private:
    const QString _newName;
};

class AppendByteCommand : public Command
{
public:
    static constexpr string_view name = "appendbyte";

    AppendByteCommand(const QString &fileName, char ch)
        : Command(fileName)
        , _ch(ch)
    {
    }
    ~AppendByteCommand() override { }

    bool execute(QDir &rootDir) const override
    {
        cerr << name << endl;

        if (_ch == '\0') {
            cerr << "Error: appending a NUL byte is probably a failure somewhere else." << endl;
            return false;
        }
        cerr << ".... file: " << qPrintable(_fileName) << ", byte: " << _ch << endl;
        return writeToFile(name, rootDir.filePath(_fileName), QIODevice::WriteOnly | QIODevice::Append, QByteArray(1, _ch));
        ;
    }

    static Command *parse(QStringListIterator &it)
    {
        QString fileName = parseFileName(it);
        if (fileName.isEmpty()) {
            cerr << "Error: invalid filename for " << name << " command" << endl;
            return {};
        }

        QString charStr = it.next();
        if (charStr.size() != 1) {
            cerr << "Error: content for " << name << " command should be 1 character in size" << endl;
        }

        return new AppendByteCommand(fileName, charStr.at(0).toLatin1());
    }

private:
    const char _ch;
};

class InsertCommand : public Command
{
public:
    static constexpr string_view name = "insert";

    InsertCommand(const QString &fileName, qlonglong count, char ch)
        : Command(fileName)
        , _count(count)
        , _ch(ch)
    {
    }
    ~InsertCommand() override { }

    bool execute(QDir &rootDir) const override
    {
        cerr << name << " '" << qPrintable(_fileName) << "' with "
             << _count << " " << _ch << " characters" << endl;
        if (QFileInfo::exists(rootDir.filePath(_fileName))) {
            cerr << "Error: file '" << qPrintable(_fileName) << "' for " << name << " command already exists" << endl;
            return false;
        }

        return writeToFile(name, rootDir.filePath(_fileName), QIODevice::WriteOnly, QByteArray(_count, _ch));
    }

    static Command *parse(QStringListIterator &it)
    {
        QString fileName = parseFileName(it);
        if (fileName.isEmpty()) {
            cerr << "Error: invalid filename for " << name << " command" << endl;
            return {};
        }

        QString countStr = it.next();
        bool ok = false;
        auto count = countStr.toLongLong(&ok);
        if (!ok) {
            cerr << "Error: '" << qPrintable(countStr) << "' is not a valid number (" << name << ")" << endl;
            return {};
        }

        QString charStr = it.next();
        if (charStr.size() != 1) {
            cerr << "Error: content for " << name << " command should be 1 character in size" << endl;
        }

        return new InsertCommand(fileName, count, charStr.at(0).toLatin1());
    }

private:
    const qlonglong _count;
    const char _ch;
};

class RemoveCommand : public Command
{
public:
    static constexpr string_view name = "remove";

    RemoveCommand(const QString &fileName)
        : Command(fileName)
    {
    }
    ~RemoveCommand() override { }

    bool execute(QDir &rootDir) const override
    {
        cerr << name << endl;
        QFileInfo fi(rootDir.filePath(_fileName));
        if (fi.isFile()) {
            return rootDir.remove(_fileName);
        } else {
            return QDir(fi.filePath()).removeRecursively();
        }
    }

    static Command *parse(QStringListIterator &it)
    {
        QString fileName = parseFileName(it);
        if (fileName.isEmpty()) {
            cerr << "Error: invalid filename for " << name << " command" << endl;
            return {};
        }

        return new RemoveCommand(fileName);
    }
};

class MkdirCommand : public Command
{
public:
    static constexpr string_view name = "mkdir";

    MkdirCommand(const QString &fileName)
        : Command(fileName)
    {
    }
    ~MkdirCommand() override { }

    bool execute(QDir &rootDir) const override
    {
        cerr << name << " " << qPrintable(_fileName) << endl;
        return rootDir.mkdir(_fileName);
    }

    static Command *parse(QStringListIterator &it)
    {
        QString fileName = parseFileName(it);
        if (fileName.isEmpty()) {
            cerr << "Error: invalid directory name for " << name << " command" << endl;
            return {};
        }

        return new MkdirCommand(fileName);
    }
};

/**
 * @brief parseArguments Creates a list of commands to be executed.
 *
 * Each command's `parse` method reads the required arguments from the command-line by using the
 * iterator.
 *
 * @param it The iterator over the argument list
 * @return a list of commands to execute
 */
vector<Command *> parseArguments(QStringListIterator &it)
{
    map<string_view, function<Command *(QStringListIterator &)>> parserFunctions = {
        { SetMtimeCommand::name, SetMtimeCommand::parse },
        { SetContentsCommand::name, SetContentsCommand::parse },
        { RenameCommand::name, RenameCommand::parse },
        { AppendByteCommand::name, AppendByteCommand::parse },
        { InsertCommand::name, InsertCommand::parse },
        { RemoveCommand::name, RemoveCommand::parse },
        { MkdirCommand::name, MkdirCommand::parse },
    };

    vector<Command *> commands;

    while (it.hasNext()) {
        const QString option = it.next();
        auto pf = parserFunctions.find(option.toStdString());
        if (pf == parserFunctions.end()) {
            cerr << "Error: unknown command '" << qPrintable(option) << "'" << endl;
            return {};
        }

        if (auto cmd = (pf->second)(it)) {
            commands.emplace_back(cmd);
        } else {
            return {};
        }
    }

    return commands;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringListIterator it(app.arguments());
    if (it.hasNext()) {
        // skip program name
        it.next();
    }

    QDir rootDir(it.next());

    const auto commands = parseArguments(it);
    if (commands.empty()) {
        return -1;
    }

    cerr << "Starting executing commands in '" << qPrintable(rootDir.absolutePath()) << "' ...:" << endl;

    for (const auto &cmd : commands) {
        cerr << ".. Executing command: ";
        if (!cmd->execute(rootDir)) {
            return -2;
        }
        cerr << ".. command done." << endl;
    }

    cerr << "Successfully executed all commands." << endl;

    qDeleteAll(commands);

    return 0;
}
