#include "LinuxVideoCaptureTextureSample.h"

FLinuxVideoCaptureTextureSample::FLinuxVideoCaptureTextureSample(
	const void* SourceBuffer,
	uint32 SourceBufferSize,
	FIntPoint InDimensions,
	FTimespan InTime,
	FTimespan InDuration)
	: Dimensions(InDimensions)
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
	return FIntPoint(Dimensions.X / 2, Dimensions.Y);
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
	return static_cast<uint32>(Dimensions.X * 2);
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
