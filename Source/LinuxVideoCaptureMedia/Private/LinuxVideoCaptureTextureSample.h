#pragma once

#include "CoreMinimal.h"
#include "IMediaTextureSample.h"

class FLinuxVideoCaptureTextureSample final : public IMediaTextureSample
{
public:
	FLinuxVideoCaptureTextureSample(
		const void* SourceBuffer,
		uint32 SourceBufferSize,
		FIntPoint InDimensions,
		FTimespan InTime,
		FTimespan InDuration);

	virtual const void* GetBuffer() override;
	virtual FIntPoint GetDim() const override;
	virtual FTimespan GetDuration() const override;
	virtual EMediaTextureSampleFormat GetFormat() const override;
	virtual FIntPoint GetOutputDim() const override;
	virtual uint32 GetStride() const override;
#if WITH_ENGINE
	virtual FRHITexture* GetTexture() const override;
#endif
	virtual FMediaTimeStamp GetTime() const override;
	virtual bool IsCacheable() const override;
	virtual bool IsOutputSrgb() const override;

private:
	TArray<uint8> Buffer;
	FIntPoint     Dimensions;
	FTimespan     Time;
	FTimespan     Duration;
};

