#include "list.h"
#include "interrupt.h"

void list_init(PLIST list)
{
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

void list_insert_before(PLIST_NODE before, PLIST_NODE node)
{
    INTR_STATUS old_status = intr_disable();

    before->prev->next = node;
    node->prev = before->prev;
    node->next = before;

    before->prev = node;

    intr_set_status(old_status);
}

// insert node into head
void list_push(PLIST plist, PLIST_NODE elem)
{
    list_insert_before(plist->head.next, elem);
}
void list_iterate(PLIST plist)
{
}

// insert node into tail
void list_append(PLIST plist, PLIST_NODE elem)
{
    list_insert_before(&plist->tail, elem);
}

//remove node from the list
void list_remove(PLIST_NODE pelem)
{
    INTR_STATUS old_status = intr_disable();
    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;
    intr_set_status(old_status);
}

//pop list first node and return itself
PLIST_NODE list_pop(PLIST plist)
{
    PLIST_NODE node = plist->head.next;
    list_remove(node);
    return node;
}

//return true if list is empty
bool list_empty(PLIST plist)
{
    return (plist->head.next == &plist->tail ? true : false);
}

//return length of list
uint32_t list_len(PLIST plist)
{
    PLIST_NODE node = plist->head.next;
    uint32_t length = 0;
    while (node != &plist->tail)
    {
        length++;
        node = node->next;
    }
    return length;
}

// return first node which satisfies the func CALLBACK
// arg and func : judge whether the node should callback
// return NULL if fail
PLIST_NODE list_traversal(PLIST plist, function func, int arg)
{
    PLIST_NODE node = plist->head.next;
    if (list_empty(plist))
    {
        return NULL;
    }

    while (node != &plist->tail)
    {
        if (func(node, arg))
        { // satisfied
            return node;
        }
        node = node->next;
    }
    return NULL;
}

// find obj_node in plist
// return true if find
bool elem_find(PLIST plist, PLIST_NODE obj_node)
{
    PLIST_NODE node = plist->head.next;
    while (node != &plist->tail)
    {
        if (node == obj_node)
        {
            return true;
        }
        node = node->next;
    }
    return false;
}
