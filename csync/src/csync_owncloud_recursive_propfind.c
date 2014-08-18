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
#include "csync_owncloud_private.h"

static void _tree_destructor(void *data) {
    propfind_recursive_element_t *element = data;
    resource_free(element->self);
    resource_free(element->children);
    SAFE_FREE(element);
}

void clear_propfind_recursive_cache(csync_owncloud_ctx_t *ctx)
{
    if (ctx->propfind_recursive_cache) {
        DEBUG_WEBDAV("clear_propfind_recursive_cache Invalidating..");
        c_rbtree_destroy(ctx->propfind_recursive_cache, _tree_destructor);
        ctx->propfind_recursive_cache = NULL;
    }
}

struct listdir_context *get_listdir_context_from_recursive_cache(csync_owncloud_ctx_t *ctx, const char *curi)
{
    propfind_recursive_element_t *element = NULL;
    struct listdir_context *fetchCtx = NULL;
    struct resource *iterator, *r;

    if (!ctx->propfind_recursive_cache) {
        DEBUG_WEBDAV("get_listdir_context_from_recursive_cache No cache");
        return NULL;
    }

    element = c_rbtree_node_data(c_rbtree_find(ctx->propfind_recursive_cache, curi));
    if (!element) {
        DEBUG_WEBDAV("get_listdir_context_from_recursive_cache No element %s in cache found", curi);
        return NULL;
    }
    if( ctx->csync_ctx->callbacks.update_callback ) {
        ctx->csync_ctx->callbacks.update_callback(false, curi, ctx->csync_ctx->callbacks.update_callback_userdata);
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
static void propfind_results_recursive_callback(void *userdata,
                    const ne_uri *uri,
                    const ne_prop_result_set *set)
{
    struct resource *newres = 0;

    const ne_status *status = NULL;
    char *path = ne_path_unescape( uri->path );
    char *parentPath;
    propfind_recursive_element_t *element = NULL;
    propfind_recursive_element_t *pElement = NULL;
    int depth = 0;
    csync_owncloud_ctx_t *ctx = (csync_owncloud_ctx_t*) userdata;


    (void) status;

    if (!ctx->propfind_recursive_cache) {
        c_rbtree_create(&ctx->propfind_recursive_cache, _key_cmp, _data_cmp);
    }

    /* Fill the resource structure with the data about the file */
    newres = c_malloc(sizeof(struct resource));

    newres->uri =  path; /* no need to strdup because ne_path_unescape already allocates */
    newres->name = c_basename( path );
    fill_webdav_properties_into_resource(newres, set);

    if( newres->type == resr_collection) {
        ctx->propfind_recursive_cache_folder_count++;
    } else {
        ctx->propfind_recursive_cache_file_count++;
    }

    /* Create new item in rb tree */
    if (newres->type == resr_collection) {
        DEBUG_WEBDAV("propfind_results_recursive %s is a folder", newres->uri);
        /* Check if in rb tree */
        element = c_rbtree_node_data(c_rbtree_find(ctx->propfind_recursive_cache,uri->path));
        /* If not, create a new item and insert it */
        if (!element) {
            element = c_malloc(sizeof(propfind_recursive_element_t));
            element->self = resource_dup(newres);
            element->self->next = 0;
            element->children = NULL;
            element->parent = NULL;
            c_rbtree_insert(ctx->propfind_recursive_cache, element);
            /* DEBUG_WEBDAV("results_recursive Added collection %s", newres->uri); */

            // We do this here and in get_listdir_context_from_recursive_cache because
            // a recursive PROPFIND might take some time but we still want to
            // be informed. Later when get_listdir_context_from_recursive_cache is
            // called the DB queries might be the problem causing slowness, so do it again there then.
            if( ctx->csync_ctx->callbacks.update_callback ) {
		ctx->csync_ctx->callbacks.update_callback(false, path, ctx->csync_ctx->callbacks.update_callback_userdata);
	    }
        }
    }

    /* Check for parent in tree. If exists: Insert it into the children elements there */
    parentPath = ne_path_parent(uri->path);
    if (parentPath) {
        propfind_recursive_element_t *parentElement = NULL;

        parentElement = c_rbtree_node_data(c_rbtree_find(ctx->propfind_recursive_cache,parentPath));
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
            if (depth > ctx->propfind_recursive_cache_depth) {
                DEBUG_WEBDAV("propfind_results_recursive %s new maximum tree depth %d", newres->uri, depth);
                ctx->propfind_recursive_cache_depth = depth;
            }

            /* DEBUG_WEBDAV("results_recursive Added child %s to collection %s", newres->uri, element->self->uri); */
            return;
        }
    }

    resource_free(newres);
    newres = NULL;
}

void fetch_resource_list_recursive(csync_owncloud_ctx_t *ctx, const char *uri, const char *curi)
{
    int ret = 0;
    ne_propfind_handler *hdl = NULL;
    ne_request *request = NULL;
    const char *content_type = NULL;
    const ne_status *req_status = NULL;
    int depth = NE_DEPTH_INFINITE;

    DEBUG_WEBDAV("fetch_resource_list_recursive Starting recursive propfind %s %s", uri, curi);
    if( ctx->csync_ctx->callbacks.update_callback ) {
	ctx->csync_ctx->callbacks.update_callback(false, curi, ctx->csync_ctx->callbacks.update_callback_userdata);
    }

    /* do a propfind request and parse the results in the results function, set as callback */
    hdl = ne_propfind_create(ctx->dav_session.ctx, curi, depth);

    if(hdl) {
        ret = ne_propfind_named(hdl, ls_props, propfind_results_recursive_callback, ctx);
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
            set_error_message(ctx, req_status->reason_phrase);
        }
        DEBUG_WEBDAV("Recursive propfind result code %d.", req_status ? req_status->code : 0);
    } else {
        if( ret == NE_ERROR && req_status->code == 404) {
            errno = ENOENT;
        } else {
            set_errno_from_neon_errcode(ctx, ret);
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
            set_error_message(ctx, "Server error: PROPFIND reply is not XML formatted!");
            ret = NE_CONNECT;
        }
    }

    if( ret != NE_OK ) {
        const char *err = NULL;

        err = ne_get_error( ctx->dav_session.ctx );
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
void fill_recursive_propfind_cache(csync_owncloud_ctx_t *ctx, const char *uri, const char *curi) {
    fetch_resource_list_recursive(ctx, uri, curi);

    if (ctx->propfind_recursive_cache_depth <= 2) {
        DEBUG_WEBDAV("fill_recursive_propfind_cache %s Server maybe did not give us an 'infinity' depth result", curi);
        /* transform the cache to the normal cache in propfind_cache */
        ctx->propfind_cache = get_listdir_context_from_recursive_cache(ctx, curi);
        /* clear the cache, it is bogus since the server returned only results for Depth 1 */
        clear_propfind_recursive_cache(ctx);
    } else {
        DEBUG_WEBDAV("fill_recursive_propfind_cache %s We received %d elements deep for 'infinity' depth (%d folders, %d files)",
                     curi,
                     ctx->propfind_recursive_cache_depth,
                     ctx->propfind_recursive_cache_folder_count,
                     ctx->propfind_recursive_cache_file_count);

    }
}
