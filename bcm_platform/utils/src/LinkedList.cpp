#include "LinkedList.h"


void
InitializeListHead(PLIST_ENTRY ListHead)
{
    ListHead->Flink = ListHead->Blink = ListHead;
}


bool
IsListEmpty(const LIST_ENTRY * ListHead)
{
    return (bool)(ListHead->Flink == ListHead);
}


bool
RemoveEntryList(PLIST_ENTRY Entry)
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Flink;

    Flink = Entry->Flink;
    Blink = Entry->Blink;
    Blink->Flink = Flink;
    Flink->Blink = Blink;
    return (bool)(Flink == Blink);
}


PLIST_ENTRY
RemoveHeadList(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY Flink;
    PLIST_ENTRY Entry;

    if (IsListEmpty(ListHead)) 
        return NULL;

    Entry = ListHead->Flink; 
    Flink = Entry->Flink;
    ListHead->Flink = Flink;
    Flink->Blink = ListHead;
    return Entry;
}



PLIST_ENTRY  
RemoveTailList(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Entry;

    if (IsListEmpty(ListHead)) 
        return NULL;

    Entry = ListHead->Blink;
    Blink = Entry->Blink;
    ListHead->Blink = Blink;
    Blink->Flink = ListHead;
    return Entry;
}


void
InsertTailList(PLIST_ENTRY ListHead,PLIST_ENTRY Entry)
{
    PLIST_ENTRY Blink;

    Blink = ListHead->Blink;
    Entry->Flink = ListHead;
    Entry->Blink = Blink;
    Blink->Flink = Entry;
    ListHead->Blink = Entry;
}

void
InsertHeadList(PLIST_ENTRY ListHead,PLIST_ENTRY Entry)
{
    PLIST_ENTRY Flink;

    Flink = ListHead->Flink;
    Entry->Flink = Flink;
    Entry->Blink = ListHead;
    Flink->Blink = Entry;
    ListHead->Flink = Entry;
}

bool
EntryExists(PLIST_ENTRY ListHead, PLIST_ENTRY LookupEntry)
{
    PLIST_ENTRY ThisEntry=NULL, NextEntry=NULL;
    
    if (IsListEmpty(ListHead)) {
        return false;
    }

    for(ThisEntry = ListHead->Flink,NextEntry = ThisEntry->Flink;ThisEntry != ListHead; 
        ThisEntry = NextEntry,NextEntry = ThisEntry->Flink)
    {
        if (LookupEntry == ThisEntry) {
            return true;
        }
    }
    return false;
}

