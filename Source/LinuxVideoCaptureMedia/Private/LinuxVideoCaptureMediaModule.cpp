#include "LinuxVideoCaptureMediaPrivate.h"

#include "IMediaCaptureSupport.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"
#include "LinuxVideoCaptureMediaPlayer.h"
#include "Modules/ModuleManager.h"

#include <unistd.h>

DEFINE_LOG_CATEGORY(LogLinuxVideoCaptureMedia);

#define LOCTEXT_NAMESPACE "FLinuxVideoCaptureMediaModule"

class FLinuxVideoCaptureMediaModule final
	: public IModuleInterface
	, public IMediaCaptureSupport
	, public IMediaPlayerFactory
{
public:
	virtual void StartupModule() override
	{
		SupportedPlatforms.Add(TEXT("Linux"));

		if (IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>(TEXT("Media")))
		{
			MediaModule->RegisterCaptureSupport(*this);
			MediaModule->RegisterPlayerFactory(*this);
			bRegisteredWithMediaModule = true;
		}
	}

	virtual void ShutdownModule() override
	{
		if (!bRegisteredWithMediaModule)
		{
			return;
		}

		if (IMediaModule* MediaModule = FModuleManager::GetModulePtr<IMediaModule>(TEXT("Media")))
		{
			MediaModule->UnregisterPlayerFactory(*this);
			MediaModule->UnregisterCaptureSupport(*this);
		}
		bRegisteredWithMediaModule = false;
	}

	virtual void EnumerateAudioCaptureDevices(TArray<FMediaCaptureDeviceInfo>&) override
	{
	}

	virtual void EnumerateVideoCaptureDevices(TArray<FMediaCaptureDeviceInfo>& OutDeviceInfos) override
	{
		for (int32 DeviceIndex = 0; DeviceIndex < 64; ++DeviceIndex)
		{
			const FString DevicePath = FString::Printf(TEXT("/dev/video%d"), DeviceIndex);
			if (::access(TCHAR_TO_UTF8(*DevicePath), R_OK | W_OK) != 0)
			{
				continue;
			}

			TArray<FLinuxVideoCaptureFormat> Formats;
			FString DeviceName;
			FString DeviceInformation;
			if (!EnumerateLinuxVideoCaptureFormats(
				DevicePath,
				Formats,
				&DeviceName,
				&DeviceInformation))
			{
				continue;
			}

			FMediaCaptureDeviceInfo& Device = OutDeviceInfos.Emplace_GetRef();
			Device.DisplayName = FText::FromString(DeviceName.IsEmpty() ? DevicePath : DeviceName);
			Device.Info        = MoveTemp(DeviceInformation);
			Device.Type        = EMediaCaptureDeviceType::Webcam;
			Device.Url         = FString::Printf(TEXT("%s://%s"), LinuxVideoCaptureMediaUriScheme, *DevicePath);
		}
	}

	virtual bool CanPlayUrl(
		const FString& Url,
		const IMediaOptions*,
		TArray<FText>*,
		TArray<FText>* OutErrors) const override
	{
		const bool CanPlay = Url.StartsWith(
			FString::Printf(TEXT("%s:///dev/video"), LinuxVideoCaptureMediaUriScheme),
			ESearchCase::CaseSensitive);
		if (!CanPlay && OutErrors != nullptr)
		{
			OutErrors->Add(LOCTEXT("UnsupportedUrl", "Linux Video Capture Media requires a v4l2:///dev/videoN URL."));
		}
		return CanPlay;
	}

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		return MakeShared<FLinuxVideoCaptureMediaPlayer, ESPMode::ThreadSafe>(EventSink);
	}

	virtual FText GetDisplayName() const override
	{
		return LOCTEXT("MediaPlayerDisplayName", "Linux V4L2 Video Capture");
	}

	virtual FName GetPlayerName() const override
	{
		static const FName PlayerName(TEXT("LinuxVideoCaptureMedia"));
		return PlayerName;
	}

	virtual FGuid GetPlayerPluginGUID() const override
	{
		return FGuid(0x5d83f018, 0x9b3f47ab, 0xb4be7ea7, 0xcd727c81);
	}

	virtual const TArray<FString>& GetSupportedPlatforms() const override
	{
		return SupportedPlatforms;
	}

	virtual bool SupportsFeature(EMediaFeature Feature) const override
	{
		return Feature == EMediaFeature::VideoSamples || Feature == EMediaFeature::VideoTracks;
	}

private:
	TArray<FString> SupportedPlatforms;
	bool bRegisteredWithMediaModule = false;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLinuxVideoCaptureMediaModule, LinuxVideoCaptureMedia)

