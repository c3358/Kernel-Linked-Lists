#pragma once
#include <ntddk.h>

#define MAX_PATH 50
#define LIST_ENTRY_ZERO_MEMORY 0
#define BYTES_OF_DIRECTORY 65530

typedef struct _FILE_BUFFER
{
	LIST_ENTRY Entry;
	WCHAR FileName[MAX_PATH];

}FILE_BUFFER, *PFILE_BUFFER;

typedef struct _FILE_LIST_HEAD
{
	LIST_ENTRY Entry;
	ULONG NumberOfElements;

}FILE_LIST_HEAD, *PFILE_LIST_HEAD;


extern PFILE_LIST_HEAD g_filelisthead;
extern 	KSPIN_LOCK g_lock;

PFILE_LIST_HEAD FileListEntry();

