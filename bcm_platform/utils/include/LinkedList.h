#ifndef _LINKED_LIST_HEADER_
#define _LINKED_LIST_HEADER_

/**************************************************************** 
 *  This is the C-Style linked list implementation to be used   *
 *  for storing C-Style data structures only. Classed using     *
 *  run time polymorphism cannot use this implementation.       *
 * Reason                                                       *
*****************************************************************/

#define NULL 0

typedef struct _LIST_ENTRY {
   struct _LIST_ENTRY *Flink;
   struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;

//
// Accessor Function For Accessing The Data Structure 
// In the type independent linked list.
//
#define CONTAINING_RECORD(address, type, field) ((type *)( \
                                                  (char *)(address) - \
                                                  (char *)(&((type *)0)->field)))


void InitializeListHead (PLIST_ENTRY ListHead);
bool IsListEmpty        (const LIST_ENTRY * ListHead);
bool RemoveEntryList    (PLIST_ENTRY Entry);
void InsertTailList     (PLIST_ENTRY ListHead,PLIST_ENTRY Entry);
void InsertHeadList     (PLIST_ENTRY ListHead,PLIST_ENTRY Entry);
bool EntryExists        (PLIST_ENTRY ListHead, PLIST_ENTRY LookupEntry);

PLIST_ENTRY RemoveHeadList(PLIST_ENTRY ListHead);
PLIST_ENTRY RemoveTailList(PLIST_ENTRY ListHead);

//
// Walk through the list as follows
//
//for(thisEntry = listHead->Flink,nextEntry = thisEntry->Flink;thisEntry != listHead;
//        thisEntry = nextEntry,nextEntry = thisEntry->Flink)
//    {
//            PDECODED_FRAME pDecoFr = CONTAINING_RECORD(thisEntry,DECODED_FRAME,ListEntry);
//            DisplayEntry(pDecoFr);
//    }
//
//
#endif

