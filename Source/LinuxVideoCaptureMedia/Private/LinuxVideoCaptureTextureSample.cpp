#include "LinuxVideoCaptureTextureSample.h"

FLinuxVideoCaptureTextureSample::FLinuxVideoCaptureTextureSample(
	const void* SourceBuffer,
	uint32 SourceBufferSize,
	FIntPoint InDimensions,
	uint32 InStride,
	FTimespan InTime,
	FTimespan InDuration)
	: Dimensions(InDimensions)
	, Stride(InStride)
	, Time(InTime)
	, Duration(InDuration)
{
	Buffer.SetNumUninitialized(SourceBufferSize);
	FMemory::Memcpy(Buffer.GetData(), SourceBuffer, SourceBufferSize);
}

const void* FLinuxVideoCaptureTextureSample::GetBuffer()
{
	return Buffer.GetData();
}

FIntPoint FLinuxVideoCaptureTextureSample::GetDim() const
{
	return FIntPoint(static_cast<int32>(Stride / 4), Dimensions.Y);
}

FTimespan FLinuxVideoCaptureTextureSample::GetDuration() const
{
	return Duration;
}

EMediaTextureSampleFormat FLinuxVideoCaptureTextureSample::GetFormat() const
{
	return EMediaTextureSampleFormat::CharYUY2;
}

FIntPoint FLinuxVideoCaptureTextureSample::GetOutputDim() const
{
	return Dimensions;
}

uint32 FLinuxVideoCaptureTextureSample::GetStride() const
{
	return Stride;
}

#if WITH_ENGINE
FRHITexture* FLinuxVideoCaptureTextureSample::GetTexture() const
{
	return nullptr;
}
#endif

FMediaTimeStamp FLinuxVideoCaptureTextureSample::GetTime() const
{
	return FMediaTimeStamp(Time);
}

bool FLinuxVideoCaptureTextureSample::IsCacheable() const
{
	return true;
}

bool FLinuxVideoCaptureTextureSample::IsOutputSrgb() const
{
	return true;
}
