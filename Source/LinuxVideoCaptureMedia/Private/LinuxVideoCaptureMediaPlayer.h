#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "LinuxVideoCaptureMediaPrivate.h"

class FMediaSamples;
class IMediaEventSink;

class FLinuxVideoCaptureMediaPlayer final
	: public IMediaPlayer
	, protected IMediaCache
	, protected IMediaControls
	, protected IMediaTracks
	, protected IMediaView
	, private FRunnable
{
public:
	explicit FLinuxVideoCaptureMediaPlayer(IMediaEventSink& InEventSink);
	virtual ~FLinuxVideoCaptureMediaPlayer() override;

	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FGuid GetPlayerPluginGUID() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(
		const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive,
		const FString& OriginalUrl,
		const IMediaOptions* Options) override;

protected:
	virtual bool CanControl(EMediaControl Control) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;

	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;
	virtual bool SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate) override;

private:
	struct FMappedVideoCaptureBuffer
	{
		void*  Address = nullptr;
		size_t Length  = 0;
	};

	virtual uint32 Run() override;
	virtual void Stop() override;

	bool StartCapturingSelectedVideoFormat();
	void StopCapturingSelectedVideoFormat();
	int32 ChooseDefaultVideoFormat() const;

	IMediaEventSink& EventSink;
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;
	TArray<FLinuxVideoCaptureFormat> VideoFormats;
	TArray<FMappedVideoCaptureBuffer> MappedBuffers;
	FRunnableThread* CaptureThread = nullptr;

	FString MediaUrl;
	FString DevicePath;
	FString DeviceName;
	int32 DeviceFileDescriptor = -1;
	int32 SelectedVideoFormat = INDEX_NONE;
	TAtomic<EMediaState> CurrentState = EMediaState::Closed;
	TAtomic<bool> StopCaptureThread = false;
	TAtomic<int64> CurrentTimeTicks = 0;
	TAtomic<uint64> CapturedFrameCount = 0;
	TAtomic<uint64> DroppedFrameCount = 0;
};

