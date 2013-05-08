static time_t oc_httpdate_parse( const char *date );

// Our cache, key is a char*
c_rbtree_t *propfind_recursive_cache = NULL;
// Values are propfind_recursive_element:
struct propfind_recursive_element {
    struct resource *self;
    struct resource *children;
};
typedef struct propfind_recursive_element propfind_recursive_element_t;

static struct resource* resource_dup(struct resource* o) {
    struct resource *r = c_malloc (sizeof( struct resource ));
    r->uri = c_strdup(o->uri);
    r->name = c_strdup(o->name);
    r->type = o->type;
    r->size = o->size;
    r->modtime = o->modtime;
    r->md5 = c_strdup(o->md5);
    r->next = o->next;
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
static void clear_propfind_recursive_cache()
{
    c_rbtree_destroy(propfind_recursive_cache, _tree_destructor);
    propfind_recursive_cache = NULL;
}

static struct listdir_context *get_listdir_context_from_cache(const char *curi)
{
    if (!propfind_recursive_cache) {
        DEBUG_WEBDAV("get_listdir_context_from_cache No cache");
        return NULL;
    }

    propfind_recursive_element_t *element = c_rbtree_node_data(c_rbtree_find(propfind_recursive_cache, curi));
    if (!element) {
        DEBUG_WEBDAV("get_listdir_context_from_cache No element %s in cache found", curi);
        return NULL;
    }
    if (!element->children) {
        DEBUG_WEBDAV("get_listdir_context_from_cache Element %s in cache found, but no children, assuming that recursive propfind didn't work", curi);
        return NULL;
    }

    // Out of the element, create a listdir_context.. if we could be sure that it is immutable, we could ref instead.. need to investigate
    struct listdir_context *fetchCtx = c_malloc( sizeof( struct listdir_context ));
    fetchCtx->list = NULL;
    fetchCtx->target = c_strdup(curi);
    fetchCtx->currResource = NULL;
    fetchCtx->ref = 1;

    struct resource *iterator = element->children;
    struct resource *r = NULL;
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
static void results_recursive(void *userdata,
                    const ne_uri *uri,
                    const ne_prop_result_set *set)
{
    struct resource *newres = 0;
    const char *clength, *modtime = NULL;
    const char *resourcetype = NULL;
    const char *md5sum = NULL;
    const ne_status *status = NULL;
    char *path = ne_path_unescape( uri->path );

    (void) status;

    if (!propfind_recursive_cache) {
        c_rbtree_create(&propfind_recursive_cache, _key_cmp, _data_cmp);
    }

    /* Fill the resource structure with the data about the file */
    newres = c_malloc(sizeof(struct resource));
    newres->uri =  path; /* no need to strdup because ne_path_unescape already allocates */
    newres->name = c_basename( path );

    modtime      = ne_propset_value( set, &ls_props[0] );
    clength      = ne_propset_value( set, &ls_props[1] );
    resourcetype = ne_propset_value( set, &ls_props[2] );
    md5sum       = ne_propset_value( set, &ls_props[3] );

    newres->type = resr_normal;
    if( resourcetype && strncmp( resourcetype, "<DAV:collection>", 16 ) == 0) {
        newres->type = resr_collection;
    } else {
        DEBUG_WEBDAV("results_recursive %s [%d]", newres->uri, newres->type);
    }

    if (modtime) {
        newres->modtime = oc_httpdate_parse(modtime);
    }

    /* DEBUG_WEBDAV("Parsing Modtime: %s -> %llu", modtime, (unsigned long long) newres->modtime ); */

    if (clength) {
        char *p;

        newres->size = DAV_STRTOL(clength, &p, 10);
        if (*p) {
            newres->size = 0;
        }
        /* DEBUG_WEBDAV("Parsed File size for %s from %s: %lld", newres->name, clength, (long long)newres->size ); */
    }

    if( md5sum ) {
        int len = strlen(md5sum)-2;
        if( len > 0 ) {
            /* Skip the " around the string coming back from the ne_propset_value call */
            newres->md5 = c_malloc(len+1);
            strncpy( newres->md5, md5sum+1, len );
            newres->md5[len] = '\0';
        }
    }

    DEBUG_WEBDAV("results_recursive %s [%s] >%s<", newres->uri, newres->type == resr_collection ? "collection" : "file", resourcetype);

    /* Create new item in rb tree */
    if (newres->type == resr_collection) {
        DEBUG_WEBDAV("results_recursiveIt is a collection %s", newres->uri);
        // Check if in rb tree
        propfind_recursive_element_t *element = c_rbtree_node_data(c_rbtree_find(propfind_recursive_cache,uri->path));
        // If not, create a new item and insert it
        if (!element) {
            element = c_malloc(sizeof(propfind_recursive_element_t));
            element->self = resource_dup(newres);
            element->children = NULL;
            c_rbtree_insert(propfind_recursive_cache, element);
            /* DEBUG_WEBDAV("results_recursive Added collection %s", newres->uri); */
        }
    }

    /* Check for parent in tree. If exists: Insert it into the children elements there */
    char *parentPath = ne_path_parent(uri->path);
    if (parentPath) {
        free(parentPath);
        propfind_recursive_element_t *element = c_rbtree_node_data(c_rbtree_find(propfind_recursive_cache,parentPath));
        if (element) {
            newres->next = element->children;
            element->children = newres;
            /* DEBUG_WEBDAV("results_recursive Added child %s to collection %s", newres->uri, element->self->uri); */
        } else {
            /* DEBUG_WEBDAV("results_recursive No parent %s found for child %s", parentPath, newres->uri); */
            resource_free(newres);
            newres = NULL;
        }
    }

}


/*
 * fetches a resource list from the WebDAV server. This is equivalent to list dir.
 */
static struct listdir_context *fetch_resource_list_recursive(const char *uri, const char *curi)
{
    int ret = 0;
    ne_propfind_handler *hdl = NULL;
    ne_request *request = NULL;
    const char *content_type = NULL;
    const ne_status *req_status = NULL;
    int depth = NE_DEPTH_INFINITE;

    DEBUG_WEBDAV("Starting recursive propfind %s %s", uri, curi);

    /* do a propfind request and parse the results in the results function, set as callback */
    hdl = ne_propfind_create(dav_session.ctx, curi, depth);

    if(hdl) {
        ret = ne_propfind_named(hdl, ls_props, results_recursive, NULL);
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
            if (_progresscb) {
                _progresscb(uri, CSYNC_NOTIFY_ERROR,  req_status->code, (long long)(req_status->reason_phrase) ,dav_session.userdata);
            }
        }
        DEBUG_WEBDAV("Recursive propfind result code %d.", req_status->code);
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
        return NULL;
    }

    return get_listdir_context_from_cache(curi);
}
