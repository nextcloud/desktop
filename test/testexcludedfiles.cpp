/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include <QTemporaryDir>

#include "csync_exclude.h"

using namespace OCC;

#define EXCLUDE_LIST_FILE SOURCEDIR "/../../sync-exclude.lst"

// The tests were converted from the old CMocka framework, that's why there is a global
static QScopedPointer<ExcludedFiles> excludedFiles;

static void setup() {
    excludedFiles.reset(new ExcludedFiles);
    excludedFiles->setWildcardsMatchSlash(false);
}

static void setup_init() {
    setup();

    excludedFiles->addExcludeFilePath(EXCLUDE_LIST_FILE);
    QVERIFY(excludedFiles->reloadExcludeFiles());

    /* and add some unicode stuff */
    excludedFiles->addManualExclude("*.💩"); // is this source file utf8 encoded?
    excludedFiles->addManualExclude("пятницы.*");
    excludedFiles->addManualExclude("*/*.out");
    excludedFiles->addManualExclude("latex*/*.run.xml");
    excludedFiles->addManualExclude("latex/*/*.tex.tmp");

    QVERIFY(excludedFiles->reloadExcludeFiles());
}

class TestExcludedFiles: public QObject
{
    Q_OBJECT

static auto check_file_full(const char *path)
{
    return excludedFiles->fullPatternMatch(path, ItemTypeFile);
}

static auto check_dir_full(const char *path)
{
    return excludedFiles->fullPatternMatch(path, ItemTypeDirectory);
}

static auto check_file_traversal(const char *path)
{
    return excludedFiles->traversalPatternMatch(path, ItemTypeFile);
}

static auto check_dir_traversal(const char *path)
{
    return excludedFiles->traversalPatternMatch(path, ItemTypeDirectory);
}


private slots:
    void testFun()
    {
        ExcludedFiles excluded;
        bool excludeHidden = true;
        bool keepHidden = false;

        QVERIFY(!excluded.isExcluded("/a/b", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/b~", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/.b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.b", "/a", excludeHidden));

        excluded.addExcludeFilePath(EXCLUDE_LIST_FILE);
        excluded.reloadExcludeFiles();

        QVERIFY(!excluded.isExcluded("/a/b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/b~", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/.b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.Trashes", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/foo_conflict-bar", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/foo (conflicted copy bar)", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.b", "/a", excludeHidden));

        QVERIFY(excluded.isExcluded("/a/#b#", "/a", keepHidden));
    }

    void check_csync_exclude_add()
    {
        setup();
        excludedFiles->addManualExclude("/tmp/check_csync1/*");
        QCOMPARE(check_file_full("/tmp/check_csync1/foo"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("/tmp/check_csync2/foo"), CSYNC_NOT_EXCLUDED);
        QVERIFY(excludedFiles->_allExcludes[QStringLiteral("/")].contains("/tmp/check_csync1/*"));

        QVERIFY(excludedFiles->_fullRegexFile[QStringLiteral("/")].pattern().contains("csync1"));
        QVERIFY(excludedFiles->_fullTraversalRegexFile[QStringLiteral("/")].pattern().contains("csync1"));
        QVERIFY(!excludedFiles->_bnameTraversalRegexFile[QStringLiteral("/")].pattern().contains("csync1"));

        excludedFiles->addManualExclude("foo");
        QVERIFY(excludedFiles->_bnameTraversalRegexFile[QStringLiteral("/")].pattern().contains("foo"));
        QVERIFY(excludedFiles->_fullRegexFile[QStringLiteral("/")].pattern().contains("foo"));
        QVERIFY(!excludedFiles->_fullTraversalRegexFile[QStringLiteral("/")].pattern().contains("foo"));
    }

    void check_csync_exclude_add_per_dir()
    {
        setup();
        excludedFiles->addManualExclude("*", "/tmp/check_csync1/");
        QCOMPARE(check_file_full("/tmp/check_csync1/foo"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("/tmp/check_csync2/foo"), CSYNC_NOT_EXCLUDED);
        QVERIFY(excludedFiles->_allExcludes[QStringLiteral("/tmp/check_csync1/")].contains("*"));

        excludedFiles->addManualExclude("foo");
        QVERIFY(excludedFiles->_fullRegexFile[QStringLiteral("/")].pattern().contains("foo"));

        excludedFiles->addManualExclude("foo/bar", "/tmp/check_csync1/");
        QVERIFY(excludedFiles->_fullRegexFile[QStringLiteral("/tmp/check_csync1/")].pattern().contains("bar"));
        QVERIFY(excludedFiles->_fullTraversalRegexFile[QStringLiteral("/tmp/check_csync1/")].pattern().contains("bar"));
        QVERIFY(!excludedFiles->_bnameTraversalRegexFile[QStringLiteral("/tmp/check_csync1/")].pattern().contains("foo"));
    }

    void check_csync_excluded()
    {
        setup_init();
        QCOMPARE(check_file_full(""), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("/"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("A"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("krawel_krawel"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(".kde/share/config/kwin.eventsrc"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full(".directory/cache-maximegalon/cache1.txt"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full("mozilla/.directory"), CSYNC_FILE_EXCLUDE_LIST);

        /*
        * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
        * to be found in top dir as well as in directories underneath.
        */
        QCOMPARE(check_dir_full(".apdisk"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full("foo/.apdisk"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full("foo/bar/.apdisk"), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_full(".java"), CSYNC_NOT_EXCLUDED);

        /* Files in the ignored dir .java will also be ignored. */
        QCOMPARE(check_file_full(".apdisk/totally_amazing.jar"), CSYNC_FILE_EXCLUDE_LIST);

        /* and also in subdirs */
        QCOMPARE(check_file_full("projects/.apdisk/totally_amazing.jar"), CSYNC_FILE_EXCLUDE_LIST);

        /* csync-journal is ignored in general silently. */
        QCOMPARE(check_file_full(".csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(".csync_journal.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full("subdir/.csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

        /* also the new form of the database name */
        QCOMPARE(check_file_full("._sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full("._sync_5bdd60bdfcfa.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full("._sync_5bdd60bdfcfa.db-shm"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full("subdir/._sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

        QCOMPARE(check_file_full(".sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(".sync_5bdd60bdfcfa.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full(".sync_5bdd60bdfcfa.db-shm"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_full("subdir/.sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);


        /* pattern ]*.directory - ignore and remove */
        QCOMPARE(check_file_full("my.~directory"), CSYNC_FILE_EXCLUDE_AND_REMOVE);
        QCOMPARE(check_file_full("/a_folder/my.~directory"), CSYNC_FILE_EXCLUDE_AND_REMOVE);

        /* Not excluded because the pattern .netscape/cache requires directory. */
        QCOMPARE(check_file_full(".netscape/cache"), CSYNC_NOT_EXCLUDED);

        /* Not excluded  */
        QCOMPARE(check_file_full("unicode/中文.hé"), CSYNC_NOT_EXCLUDED);
        /* excluded  */
        QCOMPARE(check_file_full("unicode/пятницы.txt"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("unicode/中文.💩"), CSYNC_FILE_EXCLUDE_LIST);

        /* path wildcards */
        QCOMPARE(check_file_full("foobar/my_manuscript.out"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("latex_tmp/my_manuscript.run.xml"), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_full("word_tmp/my_manuscript.run.xml"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_full("latex/my_manuscript.tex.tmp"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_full("latex/songbook/my_manuscript.tex.tmp"), CSYNC_FILE_EXCLUDE_LIST);

    #ifdef _WIN32
        QCOMPARE(check_file_full("file_trailing_space "), CSYNC_FILE_EXCLUDE_TRAILING_SPACE);
        QCOMPARE(check_file_full("file_trailing_dot."), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_full("AUX"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_full("file_invalid_char<"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_full("file_invalid_char\n"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
    #endif

        /* ? character */
        excludedFiles->addManualExclude("bond00?");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_full("bond00"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("bond007"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("bond0071"), CSYNC_NOT_EXCLUDED);

        /* brackets */
        excludedFiles->addManualExclude("a [bc] d");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_full("a d d"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("a  d"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("a b d"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("a c d"), CSYNC_FILE_EXCLUDE_LIST);

#ifndef Q_OS_WIN   // Because of CSYNC_FILE_EXCLUDE_INVALID_CHAR on windows
        /* escapes */
        excludedFiles->addManualExclude("a \\*");
        excludedFiles->addManualExclude("b \\?");
        excludedFiles->addManualExclude("c \\[d]");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_full("a \\*"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("a bc"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("a *"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("b \\?"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("b f"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("b ?"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("c \\[d]"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("c d"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("c [d]"), CSYNC_FILE_EXCLUDE_LIST);
#endif
    }

    void check_csync_excluded_per_dir()
    {
        const auto tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        excludedFiles.reset(new ExcludedFiles(tempDir + "/"));
        excludedFiles->setWildcardsMatchSlash(false);
        excludedFiles->addManualExclude("A");
        excludedFiles->reloadExcludeFiles();

        QCOMPARE(check_file_full("A"), CSYNC_FILE_EXCLUDE_LIST);

        excludedFiles->clearManualExcludes();
        excludedFiles->addManualExclude("A", tempDir + "/B/");
        excludedFiles->reloadExcludeFiles();

        QCOMPARE(check_file_full("A"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("B/A"), CSYNC_FILE_EXCLUDE_LIST);

        excludedFiles->clearManualExcludes();
        excludedFiles->addManualExclude("A/a1", tempDir + "/B/");
        excludedFiles->reloadExcludeFiles();

        QCOMPARE(check_file_full("A"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_full("B/A/a1"), CSYNC_FILE_EXCLUDE_LIST);

        const auto fooDir = QStringLiteral("check_csync1/foo");
        QVERIFY(QDir(tempDir).mkpath(fooDir));

        const auto fooExcludeList = QString(tempDir + '/' + fooDir + "/.sync-exclude.lst");
        QFile excludeList(fooExcludeList);
        QVERIFY(excludeList.open(QFile::WriteOnly));
        QCOMPARE(excludeList.write("bar"), 3);
        excludeList.close();

        excludedFiles->addInTreeExcludeFilePath(fooExcludeList);
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_full(QByteArray(fooDir.toUtf8() + "/bar")), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full(QByteArray(fooDir.toUtf8() + "/baz")), CSYNC_NOT_EXCLUDED);
    }

    void check_csync_excluded_traversal_per_dir()
    {
        setup_init();
        QCOMPARE(check_file_traversal("/"), CSYNC_NOT_EXCLUDED);

        /* path wildcards */
        excludedFiles->addManualExclude("*/*.tex.tmp", "/latex/");
        QCOMPARE(check_file_traversal("latex/my_manuscript.tex.tmp"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("latex/songbook/my_manuscript.tex.tmp"), CSYNC_FILE_EXCLUDE_LIST);
    }

    void check_csync_excluded_traversal()
    {
        setup_init();
        QCOMPARE(check_file_traversal(""), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("/"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal("A"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal("krawel_krawel"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal(".kde/share/config/kwin.eventsrc"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_dir_traversal("mozilla/.directory"), CSYNC_FILE_EXCLUDE_LIST);

        /*
        * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
        * to be found in top dir as well as in directories underneath.
        */
        QCOMPARE(check_dir_traversal(".apdisk"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal("foo/.apdisk"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal("foo/bar/.apdisk"), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_traversal(".java"), CSYNC_NOT_EXCLUDED);

        /* csync-journal is ignored in general silently. */
        QCOMPARE(check_file_traversal(".csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(".csync_journal.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal("subdir/.csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal("/two/subdir/.csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

        /* also the new form of the database name */
        QCOMPARE(check_file_traversal("._sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal("._sync_5bdd60bdfcfa.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal("._sync_5bdd60bdfcfa.db-shm"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal("subdir/._sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

        QCOMPARE(check_file_traversal(".sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(".sync_5bdd60bdfcfa.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal(".sync_5bdd60bdfcfa.db-shm"), CSYNC_FILE_SILENTLY_EXCLUDED);
        QCOMPARE(check_file_traversal("subdir/.sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

        /* Other builtin excludes */
        QCOMPARE(check_file_traversal("foo/Desktop.ini"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("Desktop.ini"), CSYNC_FILE_SILENTLY_EXCLUDED);

        /* pattern ]*.directory - ignore and remove */
        QCOMPARE(check_file_traversal("my.~directory"), CSYNC_FILE_EXCLUDE_AND_REMOVE);
        QCOMPARE(check_file_traversal("/a_folder/my.~directory"), CSYNC_FILE_EXCLUDE_AND_REMOVE);

        /* Not excluded because the pattern .netscape/cache requires directory. */
        QCOMPARE(check_file_traversal(".netscape/cache"), CSYNC_NOT_EXCLUDED);

        /* Not excluded  */
        QCOMPARE(check_file_traversal("unicode/中文.hé"), CSYNC_NOT_EXCLUDED);
        /* excluded  */
        QCOMPARE(check_file_traversal("unicode/пятницы.txt"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("unicode/中文.💩"), CSYNC_FILE_EXCLUDE_LIST);

        /* path wildcards */
        QCOMPARE(check_file_traversal("foobar/my_manuscript.out"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("latex_tmp/my_manuscript.run.xml"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("word_tmp/my_manuscript.run.xml"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("latex/my_manuscript.tex.tmp"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("latex/songbook/my_manuscript.tex.tmp"), CSYNC_FILE_EXCLUDE_LIST);

    #ifdef _WIN32
        QCOMPARE(check_file_traversal("file_trailing_space "), CSYNC_FILE_EXCLUDE_TRAILING_SPACE);
        QCOMPARE(check_file_traversal("file_trailing_dot."), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_traversal("AUX"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
        QCOMPARE(check_file_traversal("file_invalid_char<"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
    #endif


        /* From here the actual traversal tests */

        excludedFiles->addManualExclude("/exclude");
        excludedFiles->reloadExcludeFiles();

        /* Check toplevel dir, the pattern only works for toplevel dir. */
        QCOMPARE(check_dir_traversal("/exclude"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal("/foo/exclude"), CSYNC_NOT_EXCLUDED);

        /* check for a file called exclude. Must still work */
        QCOMPARE(check_file_traversal("/exclude"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("/foo/exclude"), CSYNC_NOT_EXCLUDED);

        /* Add an exclude for directories only: excl/ */
        excludedFiles->addManualExclude("excl/");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_dir_traversal("/excl"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal("meep/excl"), CSYNC_FILE_EXCLUDE_LIST);

        // because leading dirs aren't checked!
        QCOMPARE(check_file_traversal("meep/excl/file"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("/excl"), CSYNC_NOT_EXCLUDED);

        excludedFiles->addManualExclude("/excludepath/withsubdir");
        excludedFiles->reloadExcludeFiles();

        QCOMPARE(check_dir_traversal("/excludepath/withsubdir"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("/excludepath/withsubdir"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal("/excludepath/withsubdir2"), CSYNC_NOT_EXCLUDED);

        // because leading dirs aren't checked!
        QCOMPARE(check_dir_traversal("/excludepath/withsubdir/foo"), CSYNC_NOT_EXCLUDED);

        /* Check ending of pattern */
        QCOMPARE(check_file_traversal("/exclude"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("/excludeX"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("exclude"), CSYNC_NOT_EXCLUDED);

        excludedFiles->addManualExclude("exclude");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_traversal("exclude"), CSYNC_FILE_EXCLUDE_LIST);

        /* ? character */
        excludedFiles->addManualExclude("bond00?");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_traversal("bond00"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("bond007"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("bond0071"), CSYNC_NOT_EXCLUDED);

        /* brackets */
        excludedFiles->addManualExclude("a [bc] d");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_traversal("a d d"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("a  d"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("a b d"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("a c d"), CSYNC_FILE_EXCLUDE_LIST);

#ifndef Q_OS_WIN   // Because of CSYNC_FILE_EXCLUDE_INVALID_CHAR on windows
        /* escapes */
        excludedFiles->addManualExclude("a \\*");
        excludedFiles->addManualExclude("b \\?");
        excludedFiles->addManualExclude("c \\[d]");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_file_traversal("a \\*"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("a bc"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("a *"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("b \\?"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("b f"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("b ?"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("c \\[d]"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("c d"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("c [d]"), CSYNC_FILE_EXCLUDE_LIST);
#endif
    }

    void check_csync_dir_only()
    {
        setup();
        excludedFiles->addManualExclude("filedir");
        excludedFiles->addManualExclude("dir/");

        QCOMPARE(check_file_traversal("other"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("filedir"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("dir"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("s/other"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_file_traversal("s/filedir"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("s/dir"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_dir_traversal("other"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_dir_traversal("filedir"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal("dir"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal("s/other"), CSYNC_NOT_EXCLUDED);
        QCOMPARE(check_dir_traversal("s/filedir"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_traversal("s/dir"), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_dir_full("filedir/foo"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("filedir/foo"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full("dir/foo"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("dir/foo"), CSYNC_FILE_EXCLUDE_LIST);
    }

    void check_csync_pathes()
    {
        setup_init();
        excludedFiles->addManualExclude("/exclude");
        excludedFiles->reloadExcludeFiles();

        /* Check toplevel dir, the pattern only works for toplevel dir. */
        QCOMPARE(check_dir_full("/exclude"), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_dir_full("/foo/exclude"), CSYNC_NOT_EXCLUDED);

        /* check for a file called exclude. Must still work */
        QCOMPARE(check_file_full("/exclude"), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_full("/foo/exclude"), CSYNC_NOT_EXCLUDED);

        /* Add an exclude for directories only: excl/ */
        excludedFiles->addManualExclude("excl/");
        excludedFiles->reloadExcludeFiles();
        QCOMPARE(check_dir_full("/excl"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_dir_full("meep/excl"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("meep/excl/file"), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_file_full("/excl"), CSYNC_NOT_EXCLUDED);

        excludedFiles->addManualExclude("/excludepath/withsubdir");
        excludedFiles->reloadExcludeFiles();

        QCOMPARE(check_dir_full("/excludepath/withsubdir"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_full("/excludepath/withsubdir"), CSYNC_FILE_EXCLUDE_LIST);

        QCOMPARE(check_dir_full("/excludepath/withsubdir2"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_dir_full("/excludepath/withsubdir/foo"), CSYNC_FILE_EXCLUDE_LIST);
    }

    void check_csync_wildcards()
    {
        setup();
        excludedFiles->addManualExclude("a/foo*bar");
        excludedFiles->addManualExclude("b/foo*bar*");
        excludedFiles->addManualExclude("c/foo?bar");
        excludedFiles->addManualExclude("d/foo?bar*");
        excludedFiles->addManualExclude("e/foo?bar?");
        excludedFiles->addManualExclude("g/bar*");
        excludedFiles->addManualExclude("h/bar?");

        excludedFiles->setWildcardsMatchSlash(false);

        QCOMPARE(check_file_traversal("a/fooXYZbar"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("a/fooX/Zbar"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal("b/fooXYZbarABC"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("b/fooX/ZbarABC"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal("c/fooXbar"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("c/foo/bar"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal("d/fooXbarABC"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("d/foo/barABC"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal("e/fooXbarA"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("e/foo/barA"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal("g/barABC"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("g/XbarABC"), CSYNC_NOT_EXCLUDED);

        QCOMPARE(check_file_traversal("h/barZ"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("h/XbarZ"), CSYNC_NOT_EXCLUDED);

        excludedFiles->setWildcardsMatchSlash(true);

        QCOMPARE(check_file_traversal("a/fooX/Zbar"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("b/fooX/ZbarABC"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("c/foo/bar"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("d/foo/barABC"), CSYNC_FILE_EXCLUDE_LIST);
        QCOMPARE(check_file_traversal("e/foo/barA"), CSYNC_FILE_EXCLUDE_LIST);
    }

    void check_csync_regex_translation()
    {
        setup();
        QByteArray storage;
        auto translate = [&storage](const char *pattern) {
            storage = ExcludedFiles::convertToRegexpSyntax(pattern, false).toUtf8();
            return storage.constData();
        };

        QCOMPARE(translate(""), "");
        QCOMPARE(translate("abc"), "abc");
        QCOMPARE(translate("a*c"), "a[^/]*c");
        QCOMPARE(translate("a?c"), "a[^/]c");
        QCOMPARE(translate("a[xyz]c"), "a[xyz]c");
        QCOMPARE(translate("a[xyzc"), "a\\[xyzc");
        QCOMPARE(translate("a[!xyz]c"), "a[^xyz]c");
        QCOMPARE(translate("a\\*b\\?c\\[d\\\\e"), "a\\*b\\?c\\[d\\\\e");
        QCOMPARE(translate("a.c"), "a\\.c");
        QCOMPARE(translate("?𠜎?"), "[^/]\\𠜎[^/]"); // 𠜎 is 4-byte utf8
    }

    void check_csync_bname_trigger()
    {
        setup();
        bool wildcardsMatchSlash = false;
        QByteArray storage;
        auto translate = [&storage, &wildcardsMatchSlash](const char *pattern) {
            storage = ExcludedFiles::extractBnameTrigger(pattern, wildcardsMatchSlash).toUtf8();
            return storage.constData();
        };

        QCOMPARE(translate(""), "");
        QCOMPARE(translate("a/b/"), "");
        QCOMPARE(translate("a/b/c"), "c");
        QCOMPARE(translate("c"), "c");
        QCOMPARE(translate("a/foo*"), "foo*");
        QCOMPARE(translate("a/abc*foo*"), "abc*foo*");

        wildcardsMatchSlash = true;

        QCOMPARE(translate(""), "");
        QCOMPARE(translate("a/b/"), "");
        QCOMPARE(translate("a/b/c"), "c");
        QCOMPARE(translate("c"), "c");
        QCOMPARE(translate("*"), "*");
        QCOMPARE(translate("a/foo*"), "foo*");
        QCOMPARE(translate("a/abc?foo*"), "*foo*");
        QCOMPARE(translate("a/abc*foo*"), "*foo*");
        QCOMPARE(translate("a/abc?foo?"), "*foo?");
        QCOMPARE(translate("a/abc*foo?*"), "*foo?*");
        QCOMPARE(translate("a/abc*/foo*"), "foo*");
    }

    void check_csync_is_windows_reserved_word()
    {
        auto csync_is_windows_reserved_word = [](const char *fn) {
            QString s = QString::fromLatin1(fn);
            extern bool csync_is_windows_reserved_word(const QStringRef &filename);
            return csync_is_windows_reserved_word(&s);
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
                totalRc += check_dir_full("/this/is/quite/a/long/path/with/many/components");
                totalRc += check_file_full("/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/27/29");
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
                totalRc += check_dir_traversal("/this/is/quite/a/long/path/with/many/components");
                totalRc += check_file_traversal("/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/27/29");
            }
            QCOMPARE(totalRc, 0); // mainly to avoid optimization
        }
    }

    void check_csync_exclude_expand_escapes()
    {
        extern void csync_exclude_expand_escapes(QByteArray &input);

        QByteArray line = R"(keep \' \" \? \\ \a \b \f \n \r \t \v \z \#)";
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
        excludes.setClientVersion(ExcludedFiles::Version(2, 5, 0));

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
