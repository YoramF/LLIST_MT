/*
 * llistc_mt.h
 *
 *  Created on: Oct 31, 2018
 *      Author: yoram
 */

#ifndef LLISTC_MT_H_
#define LLISTC_MT_H_

#include <pthread.h>

#define LISTC_INODE_MAX_ELEMENTS 0x7fffffff
#define LISTC_MAX_ELEMENTS 0x7fffffffffffffff

typedef struct _lnode
{
	void 			*aval;			// Pointer to an element. Element can be of any type!
	struct _lnode	*next;			// Pointer to next node in the global list
} lnode;

typedef struct _list
{
	int				sizeOfElement;	// Size of an element in bytes
	int				optimize;		// used to optimize insertElement()
	pthread_mutex_t insert_mtx;		// mutex for thread synchronization
	lnode 			*first;			// Point to first elements lnode
} listHDR;

typedef listHDR *LIST;

LIST createList (const int sizeOfElement);
void deleteList (LIST *list);
void insertElement (const LIST list, const void *element, const int (*cfunc)(void *, void *));
lnode *insertElementO (const LIST list, const void *element, const int (*cfunc)(void *, void *), lnode *lnodeP);
long long int listElements (const LIST list, const void (*lfunc)(void *));
int getNextElement (const LIST list, lnode **lnodePtr, void *val);
void resetList (const LIST list, lnode **lnodePtr);

#endif /* LLISTC_MT_H_ */
