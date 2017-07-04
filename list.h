typedef struct _list {
  void *_list_car;
  struct _list *_list_cdr;
} _list, *list;

list nil;
void *head(list l);
list tail(list l);
list cons(void *hd, list tl);
void list_free(list l);
