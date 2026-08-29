#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLinuxVideoCaptureMedia, Log, All);

inline constexpr TCHAR LinuxVideoCaptureMediaUriScheme[] = TEXT("v4l2");

struct FLinuxVideoCaptureFormat
{
	FIntPoint Dimensions              = FIntPoint::ZeroValue;
	uint32    FrameRateNumerator      = 0;
	uint32    FrameRateDenominator    = 1;
	uint32    PixelFormat             = 0;

	float GetFramesPerSecond() const
	{
		return FrameRateDenominator == 0
			? 0.0f
			: static_cast<float>(FrameRateNumerator) / static_cast<float>(FrameRateDenominator);
	}
};

bool EnumerateLinuxVideoCaptureFormats(
	const FString& DevicePath,
	TArray<FLinuxVideoCaptureFormat>& OutFormats,
	FString* OutDeviceName = nullptr,
	FString* OutDeviceInformation = nullptr);
