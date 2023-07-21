/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include <QTemporaryDir>

#include "csync_exclude.h"
#include "testutils.h"

using namespace OCC;

namespace {
const QString excludeListFileC = QStringLiteral(SOURCEDIR "/sync-exclude.lst");
}
// The tests were converted from the old CMocka framework, that's why there is a global
static QScopedPointer<ExcludedFiles> excludedFiles;

static void setup() {
    excludedFiles.reset(new ExcludedFiles);
    excludedFiles->setWildcardsMatchSlash(false);
}

static void setup_init() {
    setup();

    excludedFiles->addExcludeFilePath(excludeListFileC);
    QVERIFY(excludedFiles->reloadExcludeFiles());

    /* and add some unicode stuff */
    excludedFiles->addManualExclude(QStringLiteral("*.💩")); // is this source file utf8 encoded?
    excludedFiles->addManualExclude(QStringLiteral("пятницы.*"));
    excludedFiles->addManualExclude(QStringLiteral("*/*.out"));
    excludedFiles->addManualExclude(QStringLiteral("latex*/*.run.xml"));
    excludedFiles->addManualExclude(QStringLiteral("latex/*/*.tex.tmp"));

    QVERIFY(excludedFiles->reloadExcludeFiles());
}

class TestExcludedFiles: public QObject
{
    Q_OBJECT

    static auto check_file_full(const QString &path) { return excludedFiles->fullPatternMatch(path, ItemTypeFile); }

    static auto check_dir_full(const QString &path) { return excludedFiles->fullPatternMatch(path, ItemTypeDirectory); }

    static auto check_file_traversal(const QString &path) { return excludedFiles->traversalPatternMatch(path, ItemTypeFile); }

    static auto check_dir_traversal(const QString &path) { return excludedFiles->traversalPatternMatch(path, ItemTypeDirectory); }

private slots:
    void testFun()
    {
        ExcludedFiles excluded;
        bool excludeHidden = true;
        bool keepHidden = false;

        auto check_isExcluded = [&](const QString &a, bool keepHidden, bool create = true) {
            auto tmp = OCC::TestUtils::createTempDir();
            Q_ASSERT(tmp.isValid());

            auto createTree = [&](const QString &path) {
                auto parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
                const auto fileName = parts.back();
                parts.pop_back(); // remove file name
                QString workPath = tmp.path() + QLatin1Char('/');
                for (const auto &p : qAsConst(parts)) {
                    QDir dir(workPath);
                    dir.mkdir(p);
                    workPath += p + QLatin1Char('/');
                }
                workPath += fileName;
                QFile file(workPath);
                QVERIFY(file.open(QFile::WriteOnly));
                file.write("ownCloud");
                file.close();
            };
            if (create) {
                createTree(a);
            }
            return excluded.isExcluded(QString{tmp.path() + a}, QString{tmp.path() + QStringLiteral("/a")}, keepHidden);
        };

        QVERIFY(!check_isExcluded(QStringLiteral("/a/b"), keepHidden));
        QVERIFY(!check_isExcluded(QStringLiteral("/a/b~"), keepHidden));
        QVERIFY(!check_isExcluded(QStringLiteral("/a/.b"), keepHidden));
        QVERIFY(check_isExcluded(QStringLiteral("/a/.b"), excludeHidden));

        excluded.addExcludeFilePath(excludeListFileC);
        excluded.reloadExcludeFiles();

        QVERIFY(!check_isExcluded(QStringLiteral("/a/b"), keepHidden));
        QVERIFY(check_isExcluded(QStringLiteral("/a/b~"), keepHidden));
        QVERIFY(!check_isExcluded(QStringLiteral("/a/.b"), keepHidden));
        QVERIFY(check_isExcluded(QStringLiteral("/a/.Trashes"), keepHidden));
        QVERIFY(check_isExcluded(QStringLiteral("/a/foo_conflict-bar"), keepHidden));
        QVERIFY(check_isExcluded(QStringLiteral("/a/foo (conflicted copy bar)"), keepHidden));
        QVERIFY(check_isExcluded(QStringLiteral("/a/.b"), excludeHidden));

        // test non exisitng folder
        QVERIFY(check_isExcluded(QStringLiteral("/a/.b"), excludeHidden, false));
    }

    void check_csync_exclude_add()
    {
        setup();
        excludedFiles->addManualExclude(QStringLiteral("/tmp/check_csync1/*"));
        QCOMPARE(check_file_full(QStringLiteral("/tmp/check_csync1/foo")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("/tmp/check_csync2/foo")), CSYNC_NOT_EXCLUDED);
        QVERIFY(excludedFiles->_allExcludes.contains(QStringLiteral("/tmp/check_csync1/*")));

        QVERIFY(excludedFiles->_fullRegexFile.pattern().contains(QStringLiteral("csync1")));
        QVERIFY(excludedFiles->_fullTraversalRegexFile.pattern().contains(QStringLiteral("csync1")));
        QVERIFY(!excludedFiles->_bnameTraversalRegexFile.pattern().contains(QStringLiteral("csync1")));

        excludedFiles->addManualExclude(QStringLiteral("foo"));
        QVERIFY(excludedFiles->_bnameTraversalRegexFile.pattern().contains(QStringLiteral("foo")));
        QVERIFY(excludedFiles->_fullRegexFile.pattern().contains(QStringLiteral("foo")));
        QVERIFY(!excludedFiles->_fullTraversalRegexFile.pattern().contains(QStringLiteral("foo")));
    }

    void check_csync_excluded()
    {
        setup_init();
        QCOMPARE(check_file_full(QString()), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("/")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("A")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("krawel_krawel")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral(".kde/share/config/kwin.eventsrc")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral(".directory/cache-maximegalon/cache1.txt")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full(QStringLiteral("mozilla/.directory")), CSYNC_FILE_EXCLUDE_LIST);

        /*
        * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
        * to be found in top dir as well as in directories underneath.
        */
        QCOMPARE(check_dir_full(QStringLiteral(".apdisk")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full(QStringLiteral("foo/.apdisk")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full(QStringLiteral("foo/bar/.apdisk")), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_full(QStringLiteral(".java")), CSYNC_NOT_EXCLUDED);

        /* Files in the ignored dir .java will also be ignored. */
        QCOMPARE(check_file_full(QStringLiteral(".apdisk/totally_amazing.jar")), CSYNC_FILE_EXCLUDE_LIST);

        /* and also in subdirs */
        QCOMPARE(check_file_full(QStringLiteral("projects/.apdisk/totally_amazing.jar")), CSYNC_FILE_EXCLUDE_LIST);

        /* csync-journal is ignored in general silently. */
        QCOMPARE(check_file_full(QStringLiteral(".csync_journal.db")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral(".csync_journal.db.ctmp")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("subdir/.csync_journal.db")), CSYNC_FILE_SILENTLY_EXCLUDED);

        /* also the new form of the database name */
        QCOMPARE(check_file_full(QStringLiteral("._sync_5bdd60bdfcfa.db")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("._sync_5bdd60bdfcfa.db.ctmp")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("._sync_5bdd60bdfcfa.db-shm")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("subdir/._sync_5bdd60bdfcfa.db")), CSYNC_FILE_SILENTLY_EXCLUDED);

        QCOMPARE(check_file_full(QStringLiteral(".sync_5bdd60bdfcfa.db")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral(".sync_5bdd60bdfcfa.db.ctmp")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral(".sync_5bdd60bdfcfa.db-shm")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("subdir/.sync_5bdd60bdfcfa.db")), CSYNC_FILE_SILENTLY_EXCLUDED);


        /* pattern ]*.directory - ignore and remove */
        QCOMPARE(check_file_full(QStringLiteral("my.~directory")), CSYNC_FILE_EXCLUDE_AND_REMOVE);
        QCOMPARE(check_file_full(QStringLiteral("/a_folder/my.~directory")), CSYNC_FILE_EXCLUDE_AND_REMOVE);

        /* Not excluded because the pattern .netscape/cache requires directory. */
        QCOMPARE(check_file_full(QStringLiteral(".netscape/cache")), CSYNC_NOT_EXCLUDED);

        /* Not excluded  */
        QCOMPARE(check_file_full(QStringLiteral("unicode/中文.hé")), CSYNC_NOT_EXCLUDED);
        /* excluded  */
        QCOMPARE(check_file_full(QStringLiteral("unicode/пятницы.txt")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("unicode/中文.💩")), CSYNC_FILE_EXCLUDE_LIST);

        /* path wildcards */
        QCOMPARE(check_file_full(QStringLiteral("foobar/my_manuscript.out")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("latex_tmp/my_manuscript.run.xml")), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_full(QStringLiteral("word_tmp/my_manuscript.run.xml")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_full(QStringLiteral("latex/my_manuscript.tex.tmp")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_full(QStringLiteral("latex/songbook/my_manuscript.tex.tmp")), CSYNC_FILE_EXCLUDE_LIST);

#ifdef _WIN32
        QCOMPARE(check_file_full(QStringLiteral("file_trailing_space ")), CSYNC_FILE_EXCLUDE_TRAILING_SPACE);
        QCOMPARE(check_file_full(QStringLiteral("file_trailing_dot.")), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_full(QStringLiteral("AUX")), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_full(QStringLiteral("file_invalid_char<")), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_full(QStringLiteral("file_invalid_char\n")), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
#endif

        /* ? character */
        excludedFiles->addManualExclude(QStringLiteral("bond00?"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_full(QStringLiteral("bond00")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("bond007")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("bond0071")), CSYNC_NOT_EXCLUDED);

        /* brackets */
        excludedFiles->addManualExclude(QStringLiteral("a [bc] d"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_full(QStringLiteral("a d d")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("a  d")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("a b d")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("a c d")), CSYNC_FILE_EXCLUDE_LIST);

#ifndef Q_OS_WIN   // Because of CSYNC_FILE_EXCLUDE_INVALID_CHAR on windows
        /* escapes */
        excludedFiles->addManualExclude(QStringLiteral("a \\*"));
        excludedFiles->addManualExclude(QStringLiteral("b \\?"));
        excludedFiles->addManualExclude(QStringLiteral("c \\[d]"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_full(QStringLiteral("a \\*")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("a bc")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("a *")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("b \\?")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("b f")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("b ?")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("c \\[d]")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("c d")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(QStringLiteral("c [d]")), CSYNC_FILE_EXCLUDE_LIST);
#endif
    }

    void check_csync_excluded_traversal()
    {
        setup_init();
        QCOMPARE(check_file_traversal(QString()), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("/")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral("A")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral("krawel_krawel")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral(".kde/share/config/kwin.eventsrc")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_dir_traversal(QStringLiteral("mozilla/.directory")), CSYNC_FILE_EXCLUDE_LIST);

        /*
        * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
        * to be found in top dir as well as in directories underneath.
        */
        QCOMPARE(check_dir_traversal(QStringLiteral(".apdisk")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal(QStringLiteral("foo/.apdisk")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal(QStringLiteral("foo/bar/.apdisk")), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_traversal(QStringLiteral(".java")), CSYNC_NOT_EXCLUDED);

        /* csync-journal is ignored in general silently. */
        QCOMPARE(check_file_traversal(QStringLiteral(".csync_journal.db")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral(".csync_journal.db.ctmp")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("subdir/.csync_journal.db")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("/two/subdir/.csync_journal.db")), CSYNC_FILE_SILENTLY_EXCLUDED);

        /* also the new form of the database name */
        QCOMPARE(check_file_traversal(QStringLiteral("._sync_5bdd60bdfcfa.db")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("._sync_5bdd60bdfcfa.db.ctmp")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("._sync_5bdd60bdfcfa.db-shm")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("subdir/._sync_5bdd60bdfcfa.db")), CSYNC_FILE_SILENTLY_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral(".sync_5bdd60bdfcfa.db")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral(".sync_5bdd60bdfcfa.db.ctmp")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral(".sync_5bdd60bdfcfa.db-shm")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("subdir/.sync_5bdd60bdfcfa.db")), CSYNC_FILE_SILENTLY_EXCLUDED);

        /* Other builtin excludes */
        QCOMPARE(check_file_traversal(QStringLiteral("foo/Desktop.ini")), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("Desktop.ini")), CSYNC_FILE_SILENTLY_EXCLUDED);

        /* pattern ]*.directory - ignore and remove */
        QCOMPARE(check_file_traversal(QStringLiteral("my.~directory")), CSYNC_FILE_EXCLUDE_AND_REMOVE);
        QCOMPARE(check_file_traversal(QStringLiteral("/a_folder/my.~directory")), CSYNC_FILE_EXCLUDE_AND_REMOVE);

        /* Not excluded because the pattern .netscape/cache requires directory. */
        QCOMPARE(check_file_traversal(QStringLiteral(".netscape/cache")), CSYNC_NOT_EXCLUDED);

        /* Not excluded  */
        QCOMPARE(check_file_traversal(QStringLiteral("unicode/中文.hé")), CSYNC_NOT_EXCLUDED);
        /* excluded  */
        QCOMPARE(check_file_traversal(QStringLiteral("unicode/пятницы.txt")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("unicode/中文.💩")), CSYNC_FILE_EXCLUDE_LIST);

        /* path wildcards */
        QCOMPARE(check_file_traversal(QStringLiteral("foobar/my_manuscript.out")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("latex_tmp/my_manuscript.run.xml")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("word_tmp/my_manuscript.run.xml")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("latex/my_manuscript.tex.tmp")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("latex/songbook/my_manuscript.tex.tmp")), CSYNC_FILE_EXCLUDE_LIST);

#ifdef _WIN32
        QCOMPARE(check_file_traversal(QStringLiteral("file_trailing_space ")), CSYNC_FILE_EXCLUDE_TRAILING_SPACE);
        QCOMPARE(check_file_traversal(QStringLiteral("file_trailing_dot.")), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_traversal(QStringLiteral("AUX")), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_traversal(QStringLiteral("file_invalid_char<")), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_traversal(QStringLiteral("file_invalid_char\n")), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
#endif


        /* From here the actual traversal tests */

        excludedFiles->addManualExclude(QStringLiteral("/exclude"));
        excludedFiles->reloadExcludeFiles();

        /* Check toplevel dir, the pattern only works for toplevel dir. */
        QCOMPARE(check_dir_traversal(QStringLiteral("/exclude")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal(QStringLiteral("/foo/exclude")), CSYNC_NOT_EXCLUDED);

        /* check for a file called exclude. Must still work */
        QCOMPARE(check_file_traversal(QStringLiteral("/exclude")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("/foo/exclude")), CSYNC_NOT_EXCLUDED);

        /* Add an exclude for directories only: excl/ */
        excludedFiles->addManualExclude(QStringLiteral("excl/"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_dir_traversal(QStringLiteral("/excl")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal(QStringLiteral("meep/excl")), CSYNC_FILE_EXCLUDE_LIST);

        // because leading dirs aren't checked!
        QCOMPARE(check_file_traversal(QStringLiteral("meep/excl/file")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("/excl")), CSYNC_NOT_EXCLUDED);

        excludedFiles->addManualExclude(QStringLiteral("/excludepath/withsubdir"));
        excludedFiles->reloadExcludeFiles();

        QCOMPARE(check_dir_traversal(QStringLiteral("/excludepath/withsubdir")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("/excludepath/withsubdir")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal(QStringLiteral("/excludepath/withsubdir2")), CSYNC_NOT_EXCLUDED);

        // because leading dirs aren't checked!
        QCOMPARE(check_dir_traversal(QStringLiteral("/excludepath/withsubdir/foo")), CSYNC_NOT_EXCLUDED);

        /* Check ending of pattern */
        QCOMPARE(check_file_traversal(QStringLiteral("/exclude")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("/excludeX")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("exclude")), CSYNC_NOT_EXCLUDED);

        excludedFiles->addManualExclude(QStringLiteral("exclude"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_traversal(QStringLiteral("exclude")), CSYNC_FILE_EXCLUDE_LIST);

        /* ? character */
        excludedFiles->addManualExclude(QStringLiteral("bond00?"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_traversal(QStringLiteral("bond00")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("bond007")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("bond0071")), CSYNC_NOT_EXCLUDED);

        /* brackets */
        excludedFiles->addManualExclude(QStringLiteral("a [bc] d"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_traversal(QStringLiteral("a d d")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("a  d")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("a b d")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("a c d")), CSYNC_FILE_EXCLUDE_LIST);

#ifndef Q_OS_WIN   // Because of CSYNC_FILE_EXCLUDE_INVALID_CHAR on windows
        /* escapes */
        excludedFiles->addManualExclude(QStringLiteral("a \\*"));
        excludedFiles->addManualExclude(QStringLiteral("b \\?"));
        excludedFiles->addManualExclude(QStringLiteral("c \\[d]"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_traversal(QStringLiteral("a \\*")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("a bc")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("a *")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("b \\?")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("b f")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("b ?")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("c \\[d]")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("c d")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("c [d]")), CSYNC_FILE_EXCLUDE_LIST);
#endif
    }

    void check_csync_dir_only()
    {
        setup();
        excludedFiles->addManualExclude(QStringLiteral("filedir"));
        excludedFiles->addManualExclude(QStringLiteral("dir/"));

        QCOMPARE(check_file_traversal(QStringLiteral("other")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("filedir")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("dir")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("s/other")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(QStringLiteral("s/filedir")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("s/dir")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_dir_traversal(QStringLiteral("other")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_dir_traversal(QStringLiteral("filedir")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal(QStringLiteral("dir")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal(QStringLiteral("s/other")), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_dir_traversal(QStringLiteral("s/filedir")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal(QStringLiteral("s/dir")), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_dir_full(QStringLiteral("filedir/foo")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("filedir/foo")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full(QStringLiteral("dir/foo")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("dir/foo")), CSYNC_FILE_EXCLUDE_LIST);
    }

    void check_csync_pathes()
    {
        setup_init();
        excludedFiles->addManualExclude(QStringLiteral("/exclude"));
        excludedFiles->reloadExcludeFiles();

        /* Check toplevel dir, the pattern only works for toplevel dir. */
        QCOMPARE(check_dir_full(QStringLiteral("/exclude")), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_dir_full(QStringLiteral("/foo/exclude")), CSYNC_NOT_EXCLUDED);

        /* check for a file called exclude. Must still work */
        QCOMPARE(check_file_full(QStringLiteral("/exclude")), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_full(QStringLiteral("/foo/exclude")), CSYNC_NOT_EXCLUDED);

        /* Add an exclude for directories only: excl/ */
        excludedFiles->addManualExclude(QStringLiteral("excl/"));
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_dir_full(QStringLiteral("/excl")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full(QStringLiteral("meep/excl")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("meep/excl/file")), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_full(QStringLiteral("/excl")), CSYNC_NOT_EXCLUDED);

        excludedFiles->addManualExclude(QStringLiteral("/excludepath/withsubdir"));
        excludedFiles->reloadExcludeFiles();

        QCOMPARE(check_dir_full(QStringLiteral("/excludepath/withsubdir")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QStringLiteral("/excludepath/withsubdir")), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_dir_full(QStringLiteral("/excludepath/withsubdir2")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_dir_full(QStringLiteral("/excludepath/withsubdir/foo")), CSYNC_FILE_EXCLUDE_LIST);
    }

    void check_csync_wildcards()
    {
        setup();
        excludedFiles->addManualExclude(QStringLiteral("a/foo*bar"));
        excludedFiles->addManualExclude(QStringLiteral("b/foo*bar*"));
        excludedFiles->addManualExclude(QStringLiteral("c/foo?bar"));
        excludedFiles->addManualExclude(QStringLiteral("d/foo?bar*"));
        excludedFiles->addManualExclude(QStringLiteral("e/foo?bar?"));
        excludedFiles->addManualExclude(QStringLiteral("g/bar*"));
        excludedFiles->addManualExclude(QStringLiteral("h/bar?"));

        excludedFiles->setWildcardsMatchSlash(false);

        QCOMPARE(check_file_traversal(QStringLiteral("a/fooXYZbar")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("a/fooX/Zbar")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral("b/fooXYZbarABC")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("b/fooX/ZbarABC")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral("c/fooXbar")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("c/foo/bar")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral("d/fooXbarABC")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("d/foo/barABC")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral("e/fooXbarA")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("e/foo/barA")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral("g/barABC")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("g/XbarABC")), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal(QStringLiteral("h/barZ")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("h/XbarZ")), CSYNC_NOT_EXCLUDED);

        excludedFiles->setWildcardsMatchSlash(true);

        QCOMPARE(check_file_traversal(QStringLiteral("a/fooX/Zbar")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("b/fooX/ZbarABC")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("c/foo/bar")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("d/foo/barABC")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal(QStringLiteral("e/foo/barA")), CSYNC_FILE_EXCLUDE_LIST);
    }

    void check_csync_regex_translation()
    {
        setup();
        auto translate = [](const QString &pattern) { return ExcludedFiles::convertToRegexpSyntax(pattern, false).toUtf8(); };

        QCOMPARE(translate(QString()), "");
        QCOMPARE(translate(QStringLiteral("abc")), "abc");
        QCOMPARE(translate(QStringLiteral("a*c")), "a[^/]*c");
        QCOMPARE(translate(QStringLiteral("a?c")), "a[^/]c");
        QCOMPARE(translate(QStringLiteral("a[xyz]c")), "a[xyz]c");
        QCOMPARE(translate(QStringLiteral("a[xyzc")), "a\\[xyzc");
        QCOMPARE(translate(QStringLiteral("a[!xyz]c")), "a[^xyz]c");
        QCOMPARE(translate(QStringLiteral("a\\*b\\?c\\[d\\\\e")), "a\\*b\\?c\\[d\\\\e");
        QCOMPARE(translate(QStringLiteral("a.c")), "a\\.c");
        QCOMPARE(translate(QStringLiteral("?𠜎?")), "[^/]\\𠜎[^/]"); // 𠜎 is 4-byte utf8
    }

    void check_csync_bname_trigger()
    {
        setup();
        bool wildcardsMatchSlash = false;
        auto translate = [&wildcardsMatchSlash](const QString &pattern) { return ExcludedFiles::extractBnameTrigger(pattern, wildcardsMatchSlash).toUtf8(); };

        QCOMPARE(translate(QString()), "");
        QCOMPARE(translate(QStringLiteral("a/b/")), "");
        QCOMPARE(translate(QStringLiteral("a/b/c")), "c");
        QCOMPARE(translate(QStringLiteral("c")), "c");
        QCOMPARE(translate(QStringLiteral("a/foo*")), "foo*");
        QCOMPARE(translate(QStringLiteral("a/abc*foo*")), "abc*foo*");

        wildcardsMatchSlash = true;

        QCOMPARE(translate(QString()), "");
        QCOMPARE(translate(QStringLiteral("a/b/")), "");
        QCOMPARE(translate(QStringLiteral("a/b/c")), "c");
        QCOMPARE(translate(QStringLiteral("c")), "c");
        QCOMPARE(translate(QStringLiteral("*")), "*");
        QCOMPARE(translate(QStringLiteral("a/foo*")), "foo*");
        QCOMPARE(translate(QStringLiteral("a/abc?foo*")), "*foo*");
        QCOMPARE(translate(QStringLiteral("a/abc*foo*")), "*foo*");
        QCOMPARE(translate(QStringLiteral("a/abc?foo?")), "*foo?");
        QCOMPARE(translate(QStringLiteral("a/abc*foo?*")), "*foo?*");
        QCOMPARE(translate(QStringLiteral("a/abc*/foo*")), "foo*");
    }

    void check_csync_is_windows_reserved_word()
    {
        auto csync_is_windows_reserved_word = [](const char *fn) {
            QString s = QString::fromLatin1(fn);
            extern bool csync_is_windows_reserved_word(QStringView filename);
            return csync_is_windows_reserved_word(s);
        };

        QVERIFY(csync_is_windows_reserved_word("CON"));
        QVERIFY(csync_is_windows_reserved_word("con"));
        QVERIFY(csync_is_windows_reserved_word("CON."));
        QVERIFY(csync_is_windows_reserved_word("con."));
        QVERIFY(csync_is_windows_reserved_word("CON.ference"));
        QVERIFY(!csync_is_windows_reserved_word("CONference"));
        QVERIFY(!csync_is_windows_reserved_word("conference"));
        QVERIFY(!csync_is_windows_reserved_word("conf.erence"));
        QVERIFY(!csync_is_windows_reserved_word("co"));

        QVERIFY(csync_is_windows_reserved_word("COM2"));
        QVERIFY(csync_is_windows_reserved_word("com2"));
        QVERIFY(csync_is_windows_reserved_word("COM2."));
        QVERIFY(csync_is_windows_reserved_word("com2."));
        QVERIFY(csync_is_windows_reserved_word("COM2.ference"));
        QVERIFY(!csync_is_windows_reserved_word("COM2ference"));
        QVERIFY(!csync_is_windows_reserved_word("com2ference"));
        QVERIFY(!csync_is_windows_reserved_word("com2f.erence"));
        QVERIFY(!csync_is_windows_reserved_word("com"));

        QVERIFY(csync_is_windows_reserved_word("CLOCK$"));
        QVERIFY(csync_is_windows_reserved_word("$Recycle.Bin"));
        QVERIFY(csync_is_windows_reserved_word("ClocK$"));
        QVERIFY(csync_is_windows_reserved_word("$recycle.bin"));

        QVERIFY(csync_is_windows_reserved_word("A:"));
        QVERIFY(csync_is_windows_reserved_word("a:"));
        QVERIFY(csync_is_windows_reserved_word("z:"));
        QVERIFY(csync_is_windows_reserved_word("Z:"));
        QVERIFY(csync_is_windows_reserved_word("M:"));
        QVERIFY(csync_is_windows_reserved_word("m:"));
    }

    /* QT_ENABLE_REGEXP_JIT=0 to get slower results :-) */
    void check_csync_excluded_performance1()
    {
        setup_init();
        const int N = 1000;
        int totalRc = 0;

        QBENCHMARK {

            for (int i = 0; i < N; ++i) {
                totalRc += check_dir_full(QStringLiteral("/this/is/quite/a/long/path/with/many/components"));
                totalRc += check_file_full(QStringLiteral("/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/27/29"));
            }
            QCOMPARE(totalRc, 0); // mainly to avoid optimization
        }
    }

    void check_csync_excluded_performance2()
    {
        const int N = 1000;
        int totalRc = 0;

        QBENCHMARK {
            for (int i = 0; i < N; ++i) {
                totalRc += check_dir_traversal(QStringLiteral("/this/is/quite/a/long/path/with/many/components"));
                totalRc += check_file_traversal(QStringLiteral("/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/27/29"));
            }
            QCOMPARE(totalRc, 0); // mainly to avoid optimization
        }
    }

    void check_csync_exclude_expand_escapes()
    {
        extern void csync_exclude_expand_escapes(QByteArray &input);

        QByteArray line = "keep \\' \\\" \\? \\\\ \\a \\b \\f \\n \\r \\t \\v \\z \\#";
        csync_exclude_expand_escapes(line);
        QVERIFY(0 == strcmp(line.constData(), "keep ' \" ? \\\\ \a \b \f \n \r \t \v \\z #"));

        line = "";
        csync_exclude_expand_escapes(line);
        QVERIFY(0 == strcmp(line.constData(), ""));

        line = "\\";
        csync_exclude_expand_escapes(line);
        QVERIFY(0 == strcmp(line.constData(), "\\"));
    }

    void check_version_directive()
    {
        ExcludedFiles excludes;
        excludes.setClientVersion(QVersionNumber(2, 5, 0));

        std::vector<std::pair<const char *, bool>> tests = {
            { "#!version == 2.5.0", true },
            { "#!version == 2.6.0", false },
            { "#!version < 2.6.0", true },
            { "#!version <= 2.6.0", true },
            { "#!version > 2.6.0", false },
            { "#!version >= 2.6.0", false },
            { "#!version < 2.4.0", false },
            { "#!version <= 2.4.0", false },
            { "#!version > 2.4.0", true },
            { "#!version >= 2.4.0", true },
            { "#!version < 2.5.0", false },
            { "#!version <= 2.5.0", true },
            { "#!version > 2.5.0", false },
            { "#!version >= 2.5.0", true },
        };
        for (auto test : tests) {
            QVERIFY(excludes.versionDirectiveKeepNextLine(test.first) == test.second);
        }
    }

};

QTEST_APPLESS_MAIN(TestExcludedFiles)
#include "testexcludedfiles.moc"
