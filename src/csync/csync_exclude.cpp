/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
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

#include "config_csync.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "c_lib.h"
#include "c_private.h"
#include "c_utf8.h"

#include "csync_exclude.h"
#include "csync_misc.h"

#include "common/utility.h"
#include "../version.h"

#include <QString>
#include <QFileInfo>
#include <QFile>
#include <QDir>


/** Expands C-like escape sequences (in place)
 */
static void csync_exclude_expand_escapes(QByteArray &input)
{
    size_t o = 0;
    char *line = input.data();
    auto len = input.size();
    for (int i = 0; i < len; ++i) {
        if (line[i] == '\\') {
            // at worst input[i+1] is \0
            switch (line[i+1]) {
            case '\'': line[o++] = '\''; break;
            case '"': line[o++] = '"'; break;
            case '?': line[o++] = '?'; break;
            case '#': line[o++] = '#'; break;
            case 'a': line[o++] = '\a'; break;
            case 'b': line[o++] = '\b'; break;
            case 'f': line[o++] = '\f'; break;
            case 'n': line[o++] = '\n'; break;
            case 'r': line[o++] = '\r'; break;
            case 't': line[o++] = '\t'; break;
            case 'v': line[o++] = '\v'; break;
            default:
                // '\*' '\?' '\[' '\\' will be processed during regex translation
                // '\\' is intentionally not expanded here (to avoid '\\*' and '\*'
                // ending up meaning the same thing)
                line[o++] = line[i];
                line[o++] = line[i + 1];
                break;
            }
            ++i;
        } else {
            line[o++] = line[i];
        }
    }
    input.resize(o);
}

// See http://support.microsoft.com/kb/74496 and
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
// Additionally, we ignore '$Recycle.Bin', see https://github.com/owncloud/client/issues/2955
static const char *win_reserved_words_3[] = { "CON", "PRN", "AUX", "NUL" };
static const char *win_reserved_words_4[] = {
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
};
static const char *win_reserved_words_n[] = { "CLOCK$", "$Recycle.Bin" };

/**
 * @brief Checks if filename is considered reserved by Windows
 * @param file_name filename
 * @return true if file is reserved, false otherwise
 */
bool csync_is_windows_reserved_word(const QStringRef &filename)
{
    size_t len_filename = filename.size();

    // Drive letters
    if (len_filename == 2 && filename[1] == ':') {
        if (filename[0] >= 'a' && filename[0] <= 'z') {
            return true;
        }
        if (filename[0] >= 'A' && filename[0] <= 'Z') {
            return true;
        }
    }

    if (len_filename == 3 || (len_filename > 3 && filename[3] == '.')) {
        for (const char *word : win_reserved_words_3) {
            if (filename.left(3).compare(QLatin1String(word), Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }

    if (len_filename == 4 || (len_filename > 4 && filename[4] == '.')) {
        for (const char *word : win_reserved_words_4) {
            if (filename.left(4).compare(QLatin1String(word), Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }

    for (const char *word : win_reserved_words_n) {
        if (filename.compare(QLatin1String(word), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

static CSYNC_EXCLUDE_TYPE _csync_excluded_common(const QString &path, bool excludeConflictFiles)
{
    int rc = -1;
    CSYNC_EXCLUDE_TYPE match = CSYNC_NOT_EXCLUDED;

    /* split up the path */
    QStringRef bname(&path);
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash >= 0) {
        bname = path.midRef(lastSlash + 1);
    }

    size_t blen = bname.size();
    // 9 = strlen(".sync_.db")
    if (blen >= 9 && bname[0] == '.') {
        if (bname.contains(QLatin1String(".db"))) {
            if (bname.startsWith(QLatin1String("._sync_"), Qt::CaseInsensitive)  // "._sync_*.db*"
                || bname.startsWith(QLatin1String(".sync_"), Qt::CaseInsensitive) // ".sync_*.db*"
                || bname.startsWith(QLatin1String(".csync_journal.db"), Qt::CaseInsensitive)) { // ".csync_journal.db*"
                return CSYNC_FILE_SILENTLY_EXCLUDED;
            }
        }
        if (bname.startsWith(QLatin1String(".owncloudsync.log"), Qt::CaseInsensitive)) { // ".owncloudsync.log*"
            return CSYNC_FILE_SILENTLY_EXCLUDED;
        }
    }

    // check the strlen and ignore the file if its name is longer than 254 chars.
    // whenever changing this also check createDownloadTmpFileName
    if (blen > 254) {
        match = CSYNC_FILE_EXCLUDE_LONG_FILENAME;
        goto out;
    }

#ifdef _WIN32
    // Windows cannot sync files ending in spaces (#2176). It also cannot
    // distinguish files ending in '.' from files without an ending,
    // as '.' is a separator that is not stored internally, so let's
    // not allow to sync those to avoid file loss/ambiguities (#416)
    if (blen > 1) {
        if (bname[blen-1]== ' ') {
            match = CSYNC_FILE_EXCLUDE_TRAILING_SPACE;
            goto out;
        } else if (bname[blen-1]== '.' ) {
            match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
            goto out;
        }
    }

    if (csync_is_windows_reserved_word(bname)) {
      match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
      goto out;
    }

    // Filter out characters not allowed in a filename on windows
    for (auto p : path) {
        switch (p.unicode()) {
        case '\\':
        case ':':
        case '?':
        case '*':
        case '"':
        case '>':
        case '<':
        case '|':
            match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
            goto out;
        default:
            break;
        }
    }
#endif

    /* We create a Desktop.ini on Windows for the sidebar icon, make sure we don't sync it. */
    if (blen == 11 && path == bname) {
        rc = bname.compare(QLatin1String("Desktop.ini"), Qt::CaseInsensitive);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
    }

    if (excludeConflictFiles && OCC::Utility::isConflictFile(path)) {
        match = CSYNC_FILE_EXCLUDE_CONFLICT;
        goto out;
    }

  out:

    return match;
}


using namespace OCC;

ExcludedFiles::ExcludedFiles()
    : _clientVersion(MIRALL_VERSION_MAJOR, MIRALL_VERSION_MINOR, MIRALL_VERSION_PATCH)
{
    // Windows used to use PathMatchSpec which allows *foo to match abc/deffoo.
    _wildcardsMatchSlash = Utility::isWindows();
}

ExcludedFiles::~ExcludedFiles()
{
}

void ExcludedFiles::addExcludeFilePath(const QString &path)
{
    _excludeFiles.insert(path);
}

void ExcludedFiles::setExcludeConflictFiles(bool onoff)
{
    _excludeConflictFiles = onoff;
}

void ExcludedFiles::addManualExclude(const QByteArray &expr)
{
    _manualExcludes.append(expr);
    _allExcludes.append(expr);
    prepare();
}

void ExcludedFiles::clearManualExcludes()
{
    _manualExcludes.clear();
    reloadExcludeFiles();
}

void ExcludedFiles::setWildcardsMatchSlash(bool onoff)
{
    _wildcardsMatchSlash = onoff;
    prepare();
}

void ExcludedFiles::setClientVersion(ExcludedFiles::Version version)
{
    _clientVersion = version;
}

bool ExcludedFiles::reloadExcludeFiles()
{
    _allExcludes.clear();
    bool success = true;
    foreach (const QString &file, _excludeFiles) {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly)) {
            success = false;
            continue;
        }
        while (!f.atEnd()) {
            QByteArray line = f.readLine().trimmed();
            if (line.startsWith("#!version")) {
                if (!versionDirectiveKeepNextLine(line))
                    f.readLine();
            }
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            csync_exclude_expand_escapes(line);
            _allExcludes.append(line);
        }
    }
    _allExcludes.append(_manualExcludes);
    prepare();
    return success;
}

bool ExcludedFiles::versionDirectiveKeepNextLine(const QByteArray &directive) const
{
    if (!directive.startsWith("#!version"))
        return true;
    QByteArrayList args = directive.split(' ');
    if (args.size() != 3)
        return true;
    QByteArray op = args[1];
    QByteArrayList argVersions = args[2].split('.');
    if (argVersions.size() != 3)
        return true;

    auto argVersion = std::make_tuple(argVersions[0].toInt(), argVersions[1].toInt(), argVersions[2].toInt());
    if (op == "<=")
        return _clientVersion <= argVersion;
    if (op == "<")
        return _clientVersion < argVersion;
    if (op == ">")
        return _clientVersion > argVersion;
    if (op == ">=")
        return _clientVersion >= argVersion;
    if (op == "==")
        return _clientVersion == argVersion;
    return true;
}

bool ExcludedFiles::isExcluded(
    const QString &filePath,
    const QString &basePath,
    bool excludeHidden) const
{
    if (!filePath.startsWith(basePath, Utility::fsCasePreserving() ? Qt::CaseInsensitive : Qt::CaseSensitive)) {
        // Mark paths we're not responsible for as excluded...
        return true;
    }

    if (excludeHidden) {
        QString path = filePath;
        // Check all path subcomponents, but to *not* check the base path:
        // We do want to be able to sync with a hidden folder as the target.
        while (path.size() > basePath.size()) {
            QFileInfo fi(path);
            if (fi.isHidden() || fi.fileName().startsWith(QLatin1Char('.'))) {
                return true;
            }

            // Get the parent path
            path = fi.absolutePath();
        }
    }

    QFileInfo fi(filePath);
    ItemType type = ItemTypeFile;
    if (fi.isDir()) {
        type = ItemTypeDirectory;
    }

    QString relativePath = filePath.mid(basePath.size());
    if (relativePath.endsWith(QLatin1Char('/'))) {
        relativePath.chop(1);
    }

    return fullPatternMatch(relativePath, type) != CSYNC_NOT_EXCLUDED;
}

CSYNC_EXCLUDE_TYPE ExcludedFiles::traversalPatternMatch(const QString &path, ItemType filetype) const
{
    auto match = _csync_excluded_common(path, _excludeConflictFiles);
    if (match != CSYNC_NOT_EXCLUDED)
        return match;
    if (_allExcludes.isEmpty())
        return CSYNC_NOT_EXCLUDED;

    // Check the bname part of the path to see whether the full
    // regex should be run.
    QStringRef bnameStr(&path);
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash >= 0) {
        bnameStr = path.midRef(lastSlash + 1);
    }

    QRegularExpressionMatch m;
    if (filetype == ItemTypeDirectory) {
        m = _bnameTraversalRegexDir.match(bnameStr);
    } else {
        m = _bnameTraversalRegexFile.match(bnameStr);
    }
    if (!m.hasMatch())
        return CSYNC_NOT_EXCLUDED;
    if (m.capturedStart(QStringLiteral("exclude")) != -1) {
        return CSYNC_FILE_EXCLUDE_LIST;
    } else if (m.capturedStart(QStringLiteral("excluderemove")) != -1) {
        return CSYNC_FILE_EXCLUDE_AND_REMOVE;
    }

    // third capture: full path matching is triggered
    QString pathStr = path;

    if (filetype == ItemTypeDirectory) {
        m = _fullTraversalRegexDir.match(pathStr);
    } else {
        m = _fullTraversalRegexFile.match(pathStr);
    }
    if (m.hasMatch()) {
        if (m.capturedStart(QStringLiteral("exclude")) != -1) {
            return CSYNC_FILE_EXCLUDE_LIST;
        } else if (m.capturedStart(QStringLiteral("excluderemove")) != -1) {
            return CSYNC_FILE_EXCLUDE_AND_REMOVE;
        }
    }
    return CSYNC_NOT_EXCLUDED;
}

CSYNC_EXCLUDE_TYPE ExcludedFiles::fullPatternMatch(const QString &p, ItemType filetype) const
{
    auto match = _csync_excluded_common(p, _excludeConflictFiles);
    if (match != CSYNC_NOT_EXCLUDED)
        return match;
    if (_allExcludes.isEmpty())
        return CSYNC_NOT_EXCLUDED;

    QRegularExpressionMatch m;
    if (filetype == ItemTypeDirectory) {
        m = _fullRegexDir.match(p);
    } else {
        m = _fullRegexFile.match(p);
    }
    if (m.hasMatch()) {
        if (m.capturedStart(QStringLiteral("exclude")) != -1) {
            return CSYNC_FILE_EXCLUDE_LIST;
        } else if (m.capturedStart(QStringLiteral("excluderemove")) != -1) {
            return CSYNC_FILE_EXCLUDE_AND_REMOVE;
        }
    }
    return CSYNC_NOT_EXCLUDED;
}

/**
 * On linux we used to use fnmatch with FNM_PATHNAME, but the windows function we used
 * didn't have that behavior. wildcardsMatchSlash can be used to control which behavior
 * the resulting regex shall use.
 */
static QString convertToRegexpSyntax(QString exclude, bool wildcardsMatchSlash)
{
    // Translate *, ?, [...] to their regex variants.
    // The escape sequences \*, \?, \[. \\ have a special meaning,
    // the other ones have already been expanded before
    // (like "\\n" being replaced by "\n").
    //
    // QString being UTF-16 makes unicode-correct escaping tricky.
    // If we escaped each UTF-16 code unit we'd end up splitting 4-byte
    // code points. To avoid problems we delegate as much work as possible to
    // QRegularExpression::escape(): It always receives as long a sequence
    // as code units as possible.
    QString regex;
    int i = 0;
    int charsToEscape = 0;
    auto flush = [&]() {
        regex.append(QRegularExpression::escape(exclude.mid(i - charsToEscape, charsToEscape)));
        charsToEscape = 0;
    };
    auto len = exclude.size();
    for (; i < len; ++i) {
        switch (exclude[i].unicode()) {
        case '*':
            flush();
            if (wildcardsMatchSlash) {
                regex.append(".*");
            } else {
                regex.append("[^/]*");
            }
            break;
        case '?':
            flush();
            if (wildcardsMatchSlash) {
                regex.append(".");
            } else {
                regex.append("[^/]");
            }
            break;
        case '[': {
            flush();
            // Find the end of the bracket expression
            auto j = i + 1;
            for (; j < len; ++j) {
                if (exclude[j] == ']')
                    break;
                if (j != len - 1 && exclude[j] == '\\' && exclude[j + 1] == ']')
                    ++j;
            }
            if (j == len) {
                // no matching ], just insert the escaped [
                regex.append("\\[");
                break;
            }
            // Translate [! to [^
            QString bracketExpr = exclude.mid(i, j - i + 1);
            if (bracketExpr.startsWith("[!"))
                bracketExpr[1] = '^';
            regex.append(bracketExpr);
            i = j;
            break;
        }
        case '\\':
            flush();
            if (i == len - 1) {
                regex.append("\\\\");
                break;
            }
            // '\*' -> '\*', but '\z' -> '\\z'
            switch (exclude[i + 1].unicode()) {
            case '*':
            case '?':
            case '[':
            case '\\':
                regex.append(QRegularExpression::escape(exclude.mid(i + 1, 1)));
                break;
            default:
                charsToEscape += 2;
                break;
            }
            ++i;
            break;
        default:
            ++charsToEscape;
            break;
        }
    }
    flush();
    return regex;
}

static QString extractBnameTrigger(const QString &exclude, bool wildcardsMatchSlash)
{
    // We can definitely drop everything to the left of a / - that will never match
    // any bname.
    QString pattern = exclude.mid(exclude.lastIndexOf('/') + 1);

    // Easy case, nothing else can match a slash, so that's it.
    if (!wildcardsMatchSlash)
        return pattern;

    // Otherwise it's more complicated. Examples:
    // - "foo*bar" can match "fooX/Xbar", pattern is "*bar"
    // - "foo*bar*" can match "fooX/XbarX", pattern is "*bar*"
    // - "foo?bar" can match "foo/bar" but also "fooXbar", pattern is "*bar"

    auto isWildcard = [](QChar c) { return c == QLatin1Char('*') || c == QLatin1Char('?'); };

    // First, skip wildcards on the very right of the pattern
    int i = pattern.size() - 1;
    while (i >= 0 && isWildcard(pattern[i]))
        --i;

    // Then scan further until the next wildcard that could match a /
    while (i >= 0 && !isWildcard(pattern[i]))
        --i;

    // Everything to the right is part of the pattern
    pattern = pattern.mid(i + 1);

    // And if there was a wildcard, it starts with a *
    if (i >= 0)
        pattern.prepend('*');

    return pattern;
}

void ExcludedFiles::prepare()
{
    // Build regular expressions for the different cases.
    //
    // To compose the _bnameTraversalRegex, _fullTraversalRegex and _fullRegex
    // patterns we collect several subgroups of patterns here.
    //
    // * The "full" group will contain all patterns that contain a non-trailing
    //   slash. They only make sense in the fullRegex and fullTraversalRegex.
    // * The "bname" group contains all patterns without a non-trailing slash.
    //   These need separate handling in the _fullRegex (slash-containing
    //   patterns must be anchored to the front, these don't need it)
    // * The "bnameTrigger" group contains the bname part of all patterns in the
    //   "full" group. These and the "bname" group become _bnameTraversalRegex.
    //
    // To complicate matters, the exclude patterns have two binary attributes
    // meaning we'll end up with 4 variants:
    // * "]" patterns mean "EXCLUDE_AND_REMOVE", they get collected in the
    //   pattern strings ending in "Remove". The others go to "Keep".
    // * trailing-slash patterns match directories only. They get collected
    //   in the pattern strings saying "Dir", the others go into "FileDir"
    //   because they match files and directories.

    QString fullFileDirKeep;
    QString fullFileDirRemove;
    QString fullDirKeep;
    QString fullDirRemove;

    QString bnameFileDirKeep;
    QString bnameFileDirRemove;
    QString bnameDirKeep;
    QString bnameDirRemove;

    QString bnameTriggerFileDir;
    QString bnameTriggerDir;

    auto regexAppend = [](QString &fileDirPattern, QString &dirPattern, const QString &appendMe, bool dirOnly) {
        QString &pattern = dirOnly ? dirPattern : fileDirPattern;
        if (!pattern.isEmpty())
            pattern.append("|");
        pattern.append(appendMe);
    };

    for (auto exclude : _allExcludes) {
        if (exclude[0] == '\n')
            continue; // empty line
        if (exclude[0] == '\r')
            continue; // empty line

        bool matchDirOnly = exclude.endsWith('/');
        if (matchDirOnly)
            exclude = exclude.left(exclude.size() - 1);

        bool removeExcluded = (exclude[0] == ']');
        if (removeExcluded)
            exclude = exclude.mid(1);

        bool fullPath = exclude.contains('/');

        /* Use QRegularExpression, append to the right pattern */
        auto &bnameFileDir = removeExcluded ? bnameFileDirRemove : bnameFileDirKeep;
        auto &bnameDir = removeExcluded ? bnameDirRemove : bnameDirKeep;
        auto &fullFileDir = removeExcluded ? fullFileDirRemove : fullFileDirKeep;
        auto &fullDir = removeExcluded ? fullDirRemove : fullDirKeep;

        auto regexExclude = convertToRegexpSyntax(QString::fromUtf8(exclude), _wildcardsMatchSlash);
        if (!fullPath) {
            regexAppend(bnameFileDir, bnameDir, regexExclude, matchDirOnly);
        } else {
            regexAppend(fullFileDir, fullDir, regexExclude, matchDirOnly);

            // For activation, trigger on the 'bname' part of the full pattern.
            QString bnameExclude = extractBnameTrigger(exclude, _wildcardsMatchSlash);
            auto regexBname = convertToRegexpSyntax(bnameExclude, true);
            regexAppend(bnameTriggerFileDir, bnameTriggerDir, regexBname, matchDirOnly);
        }
    }

    // The empty pattern would match everything - change it to match-nothing
    auto emptyMatchNothing = [](QString &pattern) {
        if (pattern.isEmpty())
            pattern = "a^";
    };
    emptyMatchNothing(fullFileDirKeep);
    emptyMatchNothing(fullFileDirRemove);
    emptyMatchNothing(fullDirKeep);
    emptyMatchNothing(fullDirRemove);

    emptyMatchNothing(bnameFileDirKeep);
    emptyMatchNothing(bnameFileDirRemove);
    emptyMatchNothing(bnameDirKeep);
    emptyMatchNothing(bnameDirRemove);

    emptyMatchNothing(bnameTriggerFileDir);
    emptyMatchNothing(bnameTriggerDir);

    // The bname regex is applied to the bname only, so it must be
    // anchored in the beginning and in the end. It has the structure:
    // (exclude)|(excluderemove)|(bname triggers).
    // If the third group matches, the fullActivatedRegex needs to be applied
    // to the full path.
    _bnameTraversalRegexFile.setPattern(
        "^(?P<exclude>" + bnameFileDirKeep + ")$|"
        + "^(?P<excluderemove>" + bnameFileDirRemove + ")$|"
        + "^(?P<trigger>" + bnameTriggerFileDir + ")$");
    _bnameTraversalRegexDir.setPattern(
        "^(?P<exclude>" + bnameFileDirKeep + "|" + bnameDirKeep + ")$|"
        + "^(?P<excluderemove>" + bnameFileDirRemove + "|" + bnameDirRemove + ")$|"
        + "^(?P<trigger>" + bnameTriggerFileDir + "|" + bnameTriggerDir + ")$");

    // The full traveral regex is applied to the full path if the trigger capture of
    // the bname regex matches. Its basic form is (exclude)|(excluderemove)".
    // This pattern can be much simpler than fullRegex since we can assume a traversal
    // situation and doesn't need to look for bname patterns in parent paths.
    _fullTraversalRegexFile.setPattern(
        QLatin1String("")
        // Full patterns are anchored to the beginning
        + "^(?P<exclude>" + fullFileDirKeep + ")(?:$|/)"
        + "|"
        + "^(?P<excluderemove>" + fullFileDirRemove + ")(?:$|/)");
    _fullTraversalRegexDir.setPattern(
        QLatin1String("")
        + "^(?P<exclude>" + fullFileDirKeep + "|" + fullDirKeep + ")(?:$|/)"
        + "|"
        + "^(?P<excluderemove>" + fullFileDirRemove + "|" + fullDirRemove + ")(?:$|/)");

    // The full regex is applied to the full path and incorporates both bname and
    // full-path patterns. It has the form "(exclude)|(excluderemove)".
    _fullRegexFile.setPattern(
        QLatin1String("(?P<exclude>")
        // Full patterns are anchored to the beginning
        + "^(?:" + fullFileDirKeep + ")(?:$|/)" + "|"
        // Simple bname patterns can be any path component
        + "(?:^|/)(?:" + bnameFileDirKeep + ")(?:$|/)" + "|"
        // When checking a file for exclusion we must check all parent paths
        // against the dir-only patterns as well.
        + "(?:^|/)(?:" + bnameDirKeep + ")/"
        + ")"
        + "|"
        + "(?P<excluderemove>"
        + "^(?:" + fullFileDirRemove + ")(?:$|/)" + "|"
        + "(?:^|/)(?:" + bnameFileDirRemove + ")(?:$|/)" + "|"
        + "(?:^|/)(?:" + bnameDirRemove + ")/"
        + ")");
    _fullRegexDir.setPattern(
        QLatin1String("(?P<exclude>")
        + "^(?:" + fullFileDirKeep + "|" + fullDirKeep + ")(?:$|/)" + "|"
        + "(?:^|/)(?:" + bnameFileDirKeep + "|" + bnameDirKeep + ")(?:$|/)"
        + ")"
        + "|"
        + "(?P<excluderemove>"
        + "^(?:" + fullFileDirRemove + "|" + fullDirRemove + ")(?:$|/)" + "|"
        + "(?:^|/)(?:" + bnameFileDirRemove + "|" + bnameDirRemove + ")(?:$|/)"
        + ")");

    QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
    if (OCC::Utility::fsCasePreserving())
        patternOptions |= QRegularExpression::CaseInsensitiveOption;
    _bnameTraversalRegexFile.setPatternOptions(patternOptions);
    _bnameTraversalRegexFile.optimize();
    _bnameTraversalRegexDir.setPatternOptions(patternOptions);
    _bnameTraversalRegexDir.optimize();
    _fullTraversalRegexFile.setPatternOptions(patternOptions);
    _fullTraversalRegexFile.optimize();
    _fullTraversalRegexDir.setPatternOptions(patternOptions);
    _fullTraversalRegexDir.optimize();
    _fullRegexFile.setPatternOptions(patternOptions);
    _fullRegexFile.optimize();
    _fullRegexDir.setPatternOptions(patternOptions);
    _fullRegexDir.optimize();
}
