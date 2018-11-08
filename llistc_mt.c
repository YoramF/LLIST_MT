/*
 * llistc_mt.c
 *
 *  Created on: Oct 31, 2018
 *      Author: yoram
 *
 *  Simple unsorted list.
 *  Provide sequantial insert, and list/retrieve functions.
 *  The list can store any variable type.
 *  For the list() function a callback function should be provided.
 *  This callback function will be called for each of the stored values
 *
 *  THis version is multi thread safe .i.e. multiple threads can access shared list safly
 *  There is one critial location that need to be synchronized:
 *  1 - insert new element to LIST
 *
 *  In thise version, getNextElement() & resetList() will use private counters and pointers per calling thread
 *  new getNextElementO() function optimized for sequential element inserts
 *
 */

#include "llistc_mt.h"

#include <stdlib.h>		// for malloc()
#include <string.h>		// for memcpy()
#include <stdio.h>		// for pritnf()
#include <pthread.h>	// for mutex() services


#define MINLIST 100
typedef LIST lnodePTR;


// Add new lnode to existing list
static lnode *addNode (const LIST list, const void *element)
{
	lnode *lnodePtr;
	void *ptr;

	// allocate the array of elements
	// no need to ZERO allocated RAM - use malloc() which is faster than calloc()
	if ((ptr = malloc(list->sizeOfElement)) == NULL)
		return NULL;

	// allocate the lnode
	if ((lnodePtr = (lnode *)malloc(sizeof(lnode))) != NULL)
	{
		lnodePtr->next = NULL;
		lnodePtr->aval = ptr;
	}
	else
	{
		free(ptr);
		return NULL;
	}

	// Copy value of element to lnode->aval
	switch (list->optimize)
	{
	case 1:
		*(char *)ptr = *(char *)element;
		break;
	case 2:
		*(unsigned short *)ptr = *(unsigned short *)element;
		break;
	case 4:
		*(unsigned long *)ptr = *(unsigned long *)element;
		break;
	case 8:
		*(unsigned long long *)ptr = *(unsigned long long *)element;
		break;
	default:
		memcpy(ptr, element, list->sizeOfElement);
	}

	return lnodePtr;
}


// Initialize a list of elements
// inintElelemnts - minimal number of elements in the list
// sizeOfElements - size of an element in the list in bytes
LIST createList (const int sizeOfElement)
{
	LIST list = NULL;

	// allocate List Header record
	if ((list = malloc(sizeof(listHDR))) == NULL)
		return NULL;

	// Initialize list header
	list->sizeOfElement = sizeOfElement;
	list->first = NULL;
	list->optimize = (sizeOfElement < 9? sizeOfElement: 9);

	// initialize mutex
	if (pthread_mutex_init(&(list->insert_mtx), NULL) != 0)
	{
		free (list);
		return NULL;
	}

	return list;
}

// Release all memory taken by list
void deleteList (LIST *list)
{
	lnode *lptr, *lptrn;

	// Scan all lnodes, release memory pointed by *arr and then relese lnode itself
	lptr = (*list)->first;
	while (lptr != NULL)
	{
		lptrn = lptr->next;
		if (lptr->aval != NULL)
			free(lptr->aval);
		free(lptr);
		lptr = lptrn;
	}

	// Release mutex
	pthread_mutex_destroy(&((*list)->insert_mtx));

	free(*list);
	*list = NULL;
}


void insertElement (const LIST list, const void *element, int (*cfunc)(void *, void *))
{
	lnode *lnodePtr, *lnodeNew = NULL, *lnodePrev;
	int cmp = 1;

	// Lock list for new element insertion
	if (pthread_mutex_lock(&(list->insert_mtx)) != 0)
		return;

	 lnodePtr = lnodePrev = list->first;

	 // Find the right place to insert the new elelent
	 while (lnodePtr != NULL)
	 {
		 if ((cmp = cfunc(lnodePtr->aval, (void *)element)) < 0)
		 {
			 // move to next lnode since new element is bigger
			 lnodePrev = lnodePtr;
			 lnodePtr = lnodePtr->next;
		 }
		 else
			 break;		// new element is either equal or smaller than current element
	 }

	 // check if element in current lnode is not equale new element.
	 // otherwhise - do nothing
	 if (cmp != 0)
	 {
		 // At this point we nweed to add new lnode to the list
		 if ((lnodeNew = addNode(list, element)) == NULL)
			 return;

		 // Check if we broke out of the loop because we reached end of list.
		 // in this case, add the new node at the end of the list;
		 // Since initial cmp value is 1, and we check for cmp < 0, it means we loop at least once therefore lnodePrev
		 // must be different than NULL
		 if (cmp < 0)
			 lnodePrev->next = lnodeNew;
		 else
		 {
			 // we need to add the new node before lnodePtr.
			 // we have two otions:
			 // 1 - lnodePrev points to valid lnode
			 // 2 - lnodePrev points to null (i.e. List is empty) or new elements is smaller the first element in list
			 if (lnodePrev == NULL)
				 // list is empty. Just add the new lnode as first node in the list
				 list->first = lnodeNew;
			 else
			 {
				lnodeNew->next = lnodePtr;

				if (lnodePrev != lnodePtr)
					// new node become the first node in the list
					lnodePrev->next = lnodeNew;
				else
					list->first = lnodeNew;
			 }
		 }
	 }

	// Release lock
	pthread_mutex_unlock(&(list->insert_mtx));
}

// same as insertElement except that it start scanning the list from lnodePtr location
// for sorted insertion it should speed item insertion for very large lists
lnode *insertElementO (const LIST list, const void *element, int (*cfunc)(void *, void *), lnode *lnodeP)
{
	lnode *lnodePtr, *lnodeNew = NULL, *lnodePrev;
	int cmp = 1;

	// Lock list for new element insertion
	if (pthread_mutex_lock(&(list->insert_mtx)) != 0)
		return NULL;

	lnodePtr = lnodePrev = list->first;

	 // check that the inserted element is larger than the element pointed on by lnodeP
	 if (lnodeP != NULL)
	 {
		if ((cmp = cfunc(lnodeP->aval, (void *)element)) < 0)
		{
			lnodePrev = lnodeP;
			lnodePtr = lnodeP->next;
		}
		else if (cmp == 0)
		{
			// Noting to add
			lnodeNew = lnodeP;
			goto end;
		}
	 }

	 // Find the right place to insert the new elelent
	 while (lnodePtr != NULL)
	 {
		 if ((cmp = cfunc(lnodePtr->aval, (void *)element)) < 0)
		 {
			 // move to next lnode since new element is bigger
			 lnodePrev = lnodePtr;
			 lnodePtr = lnodePtr->next;
		 }
		 else
			 break;		// new element is either equal or smaller than current element
	 }

	 // check if element in current lnode is not equale new element.
	 // otherwhise - do nothing
	 if (cmp != 0)
	 {
		 // At this point we need to add new lnode to the list
		 if ((lnodeNew = addNode(list, element)) == NULL)
			 return NULL;

		 // Check if we broke out of the loop because we reached end of list.
		 // in this case, add the new node at the end of the list;
		 // Since initial cmp value is 1, and we check for cmp < 0, it means we loop at least once therefore lnodePrev
		 // must be different than NULL
		 if (cmp < 0)
			 lnodePrev->next = lnodeNew;
		 else
		 {
			 // we need to add the new node before lnodePtr.
			 // we have two otions:
			 // 1 - lnodePrev points to valid lnode
			 // 2 - lnodePrev points to null (i.e. List is empty) or new elements is smaller the first element in list
			 if (lnodePrev == NULL)
				 // list is empty. Just add the new lnode as first node in the list
				 list->first = lnodeNew;
			 else
			 {
				lnodeNew->next = lnodePtr;

				if (lnodePrev != lnodePtr)
					// new node become the first node in the list
					lnodePrev->next = lnodeNew;
				else
					list->first = lnodeNew;
			 }
		 }
	 }
end:
	// Release lock
	pthread_mutex_unlock(&(list->insert_mtx));

	return lnodeNew;
}



// List all elements in LIST using callback function
// Since list can be used with any type variables, the calling module must provide the right print function
// for the stored variable
long long int listElements (const LIST list, const void (*lfunc)(void *))
{
	long long int elements = 0;
	lnode *lnodePtr = list->first;

	// Scan all lnodes in the list
	while (lnodePtr != NULL)
	{
		// increment total number of listed elements
		elements++;

		// call the callback function with the cuttent value
		lfunc(lnodePtr->aval);
		lnodePtr = lnodePtr->next;
	}
	return elements;
}

// Get next element stored in list
// Before first call to getNextElement, we must call resetList()
int getNextElement (const LIST list, lnode **lnodePtr, void *val)
{
	lnode *lnp = *lnodePtr;

	if (lnp == NULL)
		return 0;

	switch (list->optimize)
	{
		case 1:
			*(char *)val = *(char *)lnp->aval;
			break;
		case 2:
			*(unsigned short *)val = *(unsigned short *)lnp->aval;
			break;
		case 4:
			*(unsigned long *)val = *(unsigned long *)lnp->aval;
			break;
		case 8:
			*(unsigned long long *)val = *(unsigned long long *)lnp->aval;
			break;
		default:
			memcpy(val, lnp->aval, list->sizeOfElement);
	}

	*lnodePtr = lnp->next;
	return 1;
}

// Must be called once before looping on list and calling getNextElement
void resetList (const LIST list, lnode **next)
{
	*next = list->first;
}
