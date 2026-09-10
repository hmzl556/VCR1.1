
#ifndef BT_LIST_EXT_H
#define BT_LIST_EXT_H

/*---------------------------------------------------------------------------
Module Name:
    Pre-Pointer, Variable Preload List management
Abstract:
    This module defines the Bi-Direction and Single-Direction List manage functions.
Author:
    Wang Yao
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                                 Pre 2 PTR * List                              */
/*---------------------------------------------------------------------------*/
#ifndef UINT8
#define UINT8   unsigned char
#endif

#ifndef UINT32
#define UINT32    unsigned long
#endif

struct ListStru {/* New List Structure by hiw */
    void *head;
    void *tail;
};

struct List2Stru {
    void *p[2];
};

/* Bidirection List Node */
#define LGET(n)         ((void **)(n) + (1 * 2))/* From memory start ptr go to userdata */
#define LSTART(n)       ((void **)(n) - (1 * 2))/* From userdata get back to memory start ptr */
#define LNEXT(n)        (*((void **)(n) - (1 * 2)))/* a story of http://www.googies.info/blog/945.html */
#define LPREV(n)        (*((void **)(n) - 1))
#define LP(n, k)        (*((void **)(n) - (1 * (k + 1))))/* k=0 is LPREV, k=1 is LNEXT */
#define LHT(l, k)        (((struct List2Stru *)(l))->p[k])/* k=0 is Head or k=1 is Tail */
#define LFREE(n)        (FREE(LSTART(n)))/* Input ListNode userdata Ptr, Free it */

#define LIST_STRUOFF(stru_, mem_)                ((UINT32)(&(((stru_)(NULL))->mem_)))
#define LIST_STRUMEMLEN(stru_, mem_)            sizeof((((stru_)(NULL))->mem_))
void *List_Find_(struct ListStru *list, UINT32 off, UINT32 len, UINT8 *val);/* Shall not be used by UI */

#define VARLEN(stru_, mem_, len_)                (((UINT32)(&(((stru_)(NULL))->mem_))) + (len_))
#define NEWVARLEN(stru_, mem_, len_)            NEW(VARLEN(stru_, mem_, len_))

/*---------------------------------------------------------------------------
Example:
    LIST_FIND(&L2CAP_CID_LIST, struct L2CAP_CIDListStru *, cid, val);
---------------------------------------------------------------------------*/
#define LIST_FIND(list_, stru_, mem_, val_)              List_Find_(list_, LIST_STRUOFF(stru_, mem_), LIST_STRUMEMLEN(stru_, mem_), (UINT8 *)(&(val_)))/* This is UI */
#define LIST_FINDLEN(list_, stru_, mem_, len_, pval_)    List_Find_(list_, LIST_STRUOFF(stru_, mem_), (len_), (UINT8 *)(pval_))
#define LIST_FINDLENOFF0(list_, len_, pval_)             List_Find_(list_, 0, (len_), (UINT8 *)(pval_))

UINT32 List_Count(struct ListStru *list);
void *List_NodeExist(struct ListStru *list, void *p);

#define List_AddHeadNew(list, node, len) List_AddHead(list, (*node = List_NodeNew(len)))
#define List_AddNew(list, node, len) List_AddTail(list, (*node = List_NodeNew(len)))

void *_List_NodeNew(UINT32 len);/* DONOT use this, see below */
void List_AddTail(struct ListStru *list, void *node);
void List_AddHead(struct ListStru *list, void *node);
void List_InsertAt(struct ListStru *list, void *pos, void *node);
void List_InsertAfter(struct ListStru *list, void *pos, void *node);
void *List_RemoveHead(struct ListStru *list);
void *List_RemoveTail(struct ListStru *list);
void List_RemoveAt(struct ListStru *list, void *node);
void List_RemoveAll(struct ListStru *list);
void List_InsertSublistAt(struct ListStru *list, void *pos, struct ListStru *sub_list);
void List_RemoveSublist(struct ListStru *list, struct ListStru *sub_list);

#ifdef CONFIG_MEMORY_LEAK_DETECT
void *DBG_List_NodeNew(UINT32 len, char* filename, int line);
#define List_NodeNew(l) DBG_List_NodeNew(l, __FILE__, __LINE__)
#else
#define List_NodeNew(l)        _List_NodeNew(l)
#endif

#endif

