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

c_rbtree_t *propfind_recursive_cache = NULL;
int propfind_recursive_cache_depth = 0;
int propfind_recursive_cache_file_count = 0;
int propfind_recursive_cache_folder_count = 0;


static struct resource* resource_dup(struct resource* o) {
    struct resource *r = c_malloc (sizeof( struct resource ));
    ZERO_STRUCTP(r);

    r->uri = c_strdup(o->uri);
    r->name = c_strdup(o->name);
    r->type = o->type;
    r->size = o->size;
    r->modtime = o->modtime;
    if( o->md5 ) {
        r->md5 = c_strdup(o->md5);
    }
    r->next = o->next;
    csync_vio_set_file_id(r->file_id, o->file_id);

    return r;
}
static void resource_free(struct resource* o) {
    struct resource* old = NULL;
    while (o)
    {
        old = o;
        o = o->next;
        SAFE_FREE(old->uri);
        SAFE_FREE(old->name);
        SAFE_FREE(old->md5);
        SAFE_FREE(old);
    }
}

static void _tree_destructor(void *data) {
    propfind_recursive_element_t *element = data;
    resource_free(element->self);
    resource_free(element->children);
    SAFE_FREE(element);
}

void clear_propfind_recursive_cache(void)
{
    if (propfind_recursive_cache) {
        DEBUG_WEBDAV("clear_propfind_recursive_cache Invalidating..");
        c_rbtree_destroy(propfind_recursive_cache, _tree_destructor);
        propfind_recursive_cache = NULL;
    }
}

struct listdir_context *get_listdir_context_from_recursive_cache(const char *curi)
{
    propfind_recursive_element_t *element = NULL;
    struct listdir_context *fetchCtx = NULL;
    struct resource *iterator, *r;

    if (!propfind_recursive_cache) {
        DEBUG_WEBDAV("get_listdir_context_from_recursive_cache No cache");
        return NULL;
    }

    element = c_rbtree_node_data(c_rbtree_find(propfind_recursive_cache, curi));
    if (!element) {
        DEBUG_WEBDAV("get_listdir_context_from_recursive_cache No element %s in cache found", curi);
        return NULL;
    }

    /* Out of the element, create a listdir_context.. if we could be sure that it is immutable, we could ref instead.. need to investigate */
    fetchCtx = c_malloc( sizeof( struct listdir_context ));
    ZERO_STRUCTP(fetchCtx);
    fetchCtx->list = NULL;
    fetchCtx->target = c_strdup(curi);
    fetchCtx->currResource = NULL;
    fetchCtx->ref = 1;

    iterator = element->children;
    r = NULL;
    while (iterator) {
        r = resource_dup(iterator);
        r->next = fetchCtx->list;
        fetchCtx->list = r;
        iterator = iterator->next;
        fetchCtx->result_count++;
        /* DEBUG_WEBDAV("get_listdir_context_from_cache Returning cache for %s element %s", fetchCtx->target, fetchCtx->list->uri); */
    }

    r = resource_dup(element->self);
    r->next = fetchCtx->list;
    fetchCtx->result_count++;
    fetchCtx->list = r;
    fetchCtx->currResource = fetchCtx->list;
    DEBUG_WEBDAV("get_listdir_context_from_cache Returning cache for %s (%d elements)", fetchCtx->target, fetchCtx->result_count);
    return fetchCtx;
}

static int _key_cmp(const void *key, const void *b) {
    const char *elementAUri = (char*)key;
    const propfind_recursive_element_t *elementB = b;
    return ne_path_compare(elementAUri, elementB->self->uri);
}
static int _data_cmp(const void *a, const void *b) {
    const propfind_recursive_element_t *elementA = a;
    const propfind_recursive_element_t *elementB = b;
    return ne_path_compare(elementA->self->uri, elementB->self->uri);
}
static void propfind_results_recursive(void *userdata,
                    const ne_uri *uri,
                    const ne_prop_result_set *set)
{
    struct resource *newres = 0;
    const char *clength, *modtime, *file_id = NULL;
    const char *resourcetype = NULL;
    const char *md5sum = NULL;
    const ne_status *status = NULL;
    char *path = ne_path_unescape( uri->path );
    char *parentPath;
    char *propfindRootUri = (char*) userdata;
    propfind_recursive_element_t *element = NULL;
    propfind_recursive_element_t *pElement = NULL;
    int depth = 0;

    (void) status;
    (void) propfindRootUri;

    if (!propfind_recursive_cache) {
        c_rbtree_create(&propfind_recursive_cache, _key_cmp, _data_cmp);
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
    if( resourcetype && strncmp( resourcetype, "<DAV:collection>", 16 ) == 0) {
        newres->type = resr_collection;
        propfind_recursive_cache_folder_count++;
    } else {
        /* DEBUG_WEBDAV("propfind_results_recursive %s [%d]", newres->uri, newres->type); */
        propfind_recursive_cache_file_count++;
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
    /*
    DEBUG_WEBDAV("propfind_results_recursive %s [%s] %s", newres->uri, newres->type == resr_collection ? "collection" : "file", newres->md5);
    */

    /* Create new item in rb tree */
    if (newres->type == resr_collection) {
        DEBUG_WEBDAV("propfind_results_recursive %s is a folder", newres->uri);
        /* Check if in rb tree */
        element = c_rbtree_node_data(c_rbtree_find(propfind_recursive_cache,uri->path));
        /* If not, create a new item and insert it */
        if (!element) {
            element = c_malloc(sizeof(propfind_recursive_element_t));
            element->self = resource_dup(newres);
            element->children = NULL;
            element->parent = NULL;
            c_rbtree_insert(propfind_recursive_cache, element);
            /* DEBUG_WEBDAV("results_recursive Added collection %s", newres->uri); */
        }
    }

    /* Check for parent in tree. If exists: Insert it into the children elements there */
    parentPath = ne_path_parent(uri->path);
    if (parentPath) {
        propfind_recursive_element_t *parentElement = NULL;

        parentElement = c_rbtree_node_data(c_rbtree_find(propfind_recursive_cache,parentPath));
        free(parentPath);

        if (parentElement) {
            newres->next = parentElement->children;
            parentElement->children = newres;

            /* If the current result is a collection we also need to set its parent */
            if (element)
                element->parent = parentElement;

            pElement = element;
            while (pElement) {
                depth++;
                pElement = pElement->parent;
            }
            if (depth > propfind_recursive_cache_depth) {
                DEBUG_WEBDAV("propfind_results_recursive %s new maximum tree depth %d", newres->uri, depth);
                propfind_recursive_cache_depth = depth;
            }

            /* DEBUG_WEBDAV("results_recursive Added child %s to collection %s", newres->uri, element->self->uri); */
        } else {
            /* DEBUG_WEBDAV("results_recursive No parent %s found for child %s", parentPath, newres->uri); */
            resource_free(newres);
            newres = NULL;
        }
    }

}

void fetch_resource_list_recursive(const char *uri, const char *curi)
{
    int ret = 0;
    ne_propfind_handler *hdl = NULL;
    ne_request *request = NULL;
    const char *content_type = NULL;
    const ne_status *req_status = NULL;
    int depth = NE_DEPTH_INFINITE;

    DEBUG_WEBDAV("fetch_resource_list_recursive Starting recursive propfind %s %s", uri, curi);

    /* do a propfind request and parse the results in the results function, set as callback */
    hdl = ne_propfind_create(dav_session.ctx, curi, depth);

    if(hdl) {
        ret = ne_propfind_named(hdl, ls_props, propfind_results_recursive, (void*)curi);
        request = ne_propfind_get_request( hdl );
        req_status = ne_get_status( request );
    }

    if( ret == NE_OK ) {
        /* Check the request status. */
        if( req_status && req_status->klass != 2 ) {
            set_errno_from_http_errcode(req_status->code);
            DEBUG_WEBDAV("ERROR: Request failed: status %d (%s)", req_status->code,
                         req_status->reason_phrase);
            ret = NE_CONNECT;
            set_error_message(req_status->reason_phrase);
            oc_notify_progress(uri, CSYNC_NOTIFY_ERROR,  req_status->code, (intptr_t)(req_status->reason_phrase));
        }
        DEBUG_WEBDAV("Recursive propfind result code %d.", req_status ? req_status->code : 0);
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

        err = ne_get_error( dav_session.ctx );
        DEBUG_WEBDAV("WRN: propfind named failed with %d, request error: %s", ret, err ? err : "<nil>");
    }

    if( hdl )
        ne_propfind_destroy(hdl);

    if( ret != NE_OK ) {
        return;
    }

    return;
}

/* Called by owncloud_opendir()->fetch_resource_list() to fill the cache */
extern struct listdir_context *propfind_cache;
void fill_recursive_propfind_cache(const char *uri, const char *curi) {
    fetch_resource_list_recursive(uri, curi);

    if (propfind_recursive_cache_depth <= 2) {
        DEBUG_WEBDAV("fill_recursive_propfind_cache %s Server maybe did not give us an 'infinity' depth result", curi);
        /* transform the cache to the normal cache in propfind_cache */
        propfind_cache = get_listdir_context_from_recursive_cache(curi);
        /* clear the cache, it is bogus since the server returned only results for Depth 1 */
        clear_propfind_recursive_cache();
    } else {
        DEBUG_WEBDAV("fill_recursive_propfind_cache %s We received %d elements deep for 'infinity' depth (%d folders, %d files)",
                     curi,
                     propfind_recursive_cache_depth,
                     propfind_recursive_cache_folder_count,
                     propfind_recursive_cache_file_count);

    }
}
