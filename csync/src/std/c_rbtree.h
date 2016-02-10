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

/**
 * @file c_rbtree.h
 *
 * @brief Interface of the cynapses libc red-black tree implementation
 *
 * A red-black tree is a type of self-balancing binary search tree. It is
 * complex, but has good worst-case running time for its operations and is
 * efficient in practice: it can search, insert, and delete in O(log n)
 * time, where n is the number of elements in the tree.
 *
 * In red-black trees, the leaf nodes are not relevant and do not contain
 * data. Therefore we use a sentinal node to save memory. All references
 * from internal nodes to leaf nodes instead point to the sentinel node.
 *
 * In a red-black tree each node has a color attribute, the value of which
 * is either red or black. In addition to the ordinary requirements imposed
 * on binary search trees, the following additional requirements of any
 * valid red-black tree apply:
 *
 *    1. A node is either red or black.
 *    2. The root is black.
 *    3. All leaves are black, even when the parent is black
 *       (The leaves are the null children.)
 *    4. Both children of every red node are black.
 *    5. Every simple path from a node to a descendant leaf contains the same
 *       number of black nodes, either counting or not counting the null black
 *       nodes. (Counting or not counting the null black nodes does not affect
 *       the structure as long as the choice is used consistently.).
 *
 * These constraints enforce a critical property of red-black trees: that the
 * longest path from the root to a leaf is no more than twice as long as the
 * shortest path from the root to a leaf in that tree. The result is that the
 * tree is roughly balanced. Since operations such as inserting, deleting, and
 * finding values requires worst-case time proportional to the height of the
 * tree, this theoretical upper bound on the height allows red-black trees to
 * be efficient in the worst-case, unlike ordinary binary search trees.
 *
 * http://en.wikipedia.org/wiki/Red-black_tree
 *
 * @defgroup cynRBTreeInternals cynapses libc red-black tree functions
 * @ingroup cynLibraryAPI
 *
 * @{
 */
#ifndef _C_RBTREE_H
#define _C_RBTREE_H

/* Forward declarations */
struct c_rbtree_s; typedef struct c_rbtree_s c_rbtree_t;
struct c_rbnode_s; typedef struct c_rbnode_s c_rbnode_t;

/**
 * Define the two colors for the red-black tree
 */
enum xrbcolor_e { BLACK = 0, RED }; typedef enum xrbcolor_e xrbcolor_t;

/**
 * @brief Callback function to compare a key with the data from a
 *        red-black tree node.
 *
 * @param key   key as a generic pointer
 * @param data  data as a generic pointer
 *
 * @return   It returns an integer less than, equal to, or greater than zero
 *           depending on the key or data you use. The function is similar
 *           to strcmp().
 */
typedef int c_rbtree_compare_func(const void *key, const void *data);

/**
 * @brief Visit function for the c_rbtree_walk() function.
 *
 * This function will be called by c_rbtree_walk() for every node. It is up to
 * the developer what the function does. The fist parameter is a node object
 * the second can be of any kind.
 *
 * @param obj    The node data that will be passed by c_rbtree_walk().
 * @param data   Generic data pointer.
 *
 * @return 0 on success, < 0 on error. You should set errno.
 *
 */
typedef int c_rbtree_visit_func(void *, void *);

/**
 * Structure that represents a red-black tree
 */
struct c_rbtree_s {
  c_rbnode_t *root;
  c_rbtree_compare_func *key_compare;
  c_rbtree_compare_func *data_compare;
  size_t size;
};

/**
 * Structure that represents a node of a red-black tree
 */
struct c_rbnode_s {
  c_rbtree_t *tree;
  c_rbnode_t *left;
  c_rbnode_t *right;
  c_rbnode_t *parent;
  void *data;
  xrbcolor_t color;
};

/**
 * @brief Create the red-black tree
 *
 * @param rbtree        The pointer to assign the allocated memory.
 *
 * @param key_compare   Callback function to compare a key with the data
 *                      inside a reb-black tree node.
 *
 * @param data_compare  Callback function to compare a key as data with thee
 *                      data inside a red-black tree node.
 */
void c_rbtree_create(c_rbtree_t **rbtree, c_rbtree_compare_func *key_compare, c_rbtree_compare_func *data_compare);

/**
 * @brief Duplicate a red-black tree.
 *
 * @param tree Tree to duplicate.
 *
 * @return   Pointer to a new allocated duplicated rbtree. NULL if an error
 *           occurred.
 */
c_rbtree_t *c_rbtree_dup(const c_rbtree_t *tree);

/**
 * @brief Free the structure of a red-black tree.
 *
 * You should call c_rbtree_destroy() before you call this function.
 *
 * @param tree  The tree to free.
 *
 * @return   0 on success, less than 0 if an error occurred.
 */
int c_rbtree_free(c_rbtree_t *tree);

/**
 * @brief Destroy the content and the nodes of an red-black tree.
 *
 * This is far from the most efficient way to walk a tree, but it is
 * the *safest* way to destroy a tree - the destructor can do almost
 * anything (as long as it does not create an infinite loop) to the
 * tree structure without risk.
 *
 * If for some strange reason you need a faster destructor (think
 * twice - speed and memory deallocation don't mix well) then consider
 * stashing an llist of dataects and destroying that instead, and just
 * using c_rbtree_free() on your tree.
 *
 * @param T            The tree to destroy.
 * @param DESTRUCTOR   The destructor to call on a node to destroy.
 */
#define c_rbtree_destroy(T, DESTRUCTOR)                 \
  do {                                                  \
    if (T) {                                            \
      c_rbnode_t *_c_rbtree_temp;                       \
      while ((_c_rbtree_temp = c_rbtree_head(T))) {     \
        (DESTRUCTOR)(_c_rbtree_temp->data);             \
        if (_c_rbtree_temp == c_rbtree_head(T)) {       \
          c_rbtree_node_delete(_c_rbtree_temp);         \
        }                                               \
      }                                                 \
    }                                                   \
    SAFE_FREE(T);                                       \
  } while (0);

/**
 * @brief Inserts a node into a red black tree.
 *
 * @param tree  The red black tree to insert the node.
 * @param data  The data to insert into the tree.
 *
 * @return  0 on success, 1 if a duplicate has been found and < 0 if an error
 *          occurred with errno set.
 *          EINVAL if a null pointer has been passed as the tree.
 *          ENOMEM if there is no memory left.
 */
int c_rbtree_insert(c_rbtree_t *tree, void *data);

/**
 * @brief Find a node in a red-black tree.
 *
 * c_rbtree_find() is searching for the given  key  in a red-black tree and
 * returns the node if the key has been found.
 *
 * @param tree   The tree to search.
 * @param key    The key to search for.
 *
 * @return   If the key was found the node will be returned. On error NULL
 *           will be returned.
 */
c_rbnode_t *c_rbtree_find(c_rbtree_t *tree, const void *key);

/**
 * @brief Get the head of the red-black tree.
 *
 * @param tree   The tree to get the head for.
 *
 * @return   The head node. NULL if an error occurred.
 */
c_rbnode_t *c_rbtree_head(c_rbtree_t *tree);

/**
 * @brief Get the tail of the red-black tree.
 *
 * @param tree   The tree to get the tail for.
 *
 * @return   The tail node. NULL if an error occurred.
 */
c_rbnode_t *c_rbtree_tail(c_rbtree_t *tree);

/**
 * @brief Get the size of the red-black tree.
 *
 * @param T  The tree to get the size from.
 *
 * @return  The size of the red-black tree.
 */
#define c_rbtree_size(T) (T) == NULL ? 0 : ((T)->size)

/**
 * @brief Walk over a red-black tree.
 * 
 * Walk over a red-black tree calling a visitor function for each node found.
 *
 * @param tree     Tree to walk.
 * @param data     Data which should be passed to the visitor function.
 * @param visitor  Visitor function. This will be called for each node passed.
 *
 * @return   0 on sucess, less than 0 if an error occurred.
 */
int c_rbtree_walk(c_rbtree_t *tree, void *data, c_rbtree_visit_func *visitor);

/**
 * @brief Delete a node in a red-black tree.
 *
 * @param node  Node which should be deleted.
 *
 * @return  0 on success, -1 if an error occurred.
 */
int c_rbtree_node_delete(c_rbnode_t *node);

/**
 * @brief Get the next node.
 *
 * @param node  The node of which you want the next node.
 *
 * @return  The next node, NULL if an error occurred.
 */
c_rbnode_t *c_rbtree_node_next(c_rbnode_t *node);

/**
 * @brief Get the previous node.
 *
 * @param node  The node of which you want the previous node.
 *
 * @return  The previous node, NULL if an error occurred.
 */
c_rbnode_t *c_rbtree_node_prev(c_rbnode_t *node);

/**
 * @brief Get the data of a node.
 *
 * @param N  The node to get the data from.
 *
 * @return  The data, NULL if an error occurred.
 */
#define c_rbtree_node_data(N) ((N) ? ((N)->data) : NULL)

/**
 * @brief Perform a sanity check for a red-black tree.
 *
 * This is mostly for testing purposes.
 *
 * @param tree  The tree to check.
 *
 * @return 0 on success, less than 0 if an error occurred.
 */
int c_rbtree_check_sanity(c_rbtree_t *tree);

/**
 * }@
 */
#endif /* _C_RBTREE_H */
