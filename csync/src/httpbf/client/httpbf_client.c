/*
 * httpbf - send big files via http
 *
 * Copyright (c) 2012 Klaas Freitag <freitag@owncloud.com>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <neon/ne_auth.h>

#include "httpbf.h"

/* Program documentation. */
static char doc[] = "Usage: httpbf [OPTION...] LOCAL REMOTEDIR\n\
httpbf - command line client to upload big files via http.\n\
\n\
Transfer a big file to a remote directory on ownCloud.\n\
\n\
-?, --help                 Give this help list\n\
    --usage                Give a short usage message\n\
-V, --version              Print program version\n\
";

static char _user[NE_ABUFSIZ];
static char _pwd[NE_ABUFSIZ];

/* The options we understand. */
static const struct option long_options[] =
{
    {"version",         no_argument,       0, 'V' },
    {"usage",           no_argument,       0, 'h' },
    {0, 0, 0, 0}
};

static const char* httpbf_version = "0.1";

static void print_version()
{
    printf( "%s\n", httpbf_version );
    exit(0);
}

static void print_help()
{
    printf( "%s\n", doc );
    exit(0);
}

static int ne_auth( void *userdata, const char *realm, int attempt,
                    char *username, char *password)
{
    (void) userdata;
    (void) realm;

    if( username && password ) {
        if( _user ) {
            /* allow user without password */
            if( strlen( _user ) < NE_ABUFSIZ ) {
                strcpy( username, _user );
            }
            if( _pwd && strlen( _pwd ) < NE_ABUFSIZ ) {
                strcpy( password, _pwd );
            }
        }
    }
    return attempt;
}


static int parse_args(int argc, char **argv)
{
    while(optind < argc) {
        int c = -1;
        struct option *opt = NULL;
        int result = getopt_long( argc, argv, "Vh", long_options, &c );

        if( result == -1 ) {
            break;
        }

        switch(result) {
        case 'V':
            print_version();
            break;
        case 'h':
            print_help();
            break;
        case 0:
            opt = (struct option*)&(long_options[c]);
            if(strcmp(opt->name, "no-name-yet")) {

            } else {
                fprintf(stderr, "Argument: No idea what!\n");
            }
            break;
        default:
            break;
        }
    }
    return optind;
}

static ne_session* create_neon_session( const char *url )
{
    ne_uri uri;
    ne_session *sess = NULL;

    memset( _user, 0, NE_ABUFSIZ );
    memset( _pwd, 0, NE_ABUFSIZ );

    if( ne_uri_parse( url, &uri ) == 0 ) {
        unsigned int port = ne_uri_defaultport(uri.scheme);
        if( uri.userinfo ) {
            char *slash = NULL;
            strcpy( _user, uri.userinfo );
            slash = strchr( _user, ':');
            if( slash ) {
                strcpy( _pwd, slash+1);
                *slash = 0;
            }
        }
        sess = ne_session_create(uri.scheme, uri.host, port);
        ne_set_server_auth(sess, ne_auth, 0 );

        ne_uri_free(&uri);
    }
    return sess;
}

static int open_local_file( const char *file )
{
    int fd = -1;

    if( !file ) return -1;

    fd = open(file, O_RDONLY);
    return fd;
}

static void transfer( const char* local, const char* uri )
{
    if( !(local && uri )) return;
    char *whole_url;
    int len;
    char *filename = basename(local);
    if( ! filename ) {
        return;
    }

    len = strlen(filename)+strlen(uri)+2;
    whole_url = malloc( len );
    strcpy(whole_url, uri);
    strcat(whole_url, "/");
    strcat(whole_url, filename);

    hbf_transfer_t *trans = hbf_init_transfer( whole_url );
    Hbf_State state;

    if( trans ) {
        ne_session *session = create_neon_session(uri);
        if( session ) {
            int fd = open_local_file( local );
            if( fd > -1 ) {
                state = hbf_splitlist(trans, fd );
                if( state == HBF_SUCCESS ) {
                    state = hbf_transfer( session, trans, "PUT" );
                }
            }
            ne_session_destroy(session);
        }
    }

    if( state != HBF_SUCCESS ) {
        printf("Upload failed: %s\n", hbf_error_string(state));
        printf("   HTTP result %d, Server Error: %s\n",
               trans->status_code, trans->error_string ? trans->error_string : "<empty>" );
    }
    /* Print the result of the recent transfer */
    hbf_free_transfer( trans );
    free(whole_url);
}

int main(int argc, char **argv) {
  int rc = 0;
  char errbuf[256] = {0};

  parse_args(argc, argv);
  /* two options must remain as source and target       */
  /* printf("ARGC: %d -> optind: %d\n", argc, optind ); */
  if( argc - optind < 2 ) {
      print_help();
  }

  transfer( argv[optind], argv[optind+1]);

}

/* vim: set ts=8 sw=2 et cindent: */
