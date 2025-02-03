/*++
    Copyright (c) Microsoft Corporation. All Rights Reserved.
    Copyright (c) Bingxing Wang. All Rights Reserved.
    Copyright (c) LumiaWoA authors. All Rights Reserved.

	Module Name:

		init.c

	Abstract:

		Contains NovaTek initialization code

	Environment:

		Kernel mode

	Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <spb.h>
#include <nt36675\ntinternal.h>
#include <init.tmh>

NTSTATUS
TchStartDevice(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext
)
/*++

  Routine Description:

	This routine is called in response to the KMDF prepare hardware call
	to initialize the touch controller for use.

  Arguments:

	ControllerContext - A pointer to the current touch controller
	context

	SpbContext - A pointer to the current i2c context

  Return Value:

	NTSTATUS indicating success or failure

--*/
{
	NT36X_CONTROLLER_CONTEXT* controller;
	ULONG interruptStatus;
	NTSTATUS status;

	controller = (NT36X_CONTROLLER_CONTEXT*)ControllerContext;
	interruptStatus = 0;
	status = STATUS_SUCCESS;

	//
	// Populate context with NT36X function descriptors
	//
	status = NT36XBuildFunctionsTable(
		ControllerContext,
		SpbContext);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Could not build table of NT36X functions - 0x%08lX",
			status);
		goto exit;
	}

	//
	// Initialize NT36X function control registers
	//
	status = NT36XConfigureFunctions(
		ControllerContext,
		SpbContext);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Could not configure chip - 0x%08lX",
			status);

		goto exit;
	}

	status = NT36XConfigureInterruptEnable(
		ControllerContext,
		SpbContext);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Could not configure interrupt enablement - 0x%08lX",
			status);
		goto exit;
	}

	//
	// Read and store the firmware version
	//
	status = NT36XGetFirmwareVersion(
		ControllerContext,
		SpbContext);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Could not get NT36X firmware version - 0x%08lX",
			status);
		goto exit;
	}

	//
	// Clear any pending interrupts
	//
	status = NT36XCheckInterrupts(
		ControllerContext,
		SpbContext,
		&interruptStatus
	);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Could not get interrupt status - 0x%08lX%",
			status);
	}

exit:
	return status;
}

NTSTATUS
TchStopDevice(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext
)
/*++

Routine Description:

	This routine cleans up the device that is stopped.

Argument:

	ControllerContext - Touch controller context

	SpbContext - A pointer to the current i2c context

Return Value:

	NTSTATUS indicating sucess or failure
--*/
{
	NT36X_CONTROLLER_CONTEXT* controller;

	UNREFERENCED_PARAMETER(SpbContext);

	controller = (NT36X_CONTROLLER_CONTEXT*)ControllerContext;

	return STATUS_SUCCESS;
}

NTSTATUS
TchAllocateContext(
	OUT VOID** ControllerContext,
	IN WDFDEVICE FxDevice
)
/*++

Routine Description:

	This routine allocates a controller context.

Argument:

	ControllerContext - Touch controller context
	FxDevice - Framework device object

Return Value:

	NTSTATUS indicating sucess or failure
--*/
{
	NT36X_CONTROLLER_CONTEXT* context;
	NTSTATUS status;
	
	context = ExAllocatePoolWithTag(
		NonPagedPoolNx,
		sizeof(NT36X_CONTROLLER_CONTEXT),
		TOUCH_POOL_TAG);

	if (NULL == context)
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Could not allocate controller context!");

		status = STATUS_UNSUCCESSFUL;
		goto exit;
	}

	RtlZeroMemory(context, sizeof(NT36X_CONTROLLER_CONTEXT));
	context->FxDevice = FxDevice;

	//
	// Get Touch settings and populate context
	//
	TchGetTouchSettings(&context->TouchSettings);

	//
	// Allocate a WDFWAITLOCK for guarding access to the
	// controller HW and driver controller context
	//
	status = WdfWaitLockCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		&context->ControllerLock);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Could not create lock - 0x%08lX",
			status);

		TchFreeContext(context);
		goto exit;

	}

	*ControllerContext = context;

exit:

	return status;
}

NTSTATUS
TchFreeContext(
	IN VOID* ControllerContext
)
/*++

Routine Description:

	This routine frees a controller context.

Argument:

	ControllerContext - Touch controller context

Return Value:

	NTSTATUS indicating sucess or failure
--*/
{
	NT36X_CONTROLLER_CONTEXT* controller;

	controller = (NT36X_CONTROLLER_CONTEXT*)ControllerContext;

	if (controller != NULL)
	{

		if (controller->ControllerLock != NULL)
		{
			WdfObjectDelete(controller->ControllerLock);
		}

		ExFreePoolWithTag(controller, TOUCH_POOL_TAG);
	}

	return STATUS_SUCCESS;
}