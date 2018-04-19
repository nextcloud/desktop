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
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <QTemporaryDir>

#define CSYNC_TEST 1
#include "csync_exclude.cpp"

#include "torture.h"

#define EXCLUDE_LIST_FILE SOURCEDIR"/../../sync-exclude.lst"

ExcludedFiles *excludedFiles = nullptr;

class ExcludedFilesTest
{
public:

static int setup(void **state) {
    CSYNC *csync;

    csync = new CSYNC("/tmp/check_csync1", new OCC::SyncJournalDb(""));
    excludedFiles = new ExcludedFiles;
    excludedFiles->setWildcardsMatchSlash(false);
    csync->exclude_traversal_fn = excludedFiles->csyncTraversalMatchFun();

    *state = csync;
    return 0;
}

static int setup_init(void **state) {
    CSYNC *csync;

    csync = new CSYNC("/tmp/check_csync1", new OCC::SyncJournalDb(""));
    excludedFiles = new ExcludedFiles;
    excludedFiles->setWildcardsMatchSlash(false);
    csync->exclude_traversal_fn = excludedFiles->csyncTraversalMatchFun();

    excludedFiles->addExcludeFilePath(EXCLUDE_LIST_FILE);
    assert_true(excludedFiles->reloadExcludeFiles());

    /* and add some unicode stuff */
    excludedFiles->addManualExclude("*.💩"); // is this source file utf8 encoded?
    excludedFiles->addManualExclude("пятницы.*");
    excludedFiles->addManualExclude("*/*.out");
    excludedFiles->addManualExclude("latex*/*.run.xml");
    excludedFiles->addManualExclude("latex/*/*.tex.tmp");

    assert_true(excludedFiles->reloadExcludeFiles());

    *state = csync;
    return 0;
}

static int teardown(void **state) {
    CSYNC *csync = (CSYNC*)*state;
    int rc;

    auto statedb = csync->statedb;
    delete csync;
    delete statedb;
    delete excludedFiles;

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync2");
    assert_int_equal(rc, 0);

    *state = NULL;

    return 0;
}

static int check_file_full(const char *path)
{
    return excludedFiles->fullPatternMatch(path, ItemTypeFile);
}

static int check_dir_full(const char *path)
{
    return excludedFiles->fullPatternMatch(path, ItemTypeDirectory);
}

static int check_file_traversal(const char *path)
{
    return excludedFiles->traversalPatternMatch(path, ItemTypeFile);
}

static int check_dir_traversal(const char *path)
{
    return excludedFiles->traversalPatternMatch(path, ItemTypeDirectory);
}

static void check_csync_exclude_add(void **)
{
    excludedFiles->addManualExclude("/tmp/check_csync1/*");
    assert_int_equal(check_file_full("/tmp/check_csync1/foo"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("/tmp/check_csync2/foo"), CSYNC_NOT_EXCLUDED);
    assert_true(excludedFiles->_allExcludes.contains("/tmp/check_csync1/*"));

    assert_true(excludedFiles->_fullRegexFile.pattern().contains("csync1"));
    assert_true(excludedFiles->_fullTraversalRegexFile.pattern().contains("csync1"));
    assert_false(excludedFiles->_bnameTraversalRegexFile.pattern().contains("csync1"));

    excludedFiles->addManualExclude("foo");
    assert_true(excludedFiles->_bnameTraversalRegexFile.pattern().contains("foo"));
    assert_true(excludedFiles->_fullRegexFile.pattern().contains("foo"));
    assert_false(excludedFiles->_fullTraversalRegexFile.pattern().contains("foo"));
}

static void check_csync_excluded(void **)
{
    assert_int_equal(check_file_full(""), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("/"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("A"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("krawel_krawel"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full(".kde/share/config/kwin.eventsrc"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full(".directory/cache-maximegalon/cache1.txt"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_full("mozilla/.directory"), CSYNC_FILE_EXCLUDE_LIST);

    /*
     * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
     * to be found in top dir as well as in directories underneath.
     */
    assert_int_equal(check_dir_full(".apdisk"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_full("foo/.apdisk"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_full("foo/bar/.apdisk"), CSYNC_FILE_EXCLUDE_LIST);

    assert_int_equal(check_file_full(".java"), CSYNC_NOT_EXCLUDED);

    /* Files in the ignored dir .java will also be ignored. */
    assert_int_equal(check_file_full(".apdisk/totally_amazing.jar"), CSYNC_FILE_EXCLUDE_LIST);

    /* and also in subdirs */
    assert_int_equal(check_file_full("projects/.apdisk/totally_amazing.jar"), CSYNC_FILE_EXCLUDE_LIST);

    /* csync-journal is ignored in general silently. */
    assert_int_equal(check_file_full(".csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_full(".csync_journal.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_full("subdir/.csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

    /* also the new form of the database name */
    assert_int_equal(check_file_full("._sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_full("._sync_5bdd60bdfcfa.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_full("._sync_5bdd60bdfcfa.db-shm"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_full("subdir/._sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

    assert_int_equal(check_file_full(".sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_full(".sync_5bdd60bdfcfa.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_full(".sync_5bdd60bdfcfa.db-shm"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_full("subdir/.sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);


    /* pattern ]*.directory - ignore and remove */
    assert_int_equal(check_file_full("my.~directory"), CSYNC_FILE_EXCLUDE_AND_REMOVE);
    assert_int_equal(check_file_full("/a_folder/my.~directory"), CSYNC_FILE_EXCLUDE_AND_REMOVE);

    /* Not excluded because the pattern .netscape/cache requires directory. */
    assert_int_equal(check_file_full(".netscape/cache"), CSYNC_NOT_EXCLUDED);

    /* Not excluded  */
    assert_int_equal(check_file_full("unicode/中文.hé"), CSYNC_NOT_EXCLUDED);
    /* excluded  */
    assert_int_equal(check_file_full("unicode/пятницы.txt"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("unicode/中文.💩"), CSYNC_FILE_EXCLUDE_LIST);

    /* path wildcards */
    assert_int_equal(check_file_full("foobar/my_manuscript.out"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("latex_tmp/my_manuscript.run.xml"), CSYNC_FILE_EXCLUDE_LIST);

    assert_int_equal(check_file_full("word_tmp/my_manuscript.run.xml"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_full("latex/my_manuscript.tex.tmp"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_full("latex/songbook/my_manuscript.tex.tmp"), CSYNC_FILE_EXCLUDE_LIST);

#ifdef _WIN32
    assert_int_equal(exclude_check_file("file_trailing_space "), CSYNC_FILE_EXCLUDE_TRAILING_SPACE);

    assert_int_equal(exclude_check_file("file_trailing_dot."), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
    assert_int_equal(exclude_check_file("AUX"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
    assert_int_equal(exclude_check_file("file_invalid_char<"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
#endif

    /* ? character */
    excludedFiles->addManualExclude("bond00?");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_file_full("bond00"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("bond007"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("bond0071"), CSYNC_NOT_EXCLUDED);

    /* brackets */
    excludedFiles->addManualExclude("a [bc] d");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_file_full("a d d"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("a  d"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("a b d"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("a c d"), CSYNC_FILE_EXCLUDE_LIST);

    /* escapes */
    excludedFiles->addManualExclude("a \\*");
    excludedFiles->addManualExclude("b \\?");
    excludedFiles->addManualExclude("c \\[d]");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_file_full("a \\*"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("a bc"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("a *"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("b \\?"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("b f"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("b ?"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("c \\[d]"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("c d"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_full("c [d]"), CSYNC_FILE_EXCLUDE_LIST);
}

static void check_csync_excluded_traversal(void **)
{
    assert_int_equal(check_file_traversal(""), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("/"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_traversal("A"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_traversal("krawel_krawel"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal(".kde/share/config/kwin.eventsrc"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_dir_traversal("mozilla/.directory"), CSYNC_FILE_EXCLUDE_LIST);

    /*
     * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
     * to be found in top dir as well as in directories underneath.
     */
    assert_int_equal(check_dir_traversal(".apdisk"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_traversal("foo/.apdisk"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_traversal("foo/bar/.apdisk"), CSYNC_FILE_EXCLUDE_LIST);

    assert_int_equal(check_file_traversal(".java"), CSYNC_NOT_EXCLUDED);

    /* csync-journal is ignored in general silently. */
    assert_int_equal(check_file_traversal(".csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal(".csync_journal.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal("subdir/.csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal("/two/subdir/.csync_journal.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

    /* also the new form of the database name */
    assert_int_equal(check_file_traversal("._sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal("._sync_5bdd60bdfcfa.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal("._sync_5bdd60bdfcfa.db-shm"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal("subdir/._sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

    assert_int_equal(check_file_traversal(".sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal(".sync_5bdd60bdfcfa.db.ctmp"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal(".sync_5bdd60bdfcfa.db-shm"), CSYNC_FILE_SILENTLY_EXCLUDED);
    assert_int_equal(check_file_traversal("subdir/.sync_5bdd60bdfcfa.db"), CSYNC_FILE_SILENTLY_EXCLUDED);

    /* Other builtin excludes */
    assert_int_equal(check_file_traversal("foo/Desktop.ini"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("Desktop.ini"), CSYNC_FILE_SILENTLY_EXCLUDED);

    /* pattern ]*.directory - ignore and remove */
    assert_int_equal(check_file_traversal("my.~directory"), CSYNC_FILE_EXCLUDE_AND_REMOVE);
    assert_int_equal(check_file_traversal("/a_folder/my.~directory"), CSYNC_FILE_EXCLUDE_AND_REMOVE);

    /* Not excluded because the pattern .netscape/cache requires directory. */
    assert_int_equal(check_file_traversal(".netscape/cache"), CSYNC_NOT_EXCLUDED);

    /* Not excluded  */
    assert_int_equal(check_file_traversal("unicode/中文.hé"), CSYNC_NOT_EXCLUDED);
    /* excluded  */
    assert_int_equal(check_file_traversal("unicode/пятницы.txt"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("unicode/中文.💩"), CSYNC_FILE_EXCLUDE_LIST);

    /* path wildcards */
    assert_int_equal(check_file_traversal("foobar/my_manuscript.out"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("latex_tmp/my_manuscript.run.xml"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("word_tmp/my_manuscript.run.xml"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("latex/my_manuscript.tex.tmp"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("latex/songbook/my_manuscript.tex.tmp"), CSYNC_FILE_EXCLUDE_LIST);

#ifdef _WIN32
    assert_int_equal(check_file_traversal("file_trailing_space "), CSYNC_FILE_EXCLUDE_TRAILING_SPACE);
    assert_int_equal(check_file_traversal("file_trailing_dot."), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
    assert_int_equal(check_file_traversal("AUX"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
    assert_int_equal(check_file_traversal("file_invalid_char<"), CSYNC_FILE_EXCLUDE_INVALID_CHAR);
#endif


    /* From here the actual traversal tests */

    excludedFiles->addManualExclude("/exclude");
    excludedFiles->reloadExcludeFiles();

    /* Check toplevel dir, the pattern only works for toplevel dir. */
    assert_int_equal(check_dir_traversal("/exclude"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_traversal("/foo/exclude"), CSYNC_NOT_EXCLUDED);

    /* check for a file called exclude. Must still work */
    assert_int_equal(check_file_traversal("/exclude"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("/foo/exclude"), CSYNC_NOT_EXCLUDED);

    /* Add an exclude for directories only: excl/ */
    excludedFiles->addManualExclude("excl/");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_dir_traversal("/excl"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_traversal("meep/excl"), CSYNC_FILE_EXCLUDE_LIST);

    // because leading dirs aren't checked!
    assert_int_equal(check_file_traversal("meep/excl/file"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("/excl"), CSYNC_NOT_EXCLUDED);

    excludedFiles->addManualExclude("/excludepath/withsubdir");
    excludedFiles->reloadExcludeFiles();

    assert_int_equal(check_dir_traversal("/excludepath/withsubdir"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("/excludepath/withsubdir"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_traversal("/excludepath/withsubdir2"), CSYNC_NOT_EXCLUDED);

    // because leading dirs aren't checked!
    assert_int_equal(check_dir_traversal("/excludepath/withsubdir/foo"), CSYNC_NOT_EXCLUDED);

    /* Check ending of pattern */
    assert_int_equal(check_file_traversal("/exclude"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("/excludeX"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("exclude"), CSYNC_NOT_EXCLUDED);

    excludedFiles->addManualExclude("exclude");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_file_traversal("exclude"), CSYNC_FILE_EXCLUDE_LIST);

    /* ? character */
    excludedFiles->addManualExclude("bond00?");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_file_traversal("bond00"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("bond007"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("bond0071"), CSYNC_NOT_EXCLUDED);

    /* brackets */
    excludedFiles->addManualExclude("a [bc] d");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_file_traversal("a d d"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("a  d"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("a b d"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("a c d"), CSYNC_FILE_EXCLUDE_LIST);

    /* escapes */
    excludedFiles->addManualExclude("a \\*");
    excludedFiles->addManualExclude("b \\?");
    excludedFiles->addManualExclude("c \\[d]");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_file_traversal("a \\*"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("a bc"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("a *"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("b \\?"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("b f"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("b ?"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("c \\[d]"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("c d"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("c [d]"), CSYNC_FILE_EXCLUDE_LIST);
}

static void check_csync_dir_only(void **)
{
    excludedFiles->addManualExclude("filedir");
    excludedFiles->addManualExclude("dir/");

    assert_int_equal(check_file_traversal("other"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("filedir"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("dir"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("s/other"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_file_traversal("s/filedir"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("s/dir"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_dir_traversal("other"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_dir_traversal("filedir"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_traversal("dir"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_traversal("s/other"), CSYNC_NOT_EXCLUDED);
    assert_int_equal(check_dir_traversal("s/filedir"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_traversal("s/dir"), CSYNC_FILE_EXCLUDE_LIST);

    assert_int_equal(check_dir_full("filedir/foo"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("filedir/foo"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_full("dir/foo"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("dir/foo"), CSYNC_FILE_EXCLUDE_LIST);
}

static void check_csync_pathes(void **)
{
    excludedFiles->addManualExclude("/exclude");
    excludedFiles->reloadExcludeFiles();

    /* Check toplevel dir, the pattern only works for toplevel dir. */
    assert_int_equal(check_dir_full("/exclude"), CSYNC_FILE_EXCLUDE_LIST);

    assert_int_equal(check_dir_full("/foo/exclude"), CSYNC_NOT_EXCLUDED);

    /* check for a file called exclude. Must still work */
    assert_int_equal(check_file_full("/exclude"), CSYNC_FILE_EXCLUDE_LIST);

    assert_int_equal(check_file_full("/foo/exclude"), CSYNC_NOT_EXCLUDED);

    /* Add an exclude for directories only: excl/ */
    excludedFiles->addManualExclude("excl/");
    excludedFiles->reloadExcludeFiles();
    assert_int_equal(check_dir_full("/excl"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_dir_full("meep/excl"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("meep/excl/file"), CSYNC_FILE_EXCLUDE_LIST);

    assert_int_equal(check_file_full("/excl"), CSYNC_NOT_EXCLUDED);

    excludedFiles->addManualExclude("/excludepath/withsubdir");
    excludedFiles->reloadExcludeFiles();

    assert_int_equal(check_dir_full("/excludepath/withsubdir"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_full("/excludepath/withsubdir"), CSYNC_FILE_EXCLUDE_LIST);

    assert_int_equal(check_dir_full("/excludepath/withsubdir2"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_dir_full("/excludepath/withsubdir/foo"), CSYNC_FILE_EXCLUDE_LIST);
}

static void check_csync_wildcards(void **)
{
    excludedFiles->addManualExclude("a/foo*bar");
    excludedFiles->addManualExclude("b/foo*bar*");
    excludedFiles->addManualExclude("c/foo?bar");
    excludedFiles->addManualExclude("d/foo?bar*");
    excludedFiles->addManualExclude("e/foo?bar?");
    excludedFiles->addManualExclude("g/bar*");
    excludedFiles->addManualExclude("h/bar?");

    excludedFiles->setWildcardsMatchSlash(false);

    assert_int_equal(check_file_traversal("a/fooXYZbar"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("a/fooX/Zbar"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_traversal("b/fooXYZbarABC"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("b/fooX/ZbarABC"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_traversal("c/fooXbar"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("c/foo/bar"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_traversal("d/fooXbarABC"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("d/foo/barABC"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_traversal("e/fooXbarA"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("e/foo/barA"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_traversal("g/barABC"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("g/XbarABC"), CSYNC_NOT_EXCLUDED);

    assert_int_equal(check_file_traversal("h/barZ"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("h/XbarZ"), CSYNC_NOT_EXCLUDED);

    excludedFiles->setWildcardsMatchSlash(true);

    assert_int_equal(check_file_traversal("a/fooX/Zbar"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("b/fooX/ZbarABC"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("c/foo/bar"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("d/foo/barABC"), CSYNC_FILE_EXCLUDE_LIST);
    assert_int_equal(check_file_traversal("e/foo/barA"), CSYNC_FILE_EXCLUDE_LIST);
}

static void check_csync_regex_translation(void **)
{
    QByteArray storage;
    auto translate = [&storage](const char *pattern) {
        storage = convertToRegexpSyntax(pattern, false).toUtf8();
        return storage.constData();
    };

    assert_string_equal(translate(""), "");
    assert_string_equal(translate("abc"), "abc");
    assert_string_equal(translate("a*c"), "a[^/]*c");
    assert_string_equal(translate("a?c"), "a[^/]c");
    assert_string_equal(translate("a[xyz]c"), "a[xyz]c");
    assert_string_equal(translate("a[xyzc"), "a\\[xyzc");
    assert_string_equal(translate("a[!xyz]c"), "a[^xyz]c");
    assert_string_equal(translate("a\\*b\\?c\\[d\\\\e"), "a\\*b\\?c\\[d\\\\e");
    assert_string_equal(translate("a.c"), "a\\.c");
    assert_string_equal(translate("?𠜎?"), "[^/]\\𠜎[^/]"); // 𠜎 is 4-byte utf8
}

static void check_csync_bname_trigger(void **)
{
    bool wildcardsMatchSlash = false;
    QByteArray storage;
    auto translate = [&storage, &wildcardsMatchSlash](const char *pattern) {
        storage = extractBnameTrigger(pattern, wildcardsMatchSlash).toUtf8();
        return storage.constData();
    };

    assert_string_equal(translate(""), "");
    assert_string_equal(translate("a/b/"), "");
    assert_string_equal(translate("a/b/c"), "c");
    assert_string_equal(translate("c"), "c");
    assert_string_equal(translate("a/foo*"), "foo*");
    assert_string_equal(translate("a/abc*foo*"), "abc*foo*");

    wildcardsMatchSlash = true;

    assert_string_equal(translate(""), "");
    assert_string_equal(translate("a/b/"), "");
    assert_string_equal(translate("a/b/c"), "c");
    assert_string_equal(translate("c"), "c");
    assert_string_equal(translate("*"), "*");
    assert_string_equal(translate("a/foo*"), "foo*");
    assert_string_equal(translate("a/abc?foo*"), "*foo*");
    assert_string_equal(translate("a/abc*foo*"), "*foo*");
    assert_string_equal(translate("a/abc?foo?"), "*foo?");
    assert_string_equal(translate("a/abc*foo?*"), "*foo?*");
    assert_string_equal(translate("a/abc*/foo*"), "foo*");
}

static void check_csync_is_windows_reserved_word(void **)
{
    assert_true(csync_is_windows_reserved_word("CON"));
    assert_true(csync_is_windows_reserved_word("con"));
    assert_true(csync_is_windows_reserved_word("CON."));
    assert_true(csync_is_windows_reserved_word("con."));
    assert_true(csync_is_windows_reserved_word("CON.ference"));
    assert_false(csync_is_windows_reserved_word("CONference"));
    assert_false(csync_is_windows_reserved_word("conference"));
    assert_false(csync_is_windows_reserved_word("conf.erence"));
    assert_false(csync_is_windows_reserved_word("co"));

    assert_true(csync_is_windows_reserved_word("COM2"));
    assert_true(csync_is_windows_reserved_word("com2"));
    assert_true(csync_is_windows_reserved_word("COM2."));
    assert_true(csync_is_windows_reserved_word("com2."));
    assert_true(csync_is_windows_reserved_word("COM2.ference"));
    assert_false(csync_is_windows_reserved_word("COM2ference"));
    assert_false(csync_is_windows_reserved_word("com2ference"));
    assert_false(csync_is_windows_reserved_word("com2f.erence"));
    assert_false(csync_is_windows_reserved_word("com"));

    assert_true(csync_is_windows_reserved_word("CLOCK$"));
    assert_true(csync_is_windows_reserved_word("$Recycle.Bin"));
    assert_true(csync_is_windows_reserved_word("ClocK$"));
    assert_true(csync_is_windows_reserved_word("$recycle.bin"));

    assert_true(csync_is_windows_reserved_word("A:"));
    assert_true(csync_is_windows_reserved_word("a:"));
    assert_true(csync_is_windows_reserved_word("z:"));
    assert_true(csync_is_windows_reserved_word("Z:"));
    assert_true(csync_is_windows_reserved_word("M:"));
    assert_true(csync_is_windows_reserved_word("m:"));
}

/* QT_ENABLE_REGEXP_JIT=0 to get slower results :-) */
static void check_csync_excluded_performance(void **)
{
    const int N = 10000;
    int totalRc = 0;
    int i = 0;

    // Being able to use QElapsedTimer for measurement would be nice...
    {
        struct timeval before, after;
        gettimeofday(&before, 0);

        for (i = 0; i < N; ++i) {
            totalRc += check_dir_full("/this/is/quite/a/long/path/with/many/components");
            totalRc += check_file_full("/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/27/29");
        }
        assert_int_equal(totalRc, CSYNC_NOT_EXCLUDED); // mainly to avoid optimization

        gettimeofday(&after, 0);

        const double total = (after.tv_sec - before.tv_sec)
                + (after.tv_usec - before.tv_usec) / 1.0e6;
        const double perCallMs = total / 2 / N * 1000;
        printf("csync_excluded: %f ms per call\n", perCallMs);
    }

    {
        struct timeval before, after;
        gettimeofday(&before, 0);

        for (i = 0; i < N; ++i) {
            totalRc += check_dir_traversal("/this/is/quite/a/long/path/with/many/components");
            totalRc += check_file_traversal("/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/27/29");
        }
        assert_int_equal(totalRc, CSYNC_NOT_EXCLUDED); // mainly to avoid optimization

        gettimeofday(&after, 0);

        const double total = (after.tv_sec - before.tv_sec)
                + (after.tv_usec - before.tv_usec) / 1.0e6;
        const double perCallMs = total / 2 / N * 1000;
        printf("csync_excluded_traversal: %f ms per call\n", perCallMs);
    }
}

static void check_csync_exclude_expand_escapes(void **state)
{
    (void)state;

    QByteArray line = "keep \\' \\\" \\? \\\\ \\a \\b \\f \\n \\r \\t \\v \\z \\#";
    csync_exclude_expand_escapes(line);
    assert_true(0 == strcmp(line.constData(), "keep ' \" ? \\\\ \a \b \f \n \r \t \v \\z #"));

    line = "";
    csync_exclude_expand_escapes(line);
    assert_true(0 == strcmp(line.constData(), ""));

    line = "\\";
    csync_exclude_expand_escapes(line);
    assert_true(0 == strcmp(line.constData(), "\\"));
}

static void check_placeholder_exclude(void **state)
{
    (void)state;

    auto readFile = [](const QString &file) {
        QFile f(file);
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        return f.readAll();
    };

    QTemporaryDir tempDir;
    QString path;
    QByteArray expected = "\n#!version < 2.5.0\n*.owncloud\n";

    // Case 1: No file exists yet, parent dirs are missing too
    path = tempDir.path() + "/foo/bar/exclude.lst";
    ExcludedFiles::setupPlaceholderExclude(path, ".owncloud");

    assert_true(QFile::exists(path));
    assert_true(readFile(path) == expected);

    // Case 2: Running it again
    ExcludedFiles::setupPlaceholderExclude(path, ".owncloud");
    assert_true(readFile(path) == expected);

    // Case 3: File exists, has some data
    {
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write("# bla\nmyexclude\n\nanotherexclude");
        f.close();
    }
    ExcludedFiles::setupPlaceholderExclude(path, ".owncloud");
    assert_true(readFile(path) == "# bla\nmyexclude\n\nanotherexclude" + expected);

    // Case 4: Running it again still does nothing
    ExcludedFiles::setupPlaceholderExclude(path, ".owncloud");
    assert_true(readFile(path) == "# bla\nmyexclude\n\nanotherexclude" + expected);

    // Case 5: Verify that reading this file doesn't actually include the exclude
    ExcludedFiles excludes;
    excludes.addExcludeFilePath(path);
    excludes.reloadExcludeFiles();
    assert_false(excludes._allExcludes.contains("*.owncloud"));
    assert_true(excludes._allExcludes.contains("myexclude"));
}

static void check_version_directive(void **state)
{
    (void)state;

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
        assert_true(excludes.versionDirectiveKeepNextLine(test.first) == test.second);
    }
}

}; // class ExcludedFilesTest

int torture_run_tests(void)
{
    typedef ExcludedFilesTest T;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(T::check_csync_exclude_add, T::setup, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_excluded, T::setup_init, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_excluded_traversal, T::setup_init, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_dir_only, T::setup, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_pathes, T::setup_init, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_wildcards, T::setup, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_regex_translation, T::setup, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_bname_trigger, T::setup, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_is_windows_reserved_word, T::setup_init, T::teardown),
        cmocka_unit_test_setup_teardown(T::check_csync_excluded_performance, T::setup_init, T::teardown),
        cmocka_unit_test(T::check_csync_exclude_expand_escapes),
        cmocka_unit_test(T::check_placeholder_exclude),
        cmocka_unit_test(T::check_version_directive),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
