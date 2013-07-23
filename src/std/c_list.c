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

#include <stdio.h>
#include <stdlib.h>

#include "c_alloc.h"
#include "c_list.h"

/*
 * Adds a new element on to the end of the list.
 */
c_list_t *c_list_append(c_list_t *list, void *data) {
  c_list_t *new;
  c_list_t *last;

  new = c_list_alloc();
  if (new == NULL) {
    return NULL;
  }
  new->data = data;

  if (list == NULL) {
    return new;
  }

  last = c_list_last(list);

  last->next = new;
  new->prev = last;

  return list;
}

/*
 * Adds a new element on at the beginning of the list.
 */
c_list_t *c_list_prepend(c_list_t *list, void *data) {
  c_list_t *new;
  c_list_t *first;

  new = c_list_alloc();
  if (new == NULL) {
    return NULL;
  }
  new->data = data;

  if (list != NULL) {
    first = c_list_first(list);

    first->prev = new;
    new->next = first;
  }

  return new;
}

/*
 * Inserts a new element into the list at the given position.
 */
c_list_t *c_list_insert(c_list_t *list, void *data, long position) {
  c_list_t *new;
  c_list_t *temp;

  /* Handle wrong values for position */
  if (position < 0) {
    return c_list_append (list, data);
  } else if (position == 0) {
    return c_list_prepend (list, data);
  }

  temp = c_list_position(list, position);

  if (temp == NULL) {
    return c_list_append(list, data);
  }

  new = c_list_alloc();
  if (new == NULL) {
    return NULL;
  }
  new->data = data;

  /* List is not empty */
  if (temp->prev) {
    temp->prev->next = new;
    new->prev = temp->prev;
  }

  new->next = temp;
  temp->prev = new;

  /*  */
  if (temp == list) {
    return new;
  }

  return list;
}

/*
 * Inserts a new element into the list, using the given comparison function to
 * determine its position.
 */
c_list_t *c_list_insert_sorted(c_list_t *list, void *data,
    c_list_compare_fn fn) {
  c_list_t *new;
  c_list_t *temp;
  int cmp;

  new = c_list_alloc();
  if (new == NULL) {
    return NULL;
  }
  new->data = data;

  /* list is empty */
  if (list == NULL) {
    return new;
  }

  temp = list;
  cmp = (fn)(data, temp->data);

  while ((temp->next) && (cmp > 0)) {
    temp = temp->next;

    cmp = (fn)(data, temp->data);
  }

  /* last element */
  if ((temp->next == NULL) && (cmp > 0)) {
    temp->next = new;
    new->prev = temp;
    return list;
  }

  /* first element */
  if (temp->prev) {
    temp->prev->next = new;
    new->prev = temp->prev;
  }

  new->next = temp;
  temp->prev = new;

  /* inserted before first */
  if (temp == list) {
    return new;
  }

  return list;
}

/*
 * Allocates space for one c_list_t element.
 */
c_list_t *c_list_alloc(void) {
  c_list_t *list = NULL;

  list = c_malloc(sizeof(c_list_t));
  if (list == NULL) {
    return NULL;
  }

  list->data = NULL;

  list->prev = NULL;
  list->next = NULL;

  return list;
}

/*
 * Removes an element from a c_list. If two elements contain the same data,
 * only the first is removed.
 */
c_list_t *c_list_remove(c_list_t *list, void *data) {
  c_list_t *temp;

  if (list == NULL || data == NULL) {
    return NULL;
  }

  temp = list;

  while (temp != NULL) {
    if (temp->data != data) {
      temp = temp->next;
    } else {
      /* not at first element */
      if (temp->prev) {
        temp->prev->next = temp->next;
      }

      /* not at last element */
      if (temp->next) {
        temp->next->prev = temp->prev;
      }

      /* first element */
      if (list == temp) {
        list = list->next;
      }

      SAFE_FREE(temp);
      break;
    }
  }

  return list;
}

/*
 * Frees all elements from a c_list.
 */
void c_list_free(c_list_t *list) {
  c_list_t *temp = NULL;

  if (list == NULL) {
    return;
  }

  list = c_list_last(list);

  while (list->prev != NULL) {
    temp = list;
    list = list->prev;

    SAFE_FREE(temp);
  }
  SAFE_FREE(list);
}

/*
 * Gets the next element in a c_list.
 */
c_list_t *c_list_next(c_list_t *list) {
  if (list == NULL) {
    return NULL;
  }

  return list->next;
}

/*
 * Gets the previous element in a c_list.
 */
c_list_t *c_list_prev(c_list_t *list) {
  if (list == NULL) {
    return NULL;
  }

  return list->prev;
}

/*
 * Gets the number of elements in a c_list
 */
unsigned long c_list_length(c_list_t *list) {
  unsigned long length = 1;

  if (list == NULL) {
    return 0;
  }

  while (list->next) {
    length++;
    list = list->next;
  }

  return length;
}

/*
 * Gets the first element in a c_list
 */
c_list_t *c_list_first(c_list_t *list) {
  if (list != NULL) {
    while (list->prev) {
      list = list->prev;
    }
  }

  return list;
}

/*
 * Gets the last element in a c_list
 */
c_list_t *c_list_last(c_list_t *list) {
  if (list != NULL) {
    while (list->next) {
      list = list->next;
    }
  }

  return list;
}

/*
 * Gets the element at the given positon in a c_list
 */
c_list_t *c_list_position(c_list_t *list, long position) {
  if (list == NULL) {
    return NULL;
  }

  while ((position-- > 0) && list != NULL) {
    list = list->next;
  }

  return list;
}

/*
 * Finds the element in a c_list_t which contains the given data.
 */
c_list_t *c_list_find(c_list_t *list, const void *data) {
  if (list == NULL) {
    return NULL;
  }

  while (list != NULL) {
    if (list->data == data) {
      break;
    }
    list = list->next;
  }
  return list;
}

/*
 * Finds an element, using a supplied function to find the desired
 * element.
 */
c_list_t *c_list_find_custom(c_list_t *list, const void *data,
    c_list_compare_fn fn) {
  int cmp;

  if (list != NULL && fn != NULL) {
    while (list != NULL) {
      cmp = (*fn)(list->data, data);
      if (cmp == 0) {
        return list;
      }
      list = list->next;
    }
  }

  return NULL;
}

/*
 * Internal used function to merge 2 lists using a compare function
 */
static c_list_t *_c_list_merge(c_list_t *list1, c_list_t *list2,
    c_list_compare_fn func) {
  int cmp;

  /* lists are emty */
  if (list1 == NULL) {
    return list2;
  } else if (list2 == NULL) {
    return list1;
  }

  cmp = ((c_list_compare_fn) func)(list1->data, list2->data);
  /* compare if it is smaller */
  if (cmp <= 0) {
    list1->next = _c_list_merge(list1->next, list2, func);
    if (list1->next) {
      list1->next->prev = list1;
    }return list1;
  } else {
    list2->next = _c_list_merge(list1, list2->next, func);
    if (list2->next) {
      list2->next->prev = list2;
    }
    return list2;
  }
}

/*
 * Internally used function to split 2 lists.
 */
static c_list_t *_c_list_split(c_list_t *list) {
  c_list_t *second = NULL;

  /* list is empty */
  if (list == NULL) {
    return NULL;
  } else if (list->next == NULL) {
  /* list has only 1 element */
    return NULL;
  } else {
    /* split */
    second = list->next;
    list->next = second->next;
    /* is last element */
    if (list->next) {
      list->next->prev = list;
    }

    second->prev = NULL;
    second->next = _c_list_split(second->next);
    /* is last element */
    if (second->next) {
      second->next->prev = second;
    }

    return second;
  }

  /* never reached */
  return NULL;
}

/*
 * Sorts the elements of a c_list. This is a merge sort.
 */
c_list_t *c_list_sort(c_list_t *list, c_list_compare_fn func) {
  c_list_t *second;

  /* list is empty */
  if (list == NULL) {
    return NULL;
  } else if (list->next == NULL) {
    /* list has only one element */
    return list;
  } else {
    /* split list */
    second = _c_list_split(list);
  }

  return _c_list_merge(c_list_sort(list, func), c_list_sort(second, func),
      func);
}

