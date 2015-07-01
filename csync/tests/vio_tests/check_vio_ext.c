/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2015-2013 by Klaas Freitag <freitag@owncloud.com>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "torture.h"

#include "csync_private.h"
#include "vio/csync_vio.h"

#ifdef _WIN32
#define CSYNC_TEST_DIR "C:/tmp/csync_test"
#else
#define CSYNC_TEST_DIR "/tmp/csync_test"
#endif
#define MKDIR_MASK (S_IRWXU |S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

#define WD_BUFFER_SIZE 255

static mbchar_t wd_buffer[WD_BUFFER_SIZE];

typedef struct {
    CSYNC *csync;
    char  *result;
} statevar;

/* remove the complete test dir */
static int wipe_testdir()
{
  int rc = 0;

#ifdef _WIN32
   /* The windows system call to rd bails out if the dir is not existing
    * Check first.
    */
   WIN32_FIND_DATA FindFileData;

   mbchar_t *dir = c_utf8_to_locale(CSYNC_TEST_DIR);
   HANDLE handle = FindFirstFile(dir, &FindFileData);
   c_free_locale_string(dir);
   int found = handle != INVALID_HANDLE_VALUE;

   if(found) {
       FindClose(handle);
       rc = system("rd /s /q C:\\tmp\\csync_test");
   }
#else
    rc = system("rm -rf /tmp/csync_test/");
#endif
    return rc;
}

static void setup_testenv(void **state) {
    int rc;

    rc = wipe_testdir();
    assert_int_equal(rc, 0);

    mbchar_t *dir = c_utf8_to_locale(CSYNC_TEST_DIR);

    rc = _tmkdir(dir, MKDIR_MASK);
    assert_int_equal(rc, 0);

    assert_non_null(_tgetcwd(wd_buffer, WD_BUFFER_SIZE));

    rc = _tchdir(dir);
    assert_int_equal(rc, 0);

    c_free_locale_string(dir);

    /* --- initialize csync */
    statevar *mystate = malloc( sizeof(statevar) );
    mystate->result = NULL;

    rc = csync_create(&(mystate->csync), "/tmp/csync1", "/tmp/csync2");
    assert_int_equal(rc, 0);

    mystate->csync->replica = LOCAL_REPLICA;

    *state = mystate;
}

static void teardown(void **state) {
    statevar *sv = (statevar*) *state;
    CSYNC *csync = sv->csync;
    int rc;

    printf("================== Tearing down!\n");

    rc = csync_destroy(csync);
    assert_int_equal(rc, 0);

    rc = _tchdir(wd_buffer);
    assert_int_equal(rc, 0);

    rc = wipe_testdir();
    assert_int_equal(rc, 0);

    *state = NULL;
}

/* This function takes a relative path, prepends it with the CSYNC_TEST_DIR
 * and creates each sub directory.
 */
static void create_dirs( const char *path )
{
  int rc;
  char *mypath = c_malloc( 2+strlen(CSYNC_TEST_DIR)+strlen(path));
  *mypath = '\0';
  strcat(mypath, CSYNC_TEST_DIR);
  strcat(mypath, "/");
  strcat(mypath, path);

  char *p = mypath+strlen(CSYNC_TEST_DIR)+1; /* start behind the offset */
  int i = 0;

  assert_non_null(path);

  while( *(p+i) ) {
    if( *(p+i) == '/' ) {
      p[i] = '\0';

      mbchar_t *mb_dir = c_utf8_to_locale(mypath);
      /* wprintf(L"OOOO %ls (%ld)\n", mb_dir, strlen(mypath)); */
      rc = _tmkdir(mb_dir, MKDIR_MASK);
      c_free_locale_string(mb_dir);

      assert_int_equal(rc, 0);
      p[i] = '/';
    }
    i++;
  }
  SAFE_FREE(mypath);
}

/*
 * This function uses the vio_opendir, vio_readdir and vio_closedir functions
 * to traverse a file tree that was created before by the create_dir function.
 *
 * It appends a listing to the result member of the incoming struct in *state
 * that can be compared later to what was expected in the calling functions.
 *
 */
static void traverse_dir(void **state, const char *dir)
{
    csync_vio_handle_t *dh;
    csync_vio_file_stat_t *dirent;
    statevar *sv = (statevar*) *state;
    CSYNC *csync = sv->csync;
    char *subdir;
    char *subdir_out;
    int rc;
    int is_dir;

    /* Format: Smuggle in the C: for unix platforms as its urgently needed
     * on Windows and the test can be nicely cross platform this way. */
#ifdef _WIN32
    const char *format_str = "%s %s";
#else
    const char *format_str = "%s C:%s";
#endif

    dh = csync_vio_opendir(csync, dir);
    assert_non_null(dh);

    while( (dirent = csync_vio_readdir(csync, dh)) ) {
        assert_non_null(dirent->name);

        assert_int_equal( dirent->fields & CSYNC_VIO_FILE_STAT_FIELDS_TYPE, CSYNC_VIO_FILE_STAT_FIELDS_TYPE );

        if( c_streq( dirent->name, "..") || c_streq( dirent->name, "." )) {
          continue;
        }

        is_dir = (dirent->type == CSYNC_VIO_FILE_TYPE_DIRECTORY) ? 1:0;

        assert_int_not_equal( asprintf( &subdir, "%s/%s", dir, dirent->name ), -1 );

        assert_int_not_equal( asprintf( &subdir_out, format_str,
                                        is_dir ? "<DIR>":"    ",
                                        subdir), -1 );

        if( !sv->result ) {
            sv->result = c_strdup( subdir_out);
        } else {
            int newlen = 1+strlen(sv->result)+strlen(subdir_out);
            char *tmp = sv->result;
            sv->result = c_malloc(newlen);
            strcpy( sv->result, tmp);
            SAFE_FREE(tmp);

            strcat( sv->result, subdir_out );
        }
        printf("%s\n", subdir_out);
        traverse_dir( state, subdir);

        SAFE_FREE(subdir);
        SAFE_FREE(subdir_out);
    }

    csync_vio_file_stat_destroy(dirent);
    rc = csync_vio_closedir(csync, dh);
    assert_int_equal(rc, 0);

}

static void check_readdir_shorttree(void **state)
{
    statevar *sv = (statevar*) *state;

    const char *t1 = "alibaba/und/die/vierzig/räuber/";
    create_dirs( t1 );

    traverse_dir(state, CSYNC_TEST_DIR);

    assert_string_equal( sv->result,
                         "<DIR> C:/tmp/csync_test/alibaba"
                         "<DIR> C:/tmp/csync_test/alibaba/und"
                         "<DIR> C:/tmp/csync_test/alibaba/und/die"
                         "<DIR> C:/tmp/csync_test/alibaba/und/die/vierzig"
                         "<DIR> C:/tmp/csync_test/alibaba/und/die/vierzig/räuber" );
}

static void check_readdir_longtree(void **state)
{
    statevar *sv = (statevar*) *state;

    /* Strange things here: Compilers only support strings with length of 4k max.
     * The expected result string is longer, so it needs to be split up in r1, r2 and r3
     */

    /* create the test tree */
    const char *t1 = "vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns/KISTE/ooooooooooooooooooooooooooohhhhhh/und/BESSER/ZWEI/Butteln/VOLL RUM/";
    create_dirs( t1 );

    const char  *r1 =
"<DIR> C:/tmp/csync_test/vierzig"
"<DIR> C:/tmp/csync_test/vierzig/mann"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum";


    const char *r2 =
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH";


    const char *r3 =
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns/KISTE"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns/KISTE/ooooooooooooooooooooooooooohhhhhh"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns/KISTE/ooooooooooooooooooooooooooohhhhhh/und"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns/KISTE/ooooooooooooooooooooooooooohhhhhh/und/BESSER"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns/KISTE/ooooooooooooooooooooooooooohhhhhh/und/BESSER/ZWEI"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns/KISTE/ooooooooooooooooooooooooooohhhhhh/und/BESSER/ZWEI/Butteln"
"<DIR> C:/tmp/csync_test/vierzig/mann/auf/des/toten/Mann/kiste/ooooooooooooooooooooooh/and/ne/bottle/voll/rum/und/so/singen/wir/VIERZIG/MANN/AUF/DES/TOTEN/MANNS/KISTE/OOOOOOOOH/AND/NE/BOTTLE/VOLL/RUM/undnochmalallezusammen/VierZig/MannaufDesTotenManns/KISTE/ooooooooooooooooooooooooooohhhhhh/und/BESSER/ZWEI/Butteln/VOLL RUM";

    /* assemble the result string ... */
    int overall_len = 1+strlen(r1)+strlen(r2)+strlen(r3);
    printf("OO Overall expected result len %d\n", overall_len);

    char *result = c_malloc(overall_len);
    *result = '\0';

    strcat(result, r1);
    strcat(result, r2);
    strcat(result, r3);

    traverse_dir(state, CSYNC_TEST_DIR);

    int result_len = strlen(sv->result);
    printf("OO Overall real result string len %d\n", result_len);

    /* and compare. */
    assert_string_equal( sv->result, result);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_readdir_shorttree, setup_testenv, teardown),
        unit_test_setup_teardown(check_readdir_longtree, setup_testenv, teardown),

    };

    return run_tests(tests);
}
