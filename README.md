# Linux Video Capture Media

Linux V4L2 webcam capture for Unreal Engine 5.8 Media Framework.

The plugin registers Linux webcams as Unreal capture devices and supplies the
Media Framework player required to open them. Capture devices use URLs such as:

```text
v4l2:///dev/video0
```

The first implementation supports uncompressed V4L2 `YUYV` input. Frames are
passed to Unreal as `CharYUY2` media texture samples without a codec dependency.

## Install in a project

Clone the repository into the project's `Plugins` directory:

```sh
git clone https://github.com/overyonder/unreal-linux-v4l2-media.git \
  Plugins/LinuxVideoCaptureMedia
```

Enable **Linux Video Capture Media** in Unreal's Plugins window and restart the
editor. The camera should then appear under capture devices in Media Player and
in MetaHuman Video Live Link source settings.

The Unreal process needs read and write permission for the corresponding
`/dev/videoN` device.

## Current capture support

- Linux only
- V4L2 streaming capture devices
- YUYV/YUY2 formats
- Resolution and frame-rate enumeration
- Runtime format selection
- Pause and resume
- Video only; audio capture is outside this plugin's scope

Compressed MJPEG and H.264 camera modes are not yet decoded. Many USB webcams
offer YUYV at 640×480 and compressed formats at higher resolutions.

## License

MIT

