#include <ntddk.h>
#include "Proc.h"

fnZwQuerySystemInformation ZwQuerySystemInformation = NULL;
PPROCESS_HEAD_LIST g_processheadlist = NULL;



VOID Unload(DRIVER_OBJECT *pDriverObject)
{
	KdPrint(("Driver Unloaded"));
}

LONG DriverEntry(DRIVER_OBJECT *pDriverObject, UNICODE_STRING *RegisterPath)
{
	KdPrint(("Driver Loaded"));

	pDriverObject->DriverUnload = Unload;

	LONG st;
	PVOID mem;
	UNICODE_STRING ustr1;
	ULONG bytes = 0;
	PSYSTEM_PROCESS_INFO info = NULL;
	PPROCESS_BUFFER alloc = NULL;
	PPROCESS_BUFFER buffer = NULL;

	RtlSecureZeroMemory(&ustr1, sizeof(ustr1));
	RtlInitUnicodeString(&ustr1, L"ZwQuerySystemInformation");
	
	ZwQuerySystemInformation = MmGetSystemRoutineAddress(&ustr1);
	if (ZwQuerySystemInformation == NULL)
		return STATUS_UNSUCCESSFUL;

	g_processheadlist = (PPROCESS_HEAD_LIST)ExAllocatePoolWithTag(NonPagedPool, sizeof(PROCESS_HEAD_LIST), 'JxK');
	if (g_processheadlist == NULL)
		return LIST_ENTRY_ZERO_MEMORY;

	g_processheadlist->NumberOfProcess = 0;

	st = ZwQuerySystemInformation(SystemProcessInformation, NULL, 0, &bytes);
	
	if (st == STATUS_INFO_LENGTH_MISMATCH)
	{
		mem = ExAllocatePool(NonPagedPool, bytes);
	
		if (mem != NULL)
		{
			st = ZwQuerySystemInformation(SystemProcessInformation, mem, bytes, &bytes);
			
			if (NT_SUCCESS(st))
			{
				info = (PSYSTEM_PROCESS_INFO)mem;
				
				if (info && MmIsAddressValid(info))
				{

					InitializeListHead(&g_processheadlist->Entry);

					while (info->NextEntryOffset)
					{
						info = (PSYSTEM_PROCESS_INFO)((PUCHAR)info + info->NextEntryOffset);


						buffer = (PPROCESS_BUFFER)ExAllocatePool(NonPagedPool, sizeof(PROCESS_BUFFER));
						RtlSecureZeroMemory(buffer, sizeof(PROCESS_BUFFER));

						buffer->ProcessId = (ULONG)info->ProcessId;
						
						buffer->ProcessInherited = (ULONG)info->InheritedFromProcessId;
						
						buffer->NumberOfThreads = info->NumberOfThreads;
						
						RtlCopyMemory(buffer->ProcessName, info->ImageName.Buffer, info->ImageName.Length);

						InsertHeadList(&g_processheadlist->Entry, &buffer->Entry);

						g_processheadlist->NumberOfProcess++;

					}
				}
			}
			ExFreePool(mem);
		}
	}

	while (!IsListEmpty(&g_processheadlist->Entry))
	{
		alloc = (PPROCESS_BUFFER)RemoveHeadList(&g_processheadlist->Entry);

		DbgPrint("\n%1ws %20lu %20lu %20lu", alloc->ProcessName, alloc->ProcessId, alloc->ProcessInherited, alloc->NumberOfThreads);
		
		ExFreePool(alloc);
	}

	ExFreePoolWithTag(g_processheadlist, 'JxK');
	
	g_processheadlist = NULL;


	return STATUS_SUCCESS;
}