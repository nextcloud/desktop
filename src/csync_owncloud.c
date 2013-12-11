/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2011      by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012      by Klaas Freitag <freitag@owncloud.com>
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

#include "csync_owncloud.h"

#include <inttypes.h>

/*
 * free the fetchCtx
 */
static void free_fetchCtx( struct listdir_context *ctx )
{
    struct resource *newres, *res;
    if( ! ctx ) return;
    newres = ctx->list;
    res = newres;

    ctx->ref--;
    if (ctx->ref > 0) return;

    SAFE_FREE(ctx->target);

    while( res ) {
        SAFE_FREE(res->uri);
        SAFE_FREE(res->name);
        SAFE_FREE(res->md5);
        memset( res->file_id, 0, FILE_ID_BUF_SIZE+1 );

        newres = res->next;
        SAFE_FREE(res);
        res = newres;
    }
    SAFE_FREE(ctx);
}


/*
 * local variables.
 */

struct dav_session_s dav_session; /* The DAV Session, initialised in dav_connect */
int _connected = 0;                   /* flag to indicate if a connection exists, ie.
                                     the dav_session is valid */

csync_auth_callback _authcb;
long long chunked_total_size = 0;
long long chunked_done = 0;

struct listdir_context *propfind_cache = 0;

bool is_first_propfind = true;


csync_vio_file_stat_t _stat_cache;
/* id cache, cache the ETag: header of a GET request */
struct { char *uri; char *id;  } _id_cache = { NULL, NULL };

static void clean_caches() {
    clear_propfind_recursive_cache();

    free_fetchCtx(propfind_cache);
    propfind_cache = NULL;

    SAFE_FREE(_stat_cache.name);
    SAFE_FREE(_stat_cache.etag );
    memset( _stat_cache.file_id, 0, FILE_ID_BUF_SIZE+1 );

    SAFE_FREE(_id_cache.uri);
    SAFE_FREE(_id_cache.id);
}



#define PUT_BUFFER_SIZE 1024*5

char _buffer[PUT_BUFFER_SIZE];

/*
 * helper method to build up a user text for SSL problems, called from the
 * verify_sslcert callback.
 */
static void addSSLWarning( char *ptr, const char *warn, int len )
{
    char *concatHere = ptr;
    int remainingLen = 0;

    if( ! (warn && ptr )) return;
    remainingLen = len - strlen(ptr);
    if( remainingLen <= 0 ) return;
    concatHere = ptr + strlen(ptr);  /* put the write pointer to the end. */
    strncpy( concatHere, warn, remainingLen );
}

/*
 * Callback to verify the SSL certificate, called from libneon.
 * It analyzes the SSL problem, creates a user information text and passes
 * it to the csync callback to ask the user.
 */
#define LEN 4096
static int verify_sslcert(void *userdata, int failures,
                          const ne_ssl_certificate *certificate)
{
    char problem[LEN];
    char buf[MAX(NE_SSL_DIGESTLEN, NE_ABUFSIZ)];
    int ret = -1;
    const ne_ssl_certificate *cert = certificate;

    (void) userdata;
    memset( problem, 0, LEN );

    while( cert ) {

      addSSLWarning( problem, "There are problems with the SSL certificate:\n", LEN );
      if( failures & NE_SSL_NOTYETVALID ) {
        addSSLWarning( problem, " * The certificate is not yet valid.\n", LEN );
      }
      if( failures & NE_SSL_EXPIRED ) {
        addSSLWarning( problem, " * The certificate has expired.\n", LEN );
      }

      if( failures & NE_SSL_UNTRUSTED ) {
        addSSLWarning( problem, " * The certificate is not trusted!\n", LEN );
      }
      if( failures & NE_SSL_IDMISMATCH ) {
        addSSLWarning( problem, " * The hostname for which the certificate was "
                       "issued does not match the hostname of the server\n", LEN );
      }
      if( failures & NE_SSL_BADCHAIN ) {
        addSSLWarning( problem, " * The certificate chain contained a certificate other than the server cert\n", LEN );
      }
      if( failures & NE_SSL_REVOKED ) {
        addSSLWarning( problem, " * The server certificate has been revoked by the issuing authority.\n", LEN );
      }

      if (ne_ssl_cert_digest(cert, buf) == 0) {
        addSSLWarning( problem, "Certificate fingerprint: ", LEN );
        addSSLWarning( problem, buf, LEN );
        addSSLWarning( problem, "\n", LEN );
      }
      cert = ne_ssl_cert_signedby( cert );
    }
    addSSLWarning( problem, "Do you want to accept the certificate chain anyway?\nAnswer yes to do so and take the risk: ", LEN );

    if( _authcb ){
        /* call the csync callback */
        DEBUG_WEBDAV("Call the csync callback for SSL problems");
        memset( buf, 0, NE_ABUFSIZ );
        (*_authcb) ( problem, buf, NE_ABUFSIZ-1, 1, 0, NULL );
        if( buf[0] == 'y' || buf[0] == 'Y') {
            ret = 0;
        } else {
            DEBUG_WEBDAV("Authentication callback replied %s", buf );

        }
    }
    DEBUG_WEBDAV("## VERIFY_SSL CERT: %d", ret  );
      return ret;
}

/*
 * Authentication callback. Is set by ne_set_server_auth to be called
 * from the neon lib to authenticate a request.
 */
static int ne_auth( void *userdata, const char *realm, int attempt,
                    char *username, char *password)
{
    char buf[NE_ABUFSIZ];

    (void) userdata;
    (void) realm;

    /* DEBUG_WEBDAV( "Authentication required %s", realm ); */
    if( username && password ) {
        DEBUG_WEBDAV( "Authentication required %s", username );
        if( dav_session.user ) {
            /* allow user without password */
            if( strlen( dav_session.user ) < NE_ABUFSIZ ) {
                strcpy( username, dav_session.user );
            }
            if( dav_session.pwd && strlen( dav_session.pwd ) < NE_ABUFSIZ ) {
                strcpy( password, dav_session.pwd );
            }
        } else if( _authcb != NULL ){
            /* call the csync callback */
            DEBUG_WEBDAV("Call the csync callback for %s", realm );
            memset( buf, 0, NE_ABUFSIZ );
            (*_authcb) ("Enter your username: ", buf, NE_ABUFSIZ-1, 1, 0, NULL );
            if( strlen(buf) < NE_ABUFSIZ ) {
                strcpy( username, buf );
            }
            memset( buf, 0, NE_ABUFSIZ );
            (*_authcb) ("Enter your password: ", buf, NE_ABUFSIZ-1, 0, 0, NULL );
            if( strlen(buf) < NE_ABUFSIZ) {
                strcpy( password, buf );
            }
        } else {
            DEBUG_WEBDAV("I can not authenticate!");
        }
    }
    return attempt;
}

/*
 * Authentication callback. Is set by ne_set_proxy_auth to be called
 * from the neon lib to authenticate against a proxy. The data to authenticate
 * against comes from mirall throught vio_module_init function.
 */
static int ne_proxy_auth( void *userdata, const char *realm, int attempt,
                          char *username, char *password)
{
    (void) userdata;
    (void) realm;
    if( dav_session.proxy_user && strlen( dav_session.proxy_user ) < NE_ABUFSIZ) {
        strcpy( username, dav_session.proxy_user );
        if( dav_session.proxy_pwd && strlen( dav_session.proxy_pwd ) < NE_ABUFSIZ) {
            strcpy( password, dav_session.proxy_pwd );
        }
    }
    /* NTLM needs several attempts */
    return (attempt < 3) ? 0 : -1;
}

/* Configure the proxy depending on the variables */
static int configureProxy( ne_session *session )
{
    int port = 8080;
    int re = -1;

    if( ! session ) return -1;
    if( ! dav_session.proxy_type ) return 0; /* Go by NoProxy per default */

    if( dav_session.proxy_port > 0 ) {
        port = dav_session.proxy_port;
    }

    if( c_streq(dav_session.proxy_type, "NoProxy" )) {
        DEBUG_WEBDAV("No proxy configured.");
        re = 0;
    } else if( c_streq(dav_session.proxy_type, "DefaultProxy") ||
               c_streq(dav_session.proxy_type, "HttpProxy")    ||
               c_streq(dav_session.proxy_type, "HttpCachingProxy") ||
               c_streq(dav_session.proxy_type, "Socks5Proxy")) {

        if( dav_session.proxy_host ) {
            DEBUG_WEBDAV("%s at %s:%d", dav_session.proxy_type, dav_session.proxy_host, port );
            if (c_streq(dav_session.proxy_type, "Socks5Proxy")) {
                ne_session_socks_proxy(session, NE_SOCK_SOCKSV5, dav_session.proxy_host, port,
                                       dav_session.proxy_user, dav_session.proxy_pwd);
            } else {
                ne_session_proxy(session, dav_session.proxy_host, port );
            }
            re = 2;
        } else {
            DEBUG_WEBDAV("%s requested but no proxy host defined.", dav_session.proxy_type );
	    /* we used to try ne_system_session_proxy here, but we should rather err out
	       to behave exactly like the caller. */
        }
    } else {
        DEBUG_WEBDAV( "Unsupported Proxy: %s", dav_session.proxy_type );
    }

    return re;
}

/*
 * This hook is called for with the response of a request. Here its checked
 * if a Set-Cookie header is there for the PHPSESSID. The key is stored into
 * the webdav session to be added to subsequent requests.
 */
static void post_request_hook(ne_request *req, void *userdata, const ne_status *status)
{
    const char *set_cookie_header = NULL;
    const char *sc  = NULL;
    char *key = NULL;

    (void) userdata;

    if (dav_session.session_key)
        return; /* We already have a session cookie, and we should ignore other ones */

    if(!(status && req)) return;
    if( status->klass == 2 || status->code == 401 ) {
        /* successful request */
        set_cookie_header =  ne_get_response_header( req, "Set-Cookie" );
        if( set_cookie_header ) {
            DEBUG_WEBDAV(" Set-Cookie found: %s", set_cookie_header);
            /* try to find a ', ' sequence which is the separator of neon if multiple Set-Cookie
             * headers are there.
             * The following code parses a string like this:
             * Set-Cookie: 50ace6bd8a669=p537brtt048jh8srlp2tuep7em95nh9u98mj992fbqc47d1aecp1;
             */
            sc = set_cookie_header;
            while(sc) {
                const char *sc_val = sc;
                const char *sc_end = sc_val;
                int cnt = 0;
                int len = strlen(sc_val); /* The length of the rest of the header string. */

                while( cnt < len && *sc_end != ';' && *sc_end != ',') {
                    cnt++;
                    sc_end++;
                }
                if( cnt == len ) {
                    /* exit: We are at the end. */
                    sc = NULL;
                } else if( *sc_end == ';' ) {
                    /* We are at the end of the session key. */
                    int keylen = sc_end-sc_val;
                    if( key ) {
                        int oldlen = strlen(key);
                        key = c_realloc(key, oldlen + 2 + keylen+1);
                        strcpy(key + oldlen, "; ");
                        strncpy(key + oldlen + 2, sc_val, keylen);
                        key[oldlen + 2 + keylen] = '\0';
                    } else {
                        key = c_malloc(keylen+1);
                        strncpy( key, sc_val, keylen );
                        key[keylen] = '\0';
                    }

                    /* now search for a ',' to find a potential other header entry */
                    while(cnt < len && *sc_end != ',') {
                        cnt++;
                        sc_end++;
                    }
                    if( cnt < len )
                        sc = sc_end+2; /* mind the space after the comma */
                    else
                        sc = NULL;
                } else if( *sc_end == ',' ) {
                    /* A new entry is to check. */
                    if( *(sc_end + 1) == ' ') {
                        sc = sc_end+2;
                    } else {
                        /* error condition */
                        sc = NULL;
                    }
                }
            }
        }
    } else {
        DEBUG_WEBDAV("Request failed, don't take session header.");
    }
    if( key ) {
        DEBUG_WEBDAV("----> Session-key: %s", key);
        SAFE_FREE(dav_session.session_key);
        dav_session.session_key = key;
    }
}

/*
 * this hook is called just after a request has been created, before its sent.
 * Here it is used to set the proxy connection header if available.
 */
static void request_created_hook(ne_request *req, void *userdata,
                                 const char *method, const char *requri)
{
    (void) userdata;
    (void) method;
    (void) requri;

    if( !req ) return;

    if(dav_session.proxy_type) {
        /* required for NTLM */
        ne_add_request_header(req, "Proxy-Connection", "Keep-Alive");
    }
}

/*
 * this hook is called just before a request has been sent.
 * Here it is used to set the session cookie if available.
 */
static void pre_send_hook(ne_request *req, void *userdata,
                          ne_buffer *header)
{
    (void) userdata;

    if( !req ) return;

    if(dav_session.session_key) {
        ne_buffer_concat(header, "Cookie: ", dav_session.session_key, "\r\n", NULL);
    }
}

static int post_send_hook(ne_request *req, void *userdata,
                          const ne_status *status)
{
    const char *location;

    (void) userdata;
    (void) status;

    location = ne_get_response_header(req, "Location");

    if( !location ) return NE_OK;

    if( dav_session.redir_callback ) {
        if( dav_session.redir_callback( dav_session.csync_ctx, location ) ) {
            return NE_REDIRECT;
        } else {
            return NE_RETRY;
        }
    }

    return NE_REDIRECT;
}

// as per http://sourceforge.net/p/predef/wiki/OperatingSystems/
// extend as required
static const char* get_platform() {
#if defined (_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "Macintosh";
#elif defined(__gnu_linux__)
    return "Linux";
#elif defined(__DragonFly__)
    /* might also define __FreeBSD__ */
    return "DragonFlyBSD";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__NetBSD__)
    return "NetBSD";
#elif defined(__OpenBSD__)
    return "OpenBSD";
#elif defined(sun) || defined(__sun)
    return "Solaris";
#else
    return "Unknown OS";
#endif
}

/*
 * Connect to a DAV server
 * This function sets the flag _connected if the connection is established
 * and returns if the flag is set, so calling it frequently is save.
 */
static int dav_connect(const char *base_url) {
    int useSSL = 0;
    int rc;
    char protocol[6] = {'\0'};
    char uaBuf[256];
    char *path = NULL;
    char *scheme = NULL;
    char *host = NULL;
    unsigned int port = 0;
    int proxystate = -1;

    if (_connected) {
        return 0;
    }

    rc = c_parse_uri( base_url, &scheme, &dav_session.user, &dav_session.pwd, &host, &port, &path );
    if( rc < 0 ) {
        DEBUG_WEBDAV("Failed to parse uri %s", base_url );
        goto out;
    }

    DEBUG_WEBDAV("* scheme %s", scheme );
    DEBUG_WEBDAV("* host %s", host );
    DEBUG_WEBDAV("* port %u", port );
    DEBUG_WEBDAV("* path %s", path );

    if( strcmp( scheme, "owncloud" ) == 0 ) {
        strcpy( protocol, "http");
    } else if( strcmp( scheme, "ownclouds" ) == 0 ) {
        strcpy( protocol, "https");
        useSSL = 1;
    } else {
        DEBUG_WEBDAV("Invalid scheme %s, go outa here!", scheme );
        rc = -1;
        goto out;
    }

    DEBUG_WEBDAV("* user %s", dav_session.user ? dav_session.user : "");

    if (port == 0) {
        port = ne_uri_defaultport(protocol);
    }

#if 0
    rc = ne_sock_init();
    DEBUG_WEBDAV("ne_sock_init: %d", rc );
    if (rc < 0) {
        rc = -1;
        goto out;
    }
#endif

    dav_session.ctx = ne_session_create( protocol, host, port);

    if (dav_session.ctx == NULL) {
        DEBUG_WEBDAV("Session create with protocol %s failed", protocol );
        rc = -1;
        goto out;
    }

    if (dav_session.read_timeout == 0)
        dav_session.read_timeout = 300;  // set 300 seconds as default.

    ne_set_read_timeout(dav_session.ctx, dav_session.read_timeout);

    snprintf( uaBuf, sizeof(uaBuf), "Mozilla/5.0 (%s) csyncoC/%s",
              get_platform(), CSYNC_STRINGIFY( LIBCSYNC_VERSION ));
    ne_set_useragent( dav_session.ctx, uaBuf);
    ne_set_server_auth(dav_session.ctx, ne_auth, 0 );

    if( useSSL ) {
        if (!ne_has_support(NE_FEATURE_SSL)) {
            DEBUG_WEBDAV("Error: SSL is not enabled.");
            rc = -1;
            goto out;
        }

        ne_ssl_trust_default_ca( dav_session.ctx );
        ne_ssl_set_verify( dav_session.ctx, verify_sslcert, 0 );
    }

    /* Hook called when a request is created. It sets the proxy connection header. */
    ne_hook_create_request( dav_session.ctx, request_created_hook, NULL );
    /* Hook called after response headers are read. It gets the Session ID. */
    ne_hook_post_headers( dav_session.ctx, post_request_hook, NULL );
    /* Hook called before a request is sent. It sets the cookies. */
    ne_hook_pre_send( dav_session.ctx, pre_send_hook, NULL );
    /* Hook called after request is dispatched. Used for handling possible redirections. */
    ne_hook_post_send( dav_session.ctx, post_send_hook, NULL );

    /* Proxy support */
    proxystate = configureProxy( dav_session.ctx );
    if( proxystate < 0 ) {
        DEBUG_WEBDAV("Error: Proxy-Configuration failed.");
    } else if( proxystate > 0 ) {
        ne_set_proxy_auth( dav_session.ctx, ne_proxy_auth, 0 );
    }

    _connected = 1;
    rc = 0;
out:
    SAFE_FREE(path);
    SAFE_FREE(host);
    SAFE_FREE(scheme);
    return rc;
}

/*
 * result parsing list.
 * This function is called to parse the result of the propfind request
 * to list directories on the WebDAV server. I takes a single resource
 * and fills a resource struct and stores it to the result list which
 * is stored in the listdir_context.
 */
static void results(void *userdata,
                    const ne_uri *uri,
                    const ne_prop_result_set *set)
{
    struct listdir_context *fetchCtx = userdata;
    struct resource *newres = 0;
    const char *clength, *modtime = NULL;
    const char *resourcetype = NULL;
    const char *md5sum = NULL;
    const char *file_id = NULL;
    const ne_status *status = NULL;
    char *path = ne_path_unescape( uri->path );

    (void) status;
    if( ! fetchCtx ) {
        DEBUG_WEBDAV("No valid fetchContext");
        return;
    }

    if( ! fetchCtx->target ) {
        DEBUG_WEBDAV("error: target must not be zero!" );
        return;
    }

    /* Fill the resource structure with the data about the file */
    newres = c_malloc(sizeof(struct resource));
    ZERO_STRUCTP(newres);
    newres->uri =  path; /* no need to strdup because ne_path_unescape already allocates */
    newres->name = c_basename( path );

    modtime      = ne_propset_value( set, &ls_props[0] );
    clength      = ne_propset_value( set, &ls_props[1] );
    resourcetype = ne_propset_value( set, &ls_props[2] );
    md5sum       = ne_propset_value( set, &ls_props[3] );
    file_id      = ne_propset_value( set, &ls_props[4] );

    newres->type = resr_normal;
    if( clength == NULL && resourcetype && strncmp( resourcetype, "<DAV:collection>", 16 ) == 0) {
        newres->type = resr_collection;
    }

    if (modtime) {
        newres->modtime = oc_httpdate_parse(modtime);
    }

    /* DEBUG_WEBDAV("Parsing Modtime: %s -> %llu", modtime, (unsigned long long) newres->modtime ); */
    newres->size = 0;
    if (clength) {
        newres->size = atoll(clength);
        /* DEBUG_WEBDAV("Parsed File size for %s from %s: %lld", newres->name, clength, (long long)newres->size ); */
    }

    if( md5sum ) {
        newres->md5 = csync_normalize_etag(md5sum);
    }

    csync_vio_set_file_id(newres->file_id, file_id);

    /* prepend the new resource to the result list */
    newres->next   = fetchCtx->list;
    fetchCtx->list = newres;
    fetchCtx->result_count = fetchCtx->result_count + 1;
    /* DEBUG_WEBDAV( "results for URI %s: %d %d", newres->name, (int)newres->size, (int)newres->type ); */
}



/*
 * fetches a resource list from the WebDAV server. This is equivalent to list dir.
 */
static struct listdir_context *fetch_resource_list(const char *uri, int depth)
{
    struct listdir_context *fetchCtx;
    int ret = 0;
    ne_propfind_handler *hdl = NULL;
    ne_request *request = NULL;
    const char *content_type = NULL;
    char *curi = NULL;
    const ne_status *req_status = NULL;

    curi = _cleanPath( uri );

    /* The old legacy one-level PROPFIND cache. Also gets filled
       by the recursive cache if 'infinity' did not suceed. */
    if (propfind_cache) {
        if (c_streq(curi, propfind_cache->target)) {
            DEBUG_WEBDAV("fetch_resource_list Using simple PROPFIND cache %s", curi);
            propfind_cache->ref++;
            SAFE_FREE(curi);
            return propfind_cache;
        }
    }

    fetchCtx = c_malloc( sizeof( struct listdir_context ));
    if (!fetchCtx) {
        errno = ENOMEM;
        SAFE_FREE(curi);
        return NULL;
    }
    fetchCtx->list = NULL;
    fetchCtx->target = curi;
    fetchCtx->currResource = NULL;
    fetchCtx->ref = 1;

    /* do a propfind request and parse the results in the results function, set as callback */
    hdl = ne_propfind_create(dav_session.ctx, curi, depth);

    if(hdl) {
        ret = ne_propfind_named(hdl, ls_props, results, fetchCtx);
        request = ne_propfind_get_request( hdl );
        req_status = ne_get_status( request );
    }

    if( ret == NE_OK ) {
        fetchCtx->currResource = fetchCtx->list;
        /* Check the request status. */
        if( req_status && req_status->klass != 2 ) {
            set_errno_from_http_errcode(req_status->code);
            DEBUG_WEBDAV("ERROR: Request failed: status %d (%s)", req_status->code,
                         req_status->reason_phrase);
            ret = NE_CONNECT;
            set_error_message(req_status->reason_phrase);
            oc_notify_progress( uri, CSYNC_NOTIFY_ERROR,
                                req_status->code,
                                (intptr_t)(req_status->reason_phrase) );
        }
        DEBUG_WEBDAV("Simple propfind result code %d.", req_status->code);
    } else {
        if( ret == NE_ERROR && req_status->code == 404) {
            errno = ENOENT;
        } else {
            set_errno_from_neon_errcode(ret);
        }
    }

    if( ret == NE_OK ) {
        /* Check the content type. If the server has a problem, ie. database is gone or such,
         * the content type is not xml but a html error message. Stop on processing if it's
         * not XML.
         * FIXME: Generate user error message from the reply content.
         */
        content_type =  ne_get_response_header( request, "Content-Type" );
        if( !(content_type && c_streq(content_type, "application/xml; charset=utf-8") ) ) {
            DEBUG_WEBDAV("ERROR: Content type of propfind request not XML: %s.",
                         content_type ?  content_type: "<empty>");
            errno = ERRNO_WRONG_CONTENT;
            set_error_message("Server error: PROPFIND reply is not XML formatted!");
            ret = NE_CONNECT;
        }
    }

    if( ret != NE_OK ) {
        const char *err = NULL;
        set_errno_from_neon_errcode(ret);

        err = ne_get_error( dav_session.ctx );
        if(err) {
            set_error_message(err);
        }
        DEBUG_WEBDAV("WRN: propfind named failed with %d, request error: %s", ret, err ? err : "<nil>");
    }

    if( hdl )
        ne_propfind_destroy(hdl);

    if( ret != NE_OK ) {
        free_fetchCtx(fetchCtx);
        return NULL;
    }

    free_fetchCtx(propfind_cache);
    propfind_cache = fetchCtx;
    propfind_cache->ref++;
    return fetchCtx;
}

static struct listdir_context *fetch_resource_list_attempts(const char *uri, int depth)
{
    int i;

    struct listdir_context *fetchCtx = NULL;
    for(i = 0; i < 10; ++i) {
        fetchCtx = fetch_resource_list(uri, depth);
        if(fetchCtx) break;
        /* only loop in case the content is not XML formatted. Otherwise for every
         * non successful stat (for non existing directories) its tried 10 times. */
        if( errno != ERRNO_WRONG_CONTENT ) break;

        DEBUG_WEBDAV("=> Errno after fetch resource list for %s: %d", uri, errno);
        DEBUG_WEBDAV("   New attempt %i", i);
    }
    return fetchCtx;
}

static void fill_stat_cache( csync_vio_file_stat_t *lfs ) {

    if( _stat_cache.name ) SAFE_FREE(_stat_cache.name);
    if( _stat_cache.etag  ) SAFE_FREE(_stat_cache.etag );

    if( !lfs) return;

    _stat_cache.name   = c_strdup(lfs->name);
    _stat_cache.mtime  = lfs->mtime;
    _stat_cache.fields = lfs->fields;
    _stat_cache.type   = lfs->type;
    _stat_cache.size   = lfs->size;
    csync_vio_file_stat_set_file_id(&_stat_cache, lfs->file_id);

    if( lfs->etag ) {
        _stat_cache.etag    = c_strdup(lfs->etag);
    }
}



/*
 * file functions
 */
static int owncloud_stat(const char *uri, csync_vio_file_stat_t *buf) {
    /* get props:
     *   modtime
     *   creattime
     *   size
     */
    csync_vio_file_stat_t *lfs = NULL;
    struct listdir_context  *fetchCtx = NULL;
    char *decodedUri = NULL;
    int len = 0;
    errno = 0;

    DEBUG_WEBDAV("owncloud_stat %s called", uri );

    buf->name = c_basename(uri);

    if (buf->name == NULL) {
        errno = ENOMEM;
        return -1;
    }

    if( _stat_cache.name && strcmp( buf->name, _stat_cache.name ) == 0 ) {
        buf->fields  = CSYNC_VIO_FILE_STAT_FIELDS_NONE;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;
        buf->fields  = _stat_cache.fields;
        buf->type    = _stat_cache.type;
        buf->mtime   = _stat_cache.mtime;
        buf->size    = _stat_cache.size;
        buf->mode    = _stat_perms( _stat_cache.type );
        buf->etag     = NULL;
        if( _stat_cache.etag ) {
            buf->etag    = c_strdup( _stat_cache.etag );
            buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ETAG;
        }
        csync_vio_file_stat_set_file_id( buf, _stat_cache.file_id );
        return 0;
    }
    DEBUG_WEBDAV("owncloud_stat => Could not find in stat cache %s", uri);

    /* fetch data via a propfind call. */
    /* fetchCtx = fetch_resource_list( uri, NE_DEPTH_ONE); */
    fetchCtx = fetch_resource_list_attempts( uri, NE_DEPTH_ONE);
    DEBUG_WEBDAV("=> Errno after fetch resource list for %s: %d", uri, errno);
    if (!fetchCtx) {
        return -1;
    }

    if( fetchCtx ) {
        struct resource *res = fetchCtx->list;
        while( res ) {
            /* remove trailing slashes */
            len = strlen(res->uri);
            while( len > 0 && res->uri[len-1] == '/' ) --len;
            decodedUri = ne_path_unescape( fetchCtx->target ); /* allocates memory */

            /* Only do the comparaison of the part of the string without the trailing
               slashes, and make sure decodedUri is not too large */
            if( strncmp(res->uri, decodedUri, len ) == 0 && decodedUri[len] == '\0') {
                SAFE_FREE( decodedUri );
                break;
            }
            res = res->next;
            SAFE_FREE( decodedUri );
        }
        if( res ) {
            DEBUG_WEBDAV("Working on file %s", res->name );
        } else {
            DEBUG_WEBDAV("ERROR: Result struct not valid!");
        }

        lfs = resourceToFileStat( res );
        if( lfs ) {
            buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;
            buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
            buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
            buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
            buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;
            buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ETAG;

            buf->fields = lfs->fields;
            buf->type   = lfs->type;
            buf->mtime  = lfs->mtime;
            buf->size   = lfs->size;
            buf->mode   = _stat_perms( lfs->type );
            buf->etag    = NULL;
            if( lfs->etag ) {
                buf->etag    = c_strdup( lfs->etag );
            }
            csync_vio_file_stat_set_file_id( buf, lfs->file_id );

            /* fill the static stat buf as input for the stat function */
            csync_vio_file_stat_destroy( lfs );
        }

        free_fetchCtx( fetchCtx );
    }
    DEBUG_WEBDAV("STAT result from propfind: %s, mtime: %llu", buf->name ? buf->name:"NULL",
                    (unsigned long long) buf->mtime );

    return 0;
}

/* capabilities are currently:
 *  bool atomar_copy_support - oC provides atomar copy
 *  bool do_post_copy_stat   - oC does not want the post copy check
 *  bool time_sync_required  - oC does not require the time sync
 *  int  unix_extensions     - oC supports unix extensions.
 *  bool propagate_on_fd     - oC supports the send_file method.
 */
static csync_vio_capabilities_t _owncloud_capabilities = { true, false, false, 0, true, false, false };

static csync_vio_capabilities_t *owncloud_capabilities(void)
{
#ifdef _WIN32
  _owncloud_capabilities.unix_extensions = 0;
#endif
  return &_owncloud_capabilities;
}

static const char* owncloud_get_etag( const char *path )
{
    ne_request *req    = NULL;
    const char *header = NULL;
    char *uri          = _cleanPath(path);
    char *cbuf   = NULL;
    csync_vio_file_stat_t *fs = NULL;
    bool doHeadRequest = false;

    if (_id_cache.uri && c_streq(path, _id_cache.uri)) {
        header = _id_cache.id;
    }

    doHeadRequest= false; /* ownCloud server doesn't have good support for HEAD yet */

    if( !header && doHeadRequest ) {
        int neon_stat;
        /* Perform an HEAD request to the resource. HEAD delivers the
         * ETag header back. */
        req = ne_request_create(dav_session.ctx, "HEAD", uri);
        neon_stat = ne_request_dispatch(req);
        set_errno_from_neon_errcode( neon_stat );

        header = ne_get_response_header(req, "etag");
    }
    /* If the request went wrong or the server did not respond correctly
     * (that can happen for collections) a stat call is done which translates
     * into a PROPFIND request.
     */
    if( ! header ) {
        /* ... and do a stat call. */
        fs = csync_vio_file_stat_new();
        if(fs == NULL) {
            DEBUG_WEBDAV( "owncloud_get_etag: memory fault.");
            errno = ENOMEM;
            return NULL;
        }
        if( owncloud_stat( path, fs ) == 0 ) {
            header = fs->etag;
        }
    }

    /* In case the result is surrounded by "" cut them away. */
    if( header ) {
        cbuf = csync_normalize_etag(header);
    }

    /* fix server problem: If we end up with an empty string, set something strange... */
    if( c_streq(cbuf, "") || c_streq(cbuf, "\"\"") ) {
        SAFE_FREE(cbuf);
        cbuf = c_strdup("empty_etag");
    }

    DEBUG_WEBDAV("Get file ID for %s: %s", path, cbuf ? cbuf:"<null>");
    if( fs ) csync_vio_file_stat_destroy(fs);
    if( req ) ne_request_destroy(req);
    SAFE_FREE(uri);


    return cbuf;
}

/*
 * directory functions
 */
static csync_vio_method_handle_t *owncloud_opendir(const char *uri) {
    struct listdir_context *fetchCtx = NULL;
    char *curi = NULL;

    DEBUG_WEBDAV("opendir method called on %s", uri );

    dav_connect( uri );

    curi = _cleanPath( uri );
    if (is_first_propfind && !dav_session.no_recursive_propfind) {
        is_first_propfind = false;
        // Try to fill it
        fill_recursive_propfind_cache(uri, curi);
    }
    if (propfind_recursive_cache) {
        // Try to fetch from recursive cache (if we have one)
        fetchCtx = get_listdir_context_from_recursive_cache(curi);
    }
    SAFE_FREE(curi);
    is_first_propfind = false;
    if (fetchCtx) {
        return fetchCtx;
    }

    /* fetchCtx = fetch_resource_list( uri, NE_DEPTH_ONE ); */
    fetchCtx = fetch_resource_list_attempts( uri, NE_DEPTH_ONE);
    if( !fetchCtx ) {
        /* errno is set properly in fetch_resource_list */
        DEBUG_WEBDAV("Errno set to %d", errno);
        return NULL;
    } else {
        fetchCtx->currResource = fetchCtx->list;
        DEBUG_WEBDAV("opendir returning handle %p (count=%d)", (void*) fetchCtx, fetchCtx->result_count );
        return fetchCtx;
    }
    /* no freeing of curi because its part of the fetchCtx and gets freed later */
}

static int owncloud_closedir(csync_vio_method_handle_t *dhandle) {

    struct listdir_context *fetchCtx = dhandle;

    DEBUG_WEBDAV("closedir method called %p!", dhandle);

    free_fetchCtx(fetchCtx);

    return 0;
}

static csync_vio_file_stat_t *owncloud_readdir(csync_vio_method_handle_t *dhandle) {

    struct listdir_context *fetchCtx = dhandle;

    if( fetchCtx == NULL) {
        /* DEBUG_WEBDAV("An empty dir or at end"); */
        return NULL;
    }

    while( fetchCtx->currResource ) {
        resource* currResource = fetchCtx->currResource;
        char *escaped_path = NULL;

        /* set pointer to next element */
        fetchCtx->currResource = fetchCtx->currResource->next;

        /* It seems strange: first uri->path is unescaped to escape it in the next step again.
         * The reason is that uri->path is not completely escaped (ie. it seems only to have
         * spaces escaped), while the fetchCtx->target is fully escaped.
         * See http://bugs.owncloud.org/thebuggenie/owncloud/issues/oc-613
         */
        escaped_path = ne_path_escape( currResource->uri );
        if (ne_path_compare(fetchCtx->target, escaped_path) != 0) {
            csync_vio_file_stat_t* lfs = resourceToFileStat(currResource);
            fill_stat_cache(lfs);
            SAFE_FREE( escaped_path );
            return lfs;
        }

        /* This is the target URI */
        SAFE_FREE( escaped_path );
    }

    return NULL;
}

static char *owncloud_error_string()
{
    return dav_session.error_string;
}

static int owncloud_commit() {

  clean_caches();

  if( dav_session.ctx )
    ne_session_destroy( dav_session.ctx );
  /* DEBUG_WEBDAV( "********** vio_module_shutdown" ); */

  dav_session.ctx = 0;

  // ne_sock_exit();
  _connected = 0;  /* triggers dav_connect to go through the whole neon setup */

  SAFE_FREE( dav_session.user );
  SAFE_FREE( dav_session.pwd );
  SAFE_FREE( dav_session.session_key);
  SAFE_FREE( dav_session.error_string );

  SAFE_FREE( dav_session.proxy_type );
  SAFE_FREE( dav_session.proxy_host );
  SAFE_FREE( dav_session.proxy_user );
  SAFE_FREE( dav_session.proxy_pwd  );

  return 0;
}

static int owncloud_set_property(const char *key, void *data) {
#define READ_STRING_PROPERTY(P) \
    if (c_streq(key, #P)) { \
        SAFE_FREE(dav_session.P); \
        dav_session.P = c_strdup((const char*)data); \
        return 0; \
    }
    READ_STRING_PROPERTY(session_key)
    READ_STRING_PROPERTY(proxy_type)
    READ_STRING_PROPERTY(proxy_host)
    READ_STRING_PROPERTY(proxy_user)
    READ_STRING_PROPERTY(proxy_pwd)
#undef READ_STRING_PROPERTY

    if (c_streq(key, "proxy_port")) {
        dav_session.proxy_port = *(int*)(data);
        return 0;
    }
    if (c_streq(key, "read_timeout") || c_streq(key, "timeout")) {
        dav_session.read_timeout = *(int*)(data);
        return 0;
    }
    if( c_streq(key, "csync_context")) {
        dav_session.csync_ctx = data;
        return 0;
    }
    if( c_streq(key, "hbf_info")) {
        dav_session.chunk_info = (csync_hbf_info_t *)(data);
        return 0;
    }
    if( c_streq(key, "get_dav_session")) {
        /* Give the ne_session to the caller */
        *(ne_session**)data = dav_session.ctx;
        return 0;
    }
    if( c_streq(key, "no_recursive_propfind")) {
        dav_session.no_recursive_propfind = *(bool*)(data);
        return 0;
    }
    if( c_streq(key, "hbf_block_size")) {
        dav_session.hbf_block_size = *(int64_t*)(data);
        return 0;
    }
    if( c_streq(key, "hbf_threshold")) {
        dav_session.hbf_threshold = *(int64_t*)(data);
        return 0;
    }
    if( c_streq(key, "bandwidth_limit_upload")) {
        dav_session.bandwidth_limit_upload = *(int*)(data);
        return 0;
    }
    if( c_streq(key, "bandwidth_limit_download")) {
        dav_session.bandwidth_limit_download = *(int*)(data);
        return 0;
    }
    if( c_streq(key, "overall_progress_data")) {
      dav_session.overall_progress_data = (csync_overall_progress_t*)(data);
    }
    if( c_streq(key, "redirect_callback")) {
        if (data) {
            csync_owncloud_redirect_callback_t* cb_wrapper = data;

            dav_session.redir_callback = *cb_wrapper;
        } else {
            dav_session.redir_callback = NULL;
        }
    }

    return -1;
}

csync_vio_method_t _method = {
    .method_table_size = sizeof(csync_vio_method_t),
    .get_capabilities = owncloud_capabilities,
    .get_etag = owncloud_get_etag,
    .open = 0,
    .creat = 0,
    .close = 0,
    .read = 0,
    .write = 0,
    .sendfile = 0,
    .lseek = 0,
    .opendir = owncloud_opendir,
    .closedir = owncloud_closedir,
    .readdir = owncloud_readdir,
    .mkdir = 0,
    .rmdir = 0,
    .stat = owncloud_stat,
    .rename = 0,
    .unlink = 0,
    .chmod = 0,
    .chown = 0,
    .utimes = 0,
    .set_property = owncloud_set_property,
    .get_error_string = owncloud_error_string,
    .commit = owncloud_commit

};

csync_vio_method_t *vio_module_init(const char *method_name, const char *args,
                                    csync_auth_callback cb, void *userdata) {
    (void) method_name;
    (void) args;
    (void) userdata;

    _authcb = cb;
    _connected = 0;  /* triggers dav_connect to go through the whole neon setup */

    memset(&dav_session, 0, sizeof(dav_session));

    /* Disable it, Mirall can enable it for the first sync (= no DB)*/
    dav_session.no_recursive_propfind = true;

    return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
    (void) method;

    /* DEBUG_WEBDAV( "********** vio_module_shutdown" ); */
}

/* vim: set ts=4 sw=4 et cindent: */
