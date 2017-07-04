#include <stdlib.h>

#include "list.h"


list nil = NULL;

void *head(list l) {
  return l->_list_car;
}

list tail(list l) {
  return l->_list_cdr;
}

list cons(void *hd, list tl) {
  list result = malloc(sizeof(_list));
  if (result == nil) {
    return nil;
  }
  result->_list_car = hd;
  result->_list_cdr = tl;
  return result;
}

void list_free(list l) {
  list nextl;
  while (l != nil) {
    nextl = tail(l);
    // unnecessary, but makes it easier to catch use-after-frees:
    l->_list_car = NULL;
    l->_list_cdr = NULL;
    free(l);
    l = nextl;
  }
}
