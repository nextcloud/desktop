/*
 * cynapses libc functions
 *
 * Copyright (c) 2003-2004 by Andrew Suffield <asuffield@debian.org>
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

/*
 * This code was originally released under GPL but Andrew Suffield agreed to
 * change it to LGPL.
 */

/*
 * static function don't have NULL pointer checks, segfaults are intended.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "c_alloc.h"
#include "c_rbtree.h"

#define NIL &_sentinel /* all leafs are sentinels */
static c_rbnode_t _sentinel = {NULL, NIL, NIL, NULL, NULL, BLACK};

void c_rbtree_create(c_rbtree_t **rbtree, c_rbtree_compare_func *key_compare, c_rbtree_compare_func *data_compare) {
  assert(rbtree);
  assert(key_compare);
  assert(data_compare);

  c_rbtree_t *tree = NULL;

  tree = c_malloc(sizeof(*tree));
  tree->root = NIL;
  tree->key_compare = key_compare;
  tree->data_compare = data_compare;
  tree->size = 0;

  *rbtree = tree;
}

static c_rbnode_t *_rbtree_subtree_dup(const c_rbnode_t *node, c_rbtree_t *new_tree, c_rbnode_t *new_parent) {
  c_rbnode_t *new_node = NULL;

  new_node = (c_rbnode_t*) c_malloc(sizeof(c_rbnode_t));

  new_node->tree = new_tree;
  new_node->data = node->data;
  new_node->color = node->color;
  new_node->parent = new_parent;

  if (node->left == NIL) {
    new_node->left = NIL;
  } else {
    new_node->left = _rbtree_subtree_dup(node->left, new_tree, new_node);
  }

  if (node->right == NIL) {
    new_node->right = NIL;
  } else {
    new_node->right = _rbtree_subtree_dup(node->right, new_tree, new_node);
  }

  return new_node;
}

c_rbtree_t *c_rbtree_dup(const c_rbtree_t *tree) {
  c_rbtree_t *new_tree = NULL;

  new_tree = (c_rbtree_t*) c_malloc(sizeof(c_rbtree_t));

  new_tree->key_compare = tree->key_compare;
  new_tree->data_compare = tree->data_compare;
  new_tree->size = tree->size;
  new_tree->root = _rbtree_subtree_dup(tree->root, new_tree, NULL);

  return new_tree;
}

static int _rbtree_subtree_free(c_rbnode_t *node) {
  assert(node);

  if (node->left != NIL) {
    if (_rbtree_subtree_free(node->left) < 0) {
      /* TODO: set errno? ECANCELED? */
      return -1;
    }
  }

  if (node->right != NIL) {
    if (_rbtree_subtree_free(node->right) < 0) {
      /* TODO: set errno? ECANCELED? */
      return -1;
    }
  }

  SAFE_FREE(node);

  return 0;
}

int c_rbtree_free(c_rbtree_t *tree) {
  if (tree == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (tree->root != NIL) {
    _rbtree_subtree_free(tree->root);
  }

  SAFE_FREE(tree);

  return 0;
}

static int _rbtree_subtree_walk(c_rbnode_t *node, void *data, c_rbtree_visit_func *visitor) {
  assert(node);
  assert(data);
  assert(visitor);

  if (node == NIL) {
    return 0;
  }

  if (_rbtree_subtree_walk(node->left, data, visitor) < 0) {
    return -1;
  }

  if ((*visitor)(node->data, data) < 0) {
    return -1;
  }

  if (_rbtree_subtree_walk(node->right, data, visitor) < 0) {
    return -1;
  }

  return 0;
}

int c_rbtree_walk(c_rbtree_t *tree, void *data, c_rbtree_visit_func *visitor) {
  if (tree == NULL || data == NULL || visitor == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (_rbtree_subtree_walk(tree->root, data, visitor) < 0) {
    return -1;
  }

  return 0;
}

static c_rbnode_t *_rbtree_subtree_head(c_rbnode_t *node) {
  assert(node);

  if (node == NIL) {
    return node;
  }

  while (node->left != NIL) {
    node = node->left;
  }

  return node;
}

static c_rbnode_t *_rbtree_subtree_tail(c_rbnode_t *node) {
  assert(node);

  if (node == NIL) {
    return node;
  }

  while (node->right != NIL) {
    node = node->right;
  }

  return node;
}

c_rbnode_t *c_rbtree_head(c_rbtree_t *tree) {
  c_rbnode_t *node = NULL;

  if (tree == NULL) {
    errno = EINVAL;
    return NULL;
  }

  node = _rbtree_subtree_head(tree->root);

  return node != NIL ? node : NULL;
}

c_rbnode_t *c_rbtree_tail(c_rbtree_t *tree) {
  c_rbnode_t *node = NULL;

  if (tree == NULL) {
    errno = EINVAL;
    return NULL;
  }

  node = _rbtree_subtree_tail(tree->root);

  return node != NIL ? node : NULL;
}

c_rbnode_t *c_rbtree_node_next(c_rbnode_t *node) {
  c_rbnode_t *parent = NULL;

  if (node == NULL) {
    errno = EINVAL;
    return NULL;
  }

  if (node->right != NIL) {
    c_rbnode_t *next = NULL;
    next = _rbtree_subtree_head(node->right);

    return next != NIL ? next : NULL;
  }

  parent = node->parent;
  while (parent && node == parent->right) {
    node = parent;
    parent = node->parent;
  }

  return parent != NULL ? parent : NULL;
}

c_rbnode_t *c_rbtree_node_prev(c_rbnode_t *node) {
  c_rbnode_t *parent = NULL;

  if (node == NULL) {
    return NULL;
  }

  if (node->left != NIL) {
    c_rbnode_t *prev = NULL;
    prev = _rbtree_subtree_tail(node->left);
    return prev != NIL ? prev : NULL;
  }

  parent = node->parent;
  while (parent && node == parent->left) {
    node = parent;
    parent = node->parent;
  }

  return parent != NULL ? parent : NULL;
}

c_rbnode_t *c_rbtree_find(c_rbtree_t *tree, const void *key) {
  int cmp = 0;
  c_rbnode_t *node = NULL;

  if (tree == NULL) {
    errno = EINVAL;
    return NULL;
  }
  node = tree->root;

  while (node != NIL) {
    cmp = tree->key_compare(key, node->data);
    if (cmp == 0) {
      return node;
    }

    if (cmp < 0) {
      node = node->left;
    } else {
        node = node->right;
    }
  }

  return NULL;
}

static void _rbtree_subtree_left_rotate(c_rbnode_t *x) {
  c_rbnode_t *y = NULL;

  assert(x);

  y = x->right;

  /* establish x-right link */
  x->right = y->left;

  if (y->left != NIL) {
    y->left->parent = x;
  }

  /* establish y->parent link */
  if (y != NIL) {
    y->parent = x->parent;
  }

  if (x->parent) {
    if (x == x->parent->left) {
      x->parent->left = y;
    } else {
      x->parent->right = y;
    }
  } else {
    x->tree->root = y;
  }

  /* link x and y */
  y->left = x;
  if (x != NIL) {
    x->parent = y;
  }
}

/* rotat node x to the right */
static void _rbtree_subtree_right_rotate(c_rbnode_t *x) {
  c_rbnode_t *y = NULL;

  assert(x);

  y = x->left;

  /* establish x->left link */
  x->left = y->right;

  if (y->right != NIL) {
    y->right->parent = x;
  }

  /* establish y->parent link */
  if (y != NIL) {
    y->parent = x->parent;
  }

  if (x->parent) {
    if (x == x->parent->right) {
        x->parent->right = y;
    } else {
      x->parent->left = y;
    }
  } else {
    x->tree->root = y;
  }

  /* link x and y */
  y->right = x;
  if (x != NIL) {
    x->parent = y;
  }
}

int c_rbtree_insert(c_rbtree_t *tree, void *data) {
  int cmp = 0;
  c_rbnode_t *current = NULL;
  c_rbnode_t *parent = NULL;
  c_rbnode_t *x = NULL;

  if (tree == NULL) {
    errno = EINVAL;
    return -1;
  }

  /* First we do a classic binary tree insert */
  current = tree->root;
  parent = NULL;

  while (current != NIL) {
    cmp = tree->data_compare(data, current->data);
    parent = current;
    if (cmp == 0) {
      return 1;
    } else if (cmp < 0) {
      current = current->left;
    } else {
      current = current->right;
    }
  }

  x = (c_rbnode_t *) c_malloc(sizeof(c_rbnode_t));

  x->tree = tree;
  x->data = data;
  x->parent = parent;
  x->left = NIL;
  x->right = NIL;
  x->color = RED;

  if (parent) {
    /* Note that cmp still contains the comparison of data with
     * parent->data, from the last pass through the loop above
     */
    if (cmp < 0) {
        parent->left = x;
    } else {
        parent->right = x;
    }
  } else {
    tree->root = x;
  }

  /* Insert fixup - check red-black properties */
  while (x != tree->root && x->parent->color == RED) {
    /* we have a violation */
    if (x->parent == x->parent->parent->left) {
      c_rbnode_t *y = NULL;

      y = x->parent->parent->right;
      if (y->color == RED) {
        x->parent->color = BLACK;
        y->color = BLACK;
        x->parent->parent->color = RED;
        x = x->parent->parent;
      } else {
        /* uncle is back */
        if (x == x->parent->right) {
          /* make x a left child */
          x = x->parent;
          _rbtree_subtree_left_rotate(x);
        }
        x->parent->color = BLACK;
        x->parent->parent->color = RED;
        _rbtree_subtree_right_rotate(x->parent->parent);
      }
    } else {
      c_rbnode_t *y = NULL;

      y = x->parent->parent->left;
      if (y->color == RED) {
        x->parent->color = BLACK;
        y->color = BLACK;
        x->parent->parent->color = RED;
        x = x->parent->parent;
      } else {
        /* uncle is back */
        if (x == x->parent->left) {
          x = x->parent;
          _rbtree_subtree_right_rotate(x);
        }
        x->parent->color = BLACK;
        x->parent->parent->color = RED;
        _rbtree_subtree_left_rotate(x->parent->parent);
      }
    }
  } /* end while */
  tree->root->color = BLACK;

  tree->size++;

  return 0;
}

int c_rbtree_node_delete(c_rbnode_t *node) {
  c_rbtree_t *tree;
  c_rbnode_t *y;
  c_rbnode_t *x;

  if (node == NULL || node == NIL) {
    errno = EINVAL;
    return -1;
  }

  tree = node->tree;

  if (node->left == NIL || node->right == NIL) {
    /* y has a NIL node as a child */
    y = node;
  } else {
    /* find tree successor with a NIL node as a child */
    y = node;
    while(y->left != NIL) {
      y = y->left;
    }
  }

  /* x is y's only child */
  if (y->left != NIL) {
    x = y->left;
  } else {
    x = y->right;
  }

  /* remove y from the parent chain */
  x->parent = y->parent;

  if (y->parent) {
    if (y == y->parent->left) {
        y->parent->left = x;
    } else {
      y->parent->right = x;
    }
  } else {
    y->tree->root = x;
  }

  /* If y is not the node we're deleting, splice it in place of that
   * node
   *
   * The traditional code would call for us to simply copy y->data, but
   * that would invalidate the wrong pointer - there might be external
   * references to this node, and we must preserve its address.
   */
  if (y != node) {
    /* Update y */
    y->parent = node->parent;
    y->left = node->left;
    y->right = node->right;

    /* Update the children and the parent */
    if (y->left != NIL) {
        y->left->parent = y;
    }
    if (y->right != NIL) {
      y->right->parent = y;
    }
    if (y->parent != NULL) {
      if (node == y->parent->left) {
        y->parent->left = y;
      } else {
        y->parent->right = y;
      }
    } else {
      y->tree->root = y;
    }
  }

  if (y->color == BLACK) {
    while (x != y->tree->root && x->color == BLACK) {
      if (x == x->parent->left) {
        c_rbnode_t *w = NULL;

        w = x->parent->right;

        if (w->color == RED) {
          w->color = BLACK;
          x->parent->color = RED;
          _rbtree_subtree_left_rotate(x->parent);
          w = x->parent->right;
        }

        if (w->left->color == BLACK && w->right->color == BLACK) {
          w->color = RED;
          x = x->parent;
        } else {
          if (w->right->color == BLACK) {
            w->left->color = BLACK;
            w->color = RED;
            _rbtree_subtree_right_rotate(w);
            w = x->parent->right;
          }
          w->color = x->parent->color;
          x->parent->color = BLACK;
          w->right->color = BLACK;
          _rbtree_subtree_left_rotate(x->parent);
          x = y->tree->root;
        }
      } else {
        c_rbnode_t *w = NULL;

        w = x->parent->left;
        if (w->color == RED) {
          w->color = BLACK;
          x->parent->color = RED;
          _rbtree_subtree_right_rotate(x->parent);
          w = x->parent->left;
        }

        if (w->right->color == BLACK && w->left->color == BLACK) {
          w->color = RED;
          x = x->parent;
        } else {
          if (w->left->color == BLACK) {
            w->right->color = BLACK;
            w->color = RED;
            _rbtree_subtree_left_rotate(w);
            w = x->parent->left;
          }
          w->color = x->parent->color;
          x->parent->color = BLACK;
          w->left->color = BLACK;
          _rbtree_subtree_right_rotate(x->parent);
          x = y->tree->root;
        }
      }
    }
    x->color = BLACK;
  } /* end if: y->color == BLACK */

  /* node has now been spliced out of the tree */
  SAFE_FREE(y);
  tree->size--;

  return 0;
}

static int _rbtree_subtree_check_black_height(c_rbnode_t *node) {
  int left = 0;
  int right = 0;

  assert(node);

  if (node == NIL) {
    return 0;
  }

  left = _rbtree_subtree_check_black_height(node->left);
  right = _rbtree_subtree_check_black_height(node->right);
  if (left != right) {
    return -1;
  }

  return left + (node->color == BLACK);
}

int c_rbtree_check_sanity(c_rbtree_t *tree) {
  c_rbnode_t *node = NULL;
  c_rbnode_t *prev = NULL;
  c_rbnode_t *next = NULL;
  c_rbnode_t *tail = NULL;
  size_t size = 0;

  if (tree == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (! tree->key_compare || ! tree->data_compare) {
    errno = EINVAL;
    return -2;
  }

  /* Iterate the tree */
  tail = c_rbtree_tail(tree);
  for (node = c_rbtree_head(tree); node; node = next) {
    if (node->tree != tree) {
      return -4;
    }

    /* We should never see a nil while iterating */
    if (node == NIL) {
      return -5;
    }

    /* node == tree-root iff node->parent == NIL */
    if (node == tree->root) {
      if (node->parent != NULL) {
        return -6;
      }
    } else {
      if (node->parent == NULL) {
        return -7;
      }
    }

    /* Invertability of the iterate functions */
    if (prev != c_rbtree_node_prev(node)) {
      return -8;
    }

    /* Check the iteration sequence */
    if (prev) {
      if (tree->data_compare(prev->data, node->data) > 0) {
        return -9;
      }

      /* And the other way around, to make sure data_compare is stable */
      if (tree->data_compare(node->data, prev->data) < 0) {
        return -10;
      }
    }

    /* The binary tree property */
    if (node->left != NIL) {
      if (tree->data_compare(node->left->data, node->data) > 0) {
        return -11;
      }

      if (tree->data_compare(node->data, node->left->data) < 0) {
        return -11;
      }
    }

    if (node->right != NIL) {
      if (tree->data_compare(node->data, node->right->data) > 0) {
        return -12;
      }

      if (tree->data_compare(node->right->data, node->data) < 0) {
        return -13;
      }
    }

    /* Red-black tree property 3: red nodes have black children */
    if (node->color == RED) {
      if (node->left->color == RED) {
        return -14;
      }
      if (node->right->color == RED) {
        return -15;
      }
    }

    /* next == NULL if node == tail */
    next = c_rbtree_node_next(node);
    if (next) {
      if (node == tail) {
        return -16;
      }
    } else {
      if (node != tail) {
        return -17;
      }
    }

    prev = node;
    size++;
  } /* end for loop */

  if (size != tree->size) {
    return -18;
  }

  if (_rbtree_subtree_check_black_height(tree->root) < 0) {
    return -19;
  }

  return 0;
}
