#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLinuxVideoCaptureMedia, Log, All);

inline constexpr TCHAR LinuxVideoCaptureMediaUriScheme[] = TEXT("v4l2");

struct FLinuxVideoCaptureFormat
{
	FIntPoint Dimensions       = FIntPoint::ZeroValue;
	uint32    FramesPerSecond  = 0;
	uint32    PixelFormat      = 0;
};

bool EnumerateLinuxVideoCaptureFormats(
	const FString& DevicePath,
	TArray<FLinuxVideoCaptureFormat>& OutFormats,
	FString* OutDeviceName = nullptr,
	FString* OutDeviceInformation = nullptr);

