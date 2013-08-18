/*
 * csync list -- a doubly-linked list
 *
 * This code is based on glist.{h,c} from glib
 *   ftp://ftp.gtk.org/pub/gtk/
 * Copyright (c) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (c) 2006-2013  Andreas Schneider <mail@csyncapses.org>
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
#ifndef _C_LIST_H
#define _C_LIST_H

/**
 * c_list -- a doubly-linked list.
 *
 * The c_list_t structure and its associated functions provide a standard
 * doubly-linked list data structure. Each node has two links: one points to
 * the previous node, or points to a null value or empty list if it is the
 * first  node; and one points to the next, or points to a null value or empty
 * list if it is the final node.
 *
 * The data contained in each element can be simply pointers to any type of
 * data. You are the owner of the data, this means you have to free the memory
 * you have allocated for the data.
 *
 * @file   c_list.h
 */


/**
 * @typedef c_list_t
 * Creates a type name for c_list_s
 */
typedef struct c_list_s c_list_t;
/**
 * @struct c_list_s
 *
 * Used for each element in a doubly-linked list. The list can hold
 * any kind of data.
 */
struct c_list_s {
  /** Link to the next element in the list */
  struct c_list_s *next;
  /** Link to the previous element in the list */
  struct c_list_s *prev;

  /**
   * Holds the element's data, which can be a pointer to any kind of
   * data.
   */
  void *data;
};

/**
 * Specifies the type of a comparison function used to compare two values. The
 * value which should be returned depends on the context in which the
 * c_list_compare_fn is used.
 *
 * @param a             First parameter to compare with.
 *
 * @param b             Second parameter to compare with.
 *
 * @return              The function should return a number > 0 if the first
 *                      parameter comes after the second parameter in the sort
 *                      order.
 */
typedef int (*c_list_compare_fn) (const void *a, const void *b);

/**
 * Adds a new element on to the end of the list.
 *
 * @param list          A pointer to c_list.
 *
 * @param data          The data for the new element.
 *
 * @return              New start of the list, which may have changed, so make
 *                      sure you store the new value.
 */
c_list_t *c_list_append(c_list_t *list, void *data);

/**
 * Adds a new element on at the beginning of the list.
 *
 * @param list          A pointer to c_list.
 *
 * @param data          The data for the new element.
 *
 * @return              New start of the list, which may have changed, so make
 *                      sure you store the new value.
 */
c_list_t *c_list_prepend(c_list_t *list, void *data);

/**
 * Inserts a new element into the list at the given position. If the position
 * is lesser than 0, the new element gets appended to the list, if the position
 * is 0, we prepend the element and if the given position is greater than the
 * length of the list, the element gets appended too.
 *
 * @param list          A pointer to c_list.
 *
 * @param data          The data for the new element.
 *
 * @param position      The position to insert the element.
 *
 * @return              New start of the list, which may have changed, so make
 *                      sure you store the new value.
 */
c_list_t *c_list_insert(c_list_t *list, void *data, long position);

/**
 * Inserts a new element into the list, using the given comparison function to
 * determine its position.
 *
 * @param list          A pointer to c_list.
 *
 * @param data          The data for the new element.
 *
 * @param fn            The function to compare elements in the list. It
 *                      should return a number > 0 if the first parameter comes
 *                      after the second parameter in the sort order.
 *
 * @return              New start of the list, which may have changed, so make
 *                      sure you store the new value.
 */
c_list_t *c_list_insert_sorted(c_list_t *list, void *data,
    c_list_compare_fn fn);

/**
 * Allocates space for one c_list_t element.
 *
 * @return             A pointer to the newly-allocated element.
 */
c_list_t *c_list_alloc(void);

/**
 * Removes an element from a c_list. If two elements contain the same data,
 * only the first is removed.
 *
 * @param list          A pointer to c_list.
 *
 * @param data          The data of the element to remove.
 *
 * @return              The first element of the list, NULL on error.
 */
c_list_t *c_list_remove(c_list_t *list, void *data);

/**
 * Frees all elements from a c_list.
 *
 * @param list          A pointer to c_list.
 */
void c_list_free(c_list_t *list);

/**
 * Gets the next element in a c_list.
 *
 * @param               An element in a c_list.
 *
 * @return              The next element, or NULL if there are no more
 *                      elements.
 */
c_list_t *c_list_next(c_list_t *list);

/**
 * Gets the previous element in a c_list.
 *
 * @param               An element in a c_list.
 *
 * @return              The previous element, or NULL if there are no more
 *                      elements.
 */
c_list_t *c_list_prev(c_list_t *list);

/**
 * Gets the number of elements in a c_list
 *
 * @param list          A pointer to c_list.
 *
 * @return              The number of elements
 */
unsigned long c_list_length(c_list_t *list);

/**
 * Gets the first element in a c_list
 *
 * @param list          A pointer to c_list.
 *
 * @return              New start of the list, which may have changed, so make
 *                      sure you store the new value.
 */
c_list_t *c_list_first(c_list_t *list);

/**
 * Gets the last element in a c_list
 *
 * @param list          A pointer to c_list.
 *
 * @return              New start of the list, which may have changed, so make
 *                      sure you store the new value.
 */
c_list_t *c_list_last(c_list_t *list);

/**
 * Gets the element at the given positon in a c_list.
 *
 * @param list          A pointer to c_list.
 *
 * @param position      The position of the element, counting from 0.
 *
 * @return              New start of the list, which may have changed, so make
 *                      sure you store the new value.
 */
c_list_t *c_list_position(c_list_t *list, long position);

/**
 * Finds the element in a c_list_t which contains the given data.
 *
 * @param list          A pointer to c_list.
 *
 * @param data          The data of the element to remove.
 *
 * @return              The found element or NULL if it is not found.
 */
c_list_t *c_list_find(c_list_t *list, const void *data);

/**
 * Finds an element, using a supplied function to find the desired
 * element.
 *
 * @param list          A pointer to c_list.
 *
 * @param data          The data of the element to remove.
 *
 * @param func          The function to call for each element. It should
 *                      return 0 when the desired element is found.
 *
 * @return              The found element or NULL if it is not found.
 */
c_list_t *c_list_find_custom(c_list_t *list, const void *data,
    c_list_compare_fn fn);

/**
 * Sorts the elements of a c_list.
 * The algorithm used is Mergesort, because that works really well
 * on linked lists, without requiring the O(N) extra space it needs
 * when you do it on arrays.
 *
 * @param list          A pointer to c_list.
 *
 * @param func          The comparison function used to sort the c_list. This
 *                      function is passed 2 elements of the GList and should
 *                      return 0 if they are equal, a negative value if the
 *                      first element comes before the second, or a positive
 *                      value if the first element comes after the second.
 *
 * @return              New start of the list, which may have changed, so make
 *                      sure you store the new value.
 */
c_list_t *c_list_sort(c_list_t *list, c_list_compare_fn func);

#endif /* _C_LIST_H */

