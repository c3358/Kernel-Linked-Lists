#include <ntifs.h>
#include <ntddk.h>

#include "File.h"


PFILE_LIST_HEAD g_filelisthead = NULL;
KSPIN_LOCK g_lock = { 0 };


NTSTATUS QueryCompletion(PDEVICE_OBJECT DeviceObject,PIRP Irp,PVOID Context)
{
	PIO_STATUS_BLOCK ioStatus;

	ioStatus = Irp->UserIosb;
	ioStatus->Status = Irp->IoStatus.Status;
	ioStatus->Information = Irp->IoStatus.Information;

	KeSetEvent(Irp->UserEvent, 0, FALSE);
	IoFreeIrp(Irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID Unload(DRIVER_OBJECT *pDriverObject)
{
	KdPrint(("\nDriver Unloaded"));
}

LONG DriverEntry(DRIVER_OBJECT *pDriverObjec, UNICODE_STRING *RegisterPath)
{
	KdPrint(("\nDriver Loaded"));

	pDriverObjec->DriverUnload = Unload;

	LONG st;
	PIRP Irp;
	PFILE_BUFFER FileBuffer = NULL;
	PFILE_BUFFER alloc = NULL;
	PFILE_OBJECT FileObject = NULL;
	PDEVICE_OBJECT DeviceObject = NULL;
	KEVENT Event;
	PIO_STACK_LOCATION pio;
	IO_STATUS_BLOCK io = { 0 };
	UNICODE_STRING filepath = { 0 };
	OBJECT_ATTRIBUTES oa = { 0 };
	HANDLE FileHandle;
	PVOID buffer = NULL;
	PFILE_BOTH_DIR_INFORMATION dirinfo = NULL;


	g_filelisthead = (PFILE_LIST_HEAD)ExAllocatePoolWithTag(NonPagedPool, sizeof(FILE_LIST_HEAD), 'MeM');
	if (g_filelisthead != LIST_ENTRY_ZERO_MEMORY)
	{
		g_filelisthead->NumberOfElements = 0;
		RtlInitUnicodeString(&filepath, L"\\GLOBAL??\\C:\\");
		InitializeObjectAttributes(&oa, &filepath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

		st = IoCreateFile(&FileHandle, FILE_LIST_DIRECTORY | FILE_ANY_ACCESS | SYNCHRONIZE, &oa, &io, 0, FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, FILE_OPEN, FILE_DIRECTORY_FILE |
			FILE_SYNCHRONOUS_IO_ALERT, NULL, 0, CreateFileTypeNone, NULL, IO_NO_PARAMETER_CHECKING, 0, NULL);
		if (NT_SUCCESS(st))
		{
			st = ObReferenceObjectByHandle(FileHandle, FILE_LIST_DIRECTORY | SYNCHRONIZE, *IoFileObjectType, KernelMode, (PVOID*)&FileObject, NULL);
			if (NT_SUCCESS(st))
			{
				DeviceObject = IoGetRelatedDeviceObject(FileObject);
				if (DeviceObject)
				{
					Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
					if (Irp != NULL)
					{
						KeInitializeEvent(&Event, NotificationEvent, FALSE);
						buffer = ExAllocatePoolWithTag(NonPagedPool, BYTES_OF_DIRECTORY, 'JxK');

						Irp->UserEvent = &Event;
						Irp->UserBuffer = buffer;
						Irp->AssociatedIrp.SystemBuffer = buffer;
						Irp->MdlAddress = NULL;
						Irp->Flags = 0;
						Irp->UserIosb = &io;
						Irp->Tail.Overlay.OriginalFileObject = FileObject;
						Irp->Tail.Overlay.Thread = KeGetCurrentThread();
						Irp->RequestorMode = KernelMode;

						pio = IoGetNextIrpStackLocation(Irp);
						pio->DeviceObject = DeviceObject;
						pio->FileObject = FileObject;
						pio->MajorFunction = IRP_MJ_DIRECTORY_CONTROL;
						pio->MinorFunction = IRP_MN_QUERY_DIRECTORY;
						pio->Flags = SL_RESTART_SCAN;
						pio->Context = 0;
						pio->Parameters.QueryDirectory.FileIndex = 0;
						pio->Parameters.QueryDirectory.FileInformationClass = FileBothDirectoryInformation;
						pio->Parameters.QueryDirectory.FileName = NULL;
						pio->Parameters.QueryDirectory.Length = BYTES_OF_DIRECTORY;

						IoSetCompletionRoutine(Irp, QueryCompletion, NULL, TRUE, TRUE, TRUE);
						st = IoCallDriver(DeviceObject, Irp);
						if (st == STATUS_PENDING)
						{
							KeWaitForSingleObject(&Event, Executive, KernelMode, TRUE, NULL);
						}


						dirinfo = (PFILE_BOTH_DIR_INFORMATION)buffer;
						if (dirinfo && MmIsAddressValid(dirinfo))
						{
							InitializeListHead(&g_filelisthead->Entry);

							KeInitializeSpinLock(&g_lock);

							for (;;)
							{
								FileBuffer = (PFILE_BUFFER)ExAllocatePool(NonPagedPool, sizeof(FILE_BUFFER));
								RtlSecureZeroMemory(FileBuffer, sizeof(FILE_BUFFER));

								if ((dirinfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (dirinfo->FileName)[0] == L".")
									goto exit;

								RtlCopyMemory(FileBuffer->FileName, dirinfo->FileName, dirinfo->FileNameLength);

								ExInterlockedInsertHeadList(&g_filelisthead->Entry, &FileBuffer->Entry, &g_lock);

								g_filelisthead->NumberOfElements++;

							exit:
								if (dirinfo->NextEntryOffset == 0) break;


								dirinfo = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)dirinfo + dirinfo->NextEntryOffset);

							}
						}
						ExFreePoolWithTag(buffer, 'JxK');
					}
				}
				ObDereferenceObject(FileObject);
			}
			ZwClose(FileHandle);
		}

	}

	while (!IsListEmpty(&g_filelisthead->Entry))
	{
		alloc = (PFILE_BUFFER)ExInterlockedRemoveHeadList(&g_filelisthead->Entry, &g_lock);
		DbgPrint("\n%ws", alloc->FileName);
		ExFreePool(alloc);
	}

	ExFreePoolWithTag(g_filelisthead, 'MeM');
	g_filelisthead = NULL;

	return STATUS_SUCCESS;
}
