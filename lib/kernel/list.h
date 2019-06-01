#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "global.h"

#define offset(struct_type, member) (int)(&((struct_type *)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
    (struct_type *)((int)elem_ptr - offset(struct_type, struct_member_name))

/*----------------- define LIST type   --------------------*/
typedef struct _LIST_NODE
{
    struct _LIST_NODE* prev; 
    struct _LIST_NODE* next; 
} LIST_NODE, *PLIST_NODE;

typedef struct _LIST
{
    LIST_NODE head;     //fixed.    First element is head.next
    LIST_NODE tail;
} LIST, *PLIST;

/* callback in list_traversal */
typedef bool(*function)(PLIST_NODE, int arg);

void list_init(PLIST);
void list_insert_before(PLIST_NODE before, PLIST_NODE elem);
void list_push(PLIST plist, PLIST_NODE elem);
//void list_iterate(PLIST plist);
void list_append(PLIST plist, PLIST_NODE elem);
void list_remove(PLIST_NODE pelem);
PLIST_NODE list_pop(PLIST plist);
bool list_empty(PLIST plist);
uint32_t list_len(PLIST plist);
PLIST_NODE list_traversal(PLIST plist, function func, int arg);
bool elem_find(PLIST plist, PLIST_NODE obj_elem);

#endif