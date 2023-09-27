/**
 * \author Koen Eelen
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list 

#ifdef DEBUG
#define DEBUG_PRINTF(...) 									                                        \
        do {											                                            \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	    \
            fprintf(stderr,__VA_ARGS__);								                            \
            fflush(stderr);                                                                         \
                } while(0)
#else
#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition, err_code)                         \
    do {                                                                \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");      \
            assert(!(condition));                                       \
        } while(0)


/*
 * The real definition of struct list / struct node
 */

struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist {
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};

dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_MEMORY_ERROR);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) {
    dplist_node_t *current = (*list)->head;
    while( current != NULL )
    {
        dplist_node_t *dummy = current->next; 
        if(free_element) (*list)->element_free(&(current->element));
        free(current);
        current = dummy;
    }
    free(current);
    free(*list);
    *list = NULL;
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {
    dplist_node_t *ref_at_index, *list_node;
    if (list == NULL) return NULL;
    list_node = malloc(sizeof(dplist_node_t));
    DPLIST_ERR_HANDLER(list_node == NULL, DPLIST_MEMORY_ERROR);
    if (insert_copy) (list_node->element) = list->element_copy(element);
    else list_node->element = element;
    // pointer drawing breakpoint
    if (list->head == NULL) { // covers case 1
        list_node->prev = NULL;
        list_node->next = NULL;
        list->head = list_node;
        // pointer drawing breakpoint
    } else if (index <= 0) { // covers case 2
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
        // pointer drawing breakpoint
    } else {
        ref_at_index = dpl_get_reference_at_index(list, index);
        assert(ref_at_index != NULL);
        // pointer drawing breakpoint
        if (index < dpl_size(list)) { // covers case 4
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node;
            ref_at_index->prev = list_node;
            // pointer drawing breakpoint
        } else { // covers case 3
            assert(ref_at_index->next == NULL);
            list_node->next = NULL;
            list_node->prev = ref_at_index;
            ref_at_index->next = list_node;
            // pointer drawing breakpoint
        }
    }
    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {
    if (list == NULL) return NULL;
    if (list->head == NULL) return list;

    dplist_node_t *current = dpl_get_reference_at_index(list, index);

    if (index <= 0)
    {
        list->head = current->next;
        if(current->next != NULL) (list->head)->prev = NULL;
        if(free_element) list->element_free(&(current->element));
        free(current);

    } else if (index >= dpl_size(list)-1)
    {
        if(dpl_size(list)>1)(current->prev)->next = NULL;
        else list->head = NULL;
        if(free_element) list->element_free(&(current->element));
        free(current);
    } else
    {
        dplist_node_t *prev = current->prev;
        dplist_node_t *next = current->next;
        prev->next = next;
        next->prev = prev;
        free(current);
    }
    return list;
}

int dpl_size(dplist_t *list) {
    if (list == NULL) return -1;

    int count = 0;
    dplist_node_t *current = (list->head);
    while (current != NULL)
    {
        count++;
        current = current->next;
    }
    return count;
}

void *dpl_get_element_at_index(dplist_t *list, int index) {
    if (list == NULL) return NULL;
    if (list->head == NULL) return NULL;
    if (index <= 0) return (list->head)->element;

    int count = 0;
    dplist_node_t *current = list->head;
    while (current->next != NULL && count < index)
    {
        current = current->next;
        count++;
    }
    return current->element;
}

int dpl_get_index_of_element(dplist_t *list, void *element) {
    if (list == NULL) return -1;

    int count = 0;
    dplist_node_t * current = (list->head);
    while (current != NULL)
    {
        if(list->element_compare(current->element,element) == 0) return count;
        current = current->next;    
        count++;
    }
    return -1;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
    int count;
    dplist_node_t *dummy;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    if (list->head == NULL) return NULL;
    for (dummy = list->head, count = 0; dummy->next != NULL; dummy = dummy->next, count++) {
        if (count >= index) return dummy;
    }
    return dummy;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {
    if(reference == NULL || list == NULL || list->head == NULL) return NULL;

    dplist_node_t * current = (list->head);
    while (current != NULL)
    {
        if (current == reference)
        {
            return current->element;
        }
        current = current->next;
    }
    return NULL;
}

dplist_node_t *dpl_get_reference_of_element(dplist_t *list, void *element) {
    if (list == NULL) return NULL;

    int count = 0;
    dplist_node_t * current = (list->head);
    while (current != NULL)
    {
        if(list->element_compare(current->element,element) == 0) return current;
        current = current->next;    
        count++;
    }
    return NULL;
}

void *dpl_get_element(dplist_t *list, void*element) {
    int index = dpl_get_index_of_element(list, element);
    if (index == -1) return NULL;
    return dpl_get_element_at_index(list,index);
}
