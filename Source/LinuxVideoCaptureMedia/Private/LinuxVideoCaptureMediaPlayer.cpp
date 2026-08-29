#include "LinuxVideoCaptureMediaPlayer.h"

#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "IMediaEventSink.h"
#include "LinuxVideoCaptureTextureSample.h"
#include "MediaSamples.h"
#include "Misc/ScopeExit.h"

#include <cerrno>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace
{
	constexpr uint32 LinuxVideoCaptureRequiredPixelFormat = V4L2_PIX_FMT_YUYV;
	constexpr uint32 LinuxVideoCaptureRequestedBufferCount = 4;

	int RetryVideoCaptureIoctl(int FileDescriptor, unsigned long Request, void* Argument)
	{
		int Result;
		do
		{
			Result = ::ioctl(FileDescriptor, Request, Argument);
		}
		while (Result == -1 && errno == EINTR);
		return Result;
	}

	bool IsUsableVideoCaptureDevice(const v4l2_capability& Capabilities)
	{
		const uint32 DeviceCapabilities =
			(Capabilities.capabilities & V4L2_CAP_DEVICE_CAPS) != 0
				? Capabilities.device_caps
				: Capabilities.capabilities;

		return (DeviceCapabilities & V4L2_CAP_VIDEO_CAPTURE) != 0 &&
			(DeviceCapabilities & V4L2_CAP_STREAMING) != 0;
	}

	void AddUniqueLinuxVideoCaptureFormat(
		TArray<FLinuxVideoCaptureFormat>& Formats,
		uint32 Width,
		uint32 Height,
		uint32 FramesPerSecond,
		uint32 PixelFormat)
	{
		if (Width == 0 || Height == 0 || FramesPerSecond == 0)
		{
			return;
		}

		const bool AlreadyPresent = Formats.ContainsByPredicate(
			[Width, Height, FramesPerSecond, PixelFormat](const FLinuxVideoCaptureFormat& Existing)
			{
				return Existing.Dimensions.X == static_cast<int32>(Width) &&
					Existing.Dimensions.Y == static_cast<int32>(Height) &&
					Existing.FramesPerSecond == FramesPerSecond &&
					Existing.PixelFormat == PixelFormat;
			});

		if (!AlreadyPresent)
		{
			FLinuxVideoCaptureFormat& Format = Formats.Emplace_GetRef();
			Format.Dimensions      = FIntPoint(static_cast<int32>(Width), static_cast<int32>(Height));
			Format.FramesPerSecond = FramesPerSecond;
			Format.PixelFormat     = PixelFormat;
		}
	}

	void EnumerateDiscreteFrameRates(
		int DeviceFileDescriptor,
		uint32 PixelFormat,
		uint32 Width,
		uint32 Height,
		TArray<FLinuxVideoCaptureFormat>& OutFormats)
	{
		v4l2_frmivalenum FrameInterval = {};
		FrameInterval.pixel_format = PixelFormat;
		FrameInterval.width        = Width;
		FrameInterval.height       = Height;

		for (FrameInterval.index = 0;
			RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_ENUM_FRAMEINTERVALS, &FrameInterval) == 0;
			++FrameInterval.index)
		{
			if (FrameInterval.type == V4L2_FRMIVAL_TYPE_DISCRETE && FrameInterval.discrete.numerator != 0)
			{
				const uint32 FramesPerSecond = FMath::Max(
					1u,
					FrameInterval.discrete.denominator / FrameInterval.discrete.numerator);
				AddUniqueLinuxVideoCaptureFormat(
					OutFormats,
					Width,
					Height,
					FramesPerSecond,
					PixelFormat);
			}
			else if (FrameInterval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS ||
				FrameInterval.type == V4L2_FRMIVAL_TYPE_STEPWISE)
			{
				const uint32 FramesPerSecond = FrameInterval.stepwise.min.numerator == 0
					? 30u
					: FMath::Max(
						1u,
						FrameInterval.stepwise.min.denominator / FrameInterval.stepwise.min.numerator);
				AddUniqueLinuxVideoCaptureFormat(
					OutFormats,
					Width,
					Height,
					FramesPerSecond,
					PixelFormat);
				break;
			}
		}
	}
}

bool EnumerateLinuxVideoCaptureFormats(
	const FString& DevicePath,
	TArray<FLinuxVideoCaptureFormat>& OutFormats,
	FString* OutDeviceName,
	FString* OutDeviceInformation)
{
	OutFormats.Reset();

	const int DeviceFileDescriptor = ::open(TCHAR_TO_UTF8(*DevicePath), O_RDWR | O_NONBLOCK);
	if (DeviceFileDescriptor == -1)
	{
		return false;
	}
	ON_SCOPE_EXIT { ::close(DeviceFileDescriptor); };

	v4l2_capability Capabilities = {};
	if (RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_QUERYCAP, &Capabilities) == -1 ||
		!IsUsableVideoCaptureDevice(Capabilities))
	{
		return false;
	}

	if (OutDeviceName != nullptr)
	{
		*OutDeviceName = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Capabilities.card));
	}
	if (OutDeviceInformation != nullptr)
	{
		*OutDeviceInformation = FString::Printf(
			TEXT("driver=%s, bus=%s, device=%s"),
			UTF8_TO_TCHAR(reinterpret_cast<const char*>(Capabilities.driver)),
			UTF8_TO_TCHAR(reinterpret_cast<const char*>(Capabilities.bus_info)),
			*DevicePath);
	}

	v4l2_fmtdesc PixelFormat = {};
	PixelFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	for (PixelFormat.index = 0;
		RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_ENUM_FMT, &PixelFormat) == 0;
		++PixelFormat.index)
	{
		if (PixelFormat.pixelformat != LinuxVideoCaptureRequiredPixelFormat)
		{
			continue;
		}

		v4l2_frmsizeenum FrameSize = {};
		FrameSize.pixel_format = PixelFormat.pixelformat;
		for (FrameSize.index = 0;
			RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_ENUM_FRAMESIZES, &FrameSize) == 0;
			++FrameSize.index)
		{
			if (FrameSize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
			{
				EnumerateDiscreteFrameRates(
					DeviceFileDescriptor,
					PixelFormat.pixelformat,
					FrameSize.discrete.width,
					FrameSize.discrete.height,
					OutFormats);
			}
			else if (FrameSize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS ||
				FrameSize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
			{
				const uint32 Width  = FMath::Clamp(640u, FrameSize.stepwise.min_width, FrameSize.stepwise.max_width);
				const uint32 Height = FMath::Clamp(480u, FrameSize.stepwise.min_height, FrameSize.stepwise.max_height);
				EnumerateDiscreteFrameRates(
					DeviceFileDescriptor,
					PixelFormat.pixelformat,
					Width,
					Height,
					OutFormats);
				break;
			}
		}
	}

	OutFormats.Sort([](const FLinuxVideoCaptureFormat& Left, const FLinuxVideoCaptureFormat& Right)
	{
		const int64 LeftPixels  = static_cast<int64>(Left.Dimensions.X) * Left.Dimensions.Y;
		const int64 RightPixels = static_cast<int64>(Right.Dimensions.X) * Right.Dimensions.Y;
		return LeftPixels == RightPixels
			? Left.FramesPerSecond > Right.FramesPerSecond
			: LeftPixels < RightPixels;
	});

	return !OutFormats.IsEmpty();
}

FLinuxVideoCaptureMediaPlayer::FLinuxVideoCaptureMediaPlayer(IMediaEventSink& InEventSink)
	: EventSink(InEventSink)
	, Samples(MakeShared<FMediaSamples, ESPMode::ThreadSafe>())
{
}

FLinuxVideoCaptureMediaPlayer::~FLinuxVideoCaptureMediaPlayer()
{
	Close();
}

void FLinuxVideoCaptureMediaPlayer::Close()
{
	const bool WasOpen = CurrentState.Load() != EMediaState::Closed;
	StopCapturingSelectedVideoFormat();
	Samples->FlushSamples();
	VideoFormats.Reset();
	SelectedVideoFormat = INDEX_NONE;
	MediaUrl.Reset();
	DevicePath.Reset();
	DeviceName.Reset();
	CurrentTimeTicks = 0;
	CurrentState = EMediaState::Closed;

	if (WasOpen)
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
	}
}

IMediaCache& FLinuxVideoCaptureMediaPlayer::GetCache() { return *this; }
IMediaControls& FLinuxVideoCaptureMediaPlayer::GetControls() { return *this; }
IMediaSamples& FLinuxVideoCaptureMediaPlayer::GetSamples() { return *Samples; }
IMediaTracks& FLinuxVideoCaptureMediaPlayer::GetTracks() { return *this; }
IMediaView& FLinuxVideoCaptureMediaPlayer::GetView() { return *this; }

FString FLinuxVideoCaptureMediaPlayer::GetInfo() const
{
	return FString::Printf(TEXT("V4L2 capture device: %s (%s)"), *DeviceName, *DevicePath);
}

FGuid FLinuxVideoCaptureMediaPlayer::GetPlayerPluginGUID() const
{
	return FGuid(0x5d83f018, 0x9b3f47ab, 0xb4be7ea7, 0xcd727c81);
}

FString FLinuxVideoCaptureMediaPlayer::GetStats() const
{
	return FString::Printf(
		TEXT("Captured frames: %llu\nDropped frames: %llu"),
		CapturedFrameCount.Load(),
		DroppedFrameCount.Load());
}

FString FLinuxVideoCaptureMediaPlayer::GetUrl() const
{
	return MediaUrl;
}

bool FLinuxVideoCaptureMediaPlayer::Open(const FString& Url, const IMediaOptions*)
{
	Close();

	FString Scheme;
	if (!Url.Split(TEXT("://"), &Scheme, &DevicePath, ESearchCase::CaseSensitive) ||
		Scheme != LinuxVideoCaptureMediaUriScheme ||
		!DevicePath.StartsWith(TEXT("/dev/video")))
	{
		return false;
	}

	FString DeviceInformation;
	if (!EnumerateLinuxVideoCaptureFormats(DevicePath, VideoFormats, &DeviceName, &DeviceInformation))
	{
		UE_LOG(LogLinuxVideoCaptureMedia, Error, TEXT("No supported YUYV format on %s"), *DevicePath);
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		return false;
	}

	MediaUrl = Url;
	SelectedVideoFormat = ChooseDefaultVideoFormat();
	CurrentState = EMediaState::Preparing;
	if (!StartCapturingSelectedVideoFormat())
	{
		CurrentState = EMediaState::Error;
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		return false;
	}

	CurrentState = EMediaState::Playing;
	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
	EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
	return true;
}

bool FLinuxVideoCaptureMediaPlayer::Open(
	const TSharedRef<FArchive, ESPMode::ThreadSafe>&,
	const FString&,
	const IMediaOptions*)
{
	return false;
}

bool FLinuxVideoCaptureMediaPlayer::CanControl(EMediaControl Control) const
{
	return Control == EMediaControl::Pause || Control == EMediaControl::Resume;
}

FTimespan FLinuxVideoCaptureMediaPlayer::GetDuration() const { return FTimespan::MaxValue(); }
float FLinuxVideoCaptureMediaPlayer::GetRate() const { return CurrentState.Load() == EMediaState::Playing ? 1.0f : 0.0f; }
EMediaState FLinuxVideoCaptureMediaPlayer::GetState() const { return CurrentState.Load(); }
EMediaStatus FLinuxVideoCaptureMediaPlayer::GetStatus() const { return EMediaStatus::None; }
FTimespan FLinuxVideoCaptureMediaPlayer::GetTime() const { return FTimespan(CurrentTimeTicks.Load()); }
bool FLinuxVideoCaptureMediaPlayer::IsLooping() const { return false; }
bool FLinuxVideoCaptureMediaPlayer::Seek(const FTimespan&) { return false; }
bool FLinuxVideoCaptureMediaPlayer::SetLooping(bool) { return false; }

TRangeSet<float> FLinuxVideoCaptureMediaPlayer::GetSupportedRates(EMediaRateThinning) const
{
	TRangeSet<float> Rates;
	Rates.Add(TRange<float>(0.0f));
	Rates.Add(TRange<float>(1.0f));
	return Rates;
}

bool FLinuxVideoCaptureMediaPlayer::SetRate(float Rate)
{
	if (Rate == 1.0f && CurrentState.Load() == EMediaState::Paused)
	{
		CurrentState = EMediaState::Playing;
		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
		return true;
	}
	if (Rate == 0.0f && CurrentState.Load() == EMediaState::Playing)
	{
		CurrentState = EMediaState::Paused;
		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		return true;
	}
	return false;
}

bool FLinuxVideoCaptureMediaPlayer::GetAudioTrackFormat(int32, int32, FMediaAudioTrackFormat&) const
{
	return false;
}

int32 FLinuxVideoCaptureMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	return TrackType == EMediaTrackType::Video && !VideoFormats.IsEmpty() ? 1 : 0;
}

int32 FLinuxVideoCaptureMediaPlayer::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return TrackType == EMediaTrackType::Video && TrackIndex == 0 ? VideoFormats.Num() : 0;
}

int32 FLinuxVideoCaptureMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	return TrackType == EMediaTrackType::Video && SelectedVideoFormat != INDEX_NONE ? 0 : INDEX_NONE;
}

FText FLinuxVideoCaptureMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return TrackType == EMediaTrackType::Video && TrackIndex == 0
		? FText::FromString(DeviceName)
		: FText::GetEmpty();
}

int32 FLinuxVideoCaptureMediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return TrackType == EMediaTrackType::Video && TrackIndex == 0 ? SelectedVideoFormat : INDEX_NONE;
}

FString FLinuxVideoCaptureMediaPlayer::GetTrackLanguage(EMediaTrackType, int32) const { return TEXT("und"); }
FString FLinuxVideoCaptureMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return TrackType == EMediaTrackType::Video && TrackIndex == 0 ? DeviceName : FString();
}

bool FLinuxVideoCaptureMediaPlayer::GetVideoTrackFormat(
	int32 TrackIndex,
	int32 FormatIndex,
	FMediaVideoTrackFormat& OutFormat) const
{
	if (TrackIndex != 0 || !VideoFormats.IsValidIndex(FormatIndex))
	{
		return false;
	}

	const FLinuxVideoCaptureFormat& Format = VideoFormats[FormatIndex];
	OutFormat.Dim       = Format.Dimensions;
	OutFormat.FrameRate = static_cast<float>(Format.FramesPerSecond);
	OutFormat.FrameRates = TRange<float>(OutFormat.FrameRate);
	OutFormat.TypeName  = TEXT("YUYV");
	return true;
}

bool FLinuxVideoCaptureMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	return TrackType == EMediaTrackType::Video && (TrackIndex == 0 || TrackIndex == INDEX_NONE);
}

bool FLinuxVideoCaptureMediaPlayer::SetTrackFormat(
	EMediaTrackType TrackType,
	int32 TrackIndex,
	int32 FormatIndex)
{
	if (TrackType != EMediaTrackType::Video || TrackIndex != 0 || !VideoFormats.IsValidIndex(FormatIndex))
	{
		return false;
	}
	if (SelectedVideoFormat == FormatIndex)
	{
		return true;
	}

	StopCapturingSelectedVideoFormat();
	Samples->FlushSamples();
	SelectedVideoFormat = FormatIndex;
	return StartCapturingSelectedVideoFormat();
}

bool FLinuxVideoCaptureMediaPlayer::SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate)
{
	return TrackIndex == 0 && VideoFormats.IsValidIndex(FormatIndex) &&
		FMath::IsNearlyEqual(static_cast<float>(VideoFormats[FormatIndex].FramesPerSecond), FrameRate);
}

int32 FLinuxVideoCaptureMediaPlayer::ChooseDefaultVideoFormat() const
{
	const int32 Preferred = VideoFormats.IndexOfByPredicate([](const FLinuxVideoCaptureFormat& Format)
	{
		return Format.Dimensions == FIntPoint(640, 480) && Format.FramesPerSecond == 30;
	});
	return Preferred != INDEX_NONE ? Preferred : 0;
}

bool FLinuxVideoCaptureMediaPlayer::StartCapturingSelectedVideoFormat()
{
	if (!VideoFormats.IsValidIndex(SelectedVideoFormat))
	{
		return false;
	}

	const FLinuxVideoCaptureFormat& Format = VideoFormats[SelectedVideoFormat];
	DeviceFileDescriptor = ::open(TCHAR_TO_UTF8(*DevicePath), O_RDWR | O_NONBLOCK);
	if (DeviceFileDescriptor == -1)
	{
		UE_LOG(LogLinuxVideoCaptureMedia, Error, TEXT("Failed to open %s: errno %d"), *DevicePath, errno);
		return false;
	}

	v4l2_format DeviceFormat = {};
	DeviceFormat.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	DeviceFormat.fmt.pix.width       = static_cast<uint32>(Format.Dimensions.X);
	DeviceFormat.fmt.pix.height      = static_cast<uint32>(Format.Dimensions.Y);
	DeviceFormat.fmt.pix.pixelformat = Format.PixelFormat;
	DeviceFormat.fmt.pix.field       = V4L2_FIELD_ANY;
	if (RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_S_FMT, &DeviceFormat) == -1 ||
		DeviceFormat.fmt.pix.pixelformat != LinuxVideoCaptureRequiredPixelFormat)
	{
		UE_LOG(LogLinuxVideoCaptureMedia, Error, TEXT("Failed to select YUYV on %s"), *DevicePath);
		StopCapturingSelectedVideoFormat();
		return false;
	}

	v4l2_streamparm StreamParameters = {};
	StreamParameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	StreamParameters.parm.capture.timeperframe.numerator   = 1;
	StreamParameters.parm.capture.timeperframe.denominator = Format.FramesPerSecond;
	RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_S_PARM, &StreamParameters);

	v4l2_requestbuffers RequestedBuffers = {};
	RequestedBuffers.count  = LinuxVideoCaptureRequestedBufferCount;
	RequestedBuffers.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	RequestedBuffers.memory = V4L2_MEMORY_MMAP;
	if (RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_REQBUFS, &RequestedBuffers) == -1 ||
		RequestedBuffers.count < 2)
	{
		UE_LOG(LogLinuxVideoCaptureMedia, Error, TEXT("Failed to allocate V4L2 buffers on %s"), *DevicePath);
		StopCapturingSelectedVideoFormat();
		return false;
	}

	MappedBuffers.Reserve(RequestedBuffers.count);
	for (uint32 BufferIndex = 0; BufferIndex < RequestedBuffers.count; ++BufferIndex)
	{
		v4l2_buffer BufferDescription = {};
		BufferDescription.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		BufferDescription.memory = V4L2_MEMORY_MMAP;
		BufferDescription.index  = BufferIndex;
		if (RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_QUERYBUF, &BufferDescription) == -1)
		{
			StopCapturingSelectedVideoFormat();
			return false;
		}

		FMappedVideoCaptureBuffer& MappedBuffer = MappedBuffers.Emplace_GetRef();
		MappedBuffer.Length = BufferDescription.length;
		MappedBuffer.Address = ::mmap(
			nullptr,
			BufferDescription.length,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			DeviceFileDescriptor,
			BufferDescription.m.offset);
		if (MappedBuffer.Address == MAP_FAILED)
		{
			MappedBuffer.Address = nullptr;
			StopCapturingSelectedVideoFormat();
			return false;
		}

		if (RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_QBUF, &BufferDescription) == -1)
		{
			StopCapturingSelectedVideoFormat();
			return false;
		}
	}

	v4l2_buf_type BufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_STREAMON, &BufferType) == -1)
	{
		StopCapturingSelectedVideoFormat();
		return false;
	}

	CapturedFrameCount = 0;
	DroppedFrameCount = 0;
	StopCaptureThread = false;
	CaptureThread = FRunnableThread::Create(this, TEXT("Linux V4L2 video capture"));
	if (CaptureThread == nullptr)
	{
		StopCapturingSelectedVideoFormat();
		return false;
	}
	return true;
}

void FLinuxVideoCaptureMediaPlayer::StopCapturingSelectedVideoFormat()
{
	StopCaptureThread = true;
	if (CaptureThread != nullptr)
	{
		CaptureThread->WaitForCompletion();
		delete CaptureThread;
		CaptureThread = nullptr;
	}

	if (DeviceFileDescriptor != -1)
	{
		v4l2_buf_type BufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_STREAMOFF, &BufferType);
	}
	for (const FMappedVideoCaptureBuffer& Buffer : MappedBuffers)
	{
		if (Buffer.Address != nullptr)
		{
			::munmap(Buffer.Address, Buffer.Length);
		}
	}
	MappedBuffers.Reset();

	if (DeviceFileDescriptor != -1)
	{
		::close(DeviceFileDescriptor);
		DeviceFileDescriptor = -1;
	}
}

uint32 FLinuxVideoCaptureMediaPlayer::Run()
{
	const double CaptureStartTime = FPlatformTime::Seconds();
	const FLinuxVideoCaptureFormat Format = VideoFormats[SelectedVideoFormat];
	const FTimespan FrameDuration = FTimespan::FromSeconds(1.0 / Format.FramesPerSecond);

	while (!StopCaptureThread.Load())
	{
		pollfd PollDescriptor = {};
		PollDescriptor.fd     = DeviceFileDescriptor;
		PollDescriptor.events = POLLIN;
		const int PollResult = ::poll(&PollDescriptor, 1, 100);
		if (PollResult <= 0)
		{
			continue;
		}

		v4l2_buffer CapturedBuffer = {};
		CapturedBuffer.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		CapturedBuffer.memory = V4L2_MEMORY_MMAP;
		if (RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_DQBUF, &CapturedBuffer) == -1)
		{
			if (errno != EAGAIN)
			{
				UE_LOG(LogLinuxVideoCaptureMedia, Warning, TEXT("V4L2 dequeue failed on %s: errno %d"), *DevicePath, errno);
			}
			continue;
		}

		if (MappedBuffers.IsValidIndex(static_cast<int32>(CapturedBuffer.index)) &&
			CurrentState.Load() == EMediaState::Playing)
		{
			const uint32 ExpectedBufferSize = static_cast<uint32>(Format.Dimensions.X * Format.Dimensions.Y * 2);
			const uint32 CapturedBufferSize = FMath::Min(ExpectedBufferSize, CapturedBuffer.bytesused);
			if (CapturedBufferSize == ExpectedBufferSize && Samples->CanReceiveVideoSamples(1))
			{
				const FTimespan SampleTime = FTimespan::FromSeconds(FPlatformTime::Seconds() - CaptureStartTime);
				Samples->AddVideo(MakeShared<FLinuxVideoCaptureTextureSample, ESPMode::ThreadSafe>(
					MappedBuffers[CapturedBuffer.index].Address,
					CapturedBufferSize,
					Format.Dimensions,
					SampleTime,
					FrameDuration));
				CurrentTimeTicks = SampleTime.GetTicks();
				++CapturedFrameCount;
			}
			else
			{
				++DroppedFrameCount;
			}
		}

		if (RetryVideoCaptureIoctl(DeviceFileDescriptor, VIDIOC_QBUF, &CapturedBuffer) == -1)
		{
			UE_LOG(LogLinuxVideoCaptureMedia, Warning, TEXT("V4L2 requeue failed on %s: errno %d"), *DevicePath, errno);
			break;
		}
	}
	return 0;
}

void FLinuxVideoCaptureMediaPlayer::Stop()
{
	StopCaptureThread = true;
}
