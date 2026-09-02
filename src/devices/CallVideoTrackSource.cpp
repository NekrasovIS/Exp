#include "devices/CallVideoTrackSource.h"

#include <api/video/i420_buffer.h>
#include <api/video/video_frame.h>
#include <api/video/video_rotation.h>
#include <libyuv/convert_from_argb.h>
#include <rtc_base/time_utils.h>

#include <QImage>

namespace devicehub {

CallVideoTrackSource::CallVideoTrackSource(bool isScreencast) : isScreencast_(isScreencast) {}

void CallVideoTrackSource::pushFrame(const QVideoFrame& frame) {
    if (!frame.isValid()) {
        return;
    }

    const QImage image = frame.toImage().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    const int64_t timestampUs = webrtc::TimeMicros();
    int adaptedWidth = 0;
    int adaptedHeight = 0;
    int cropWidth = 0;
    int cropHeight = 0;
    int cropX = 0;
    int cropY = 0;
    if (!AdaptFrame(image.width(), image.height(), timestampUs, &adaptedWidth, &adaptedHeight, &cropWidth,
                     &cropHeight, &cropX, &cropY)) {
        // Нет заинтересованных sink'ов, либо адаптер хочет отбросить
        // именно этот кадр (например, чтобы удержать целевой framerate).
        return;
    }

    const webrtc::scoped_refptr<webrtc::I420Buffer> croppedBuffer =
        webrtc::I420Buffer::Create(cropWidth, cropHeight);
    const uint8_t* cropOrigin = image.constBits() +
                                 (static_cast<size_t>(cropY) * static_cast<size_t>(image.bytesPerLine())) +
                                 (static_cast<size_t>(cropX) * 4);
    libyuv::ARGBToI420(cropOrigin, static_cast<int>(image.bytesPerLine()), croppedBuffer->MutableDataY(),
                        croppedBuffer->StrideY(), croppedBuffer->MutableDataU(), croppedBuffer->StrideU(),
                        croppedBuffer->MutableDataV(), croppedBuffer->StrideV(), cropWidth, cropHeight);

    webrtc::scoped_refptr<webrtc::I420Buffer> outputBuffer = croppedBuffer;
    if (adaptedWidth != cropWidth || adaptedHeight != cropHeight) {
        outputBuffer = webrtc::I420Buffer::Create(adaptedWidth, adaptedHeight);
        outputBuffer->ScaleFrom(*croppedBuffer);
    }

    const webrtc::VideoFrame webrtcFrame = webrtc::VideoFrame::Builder()
                                                .set_video_frame_buffer(outputBuffer)
                                                .set_timestamp_us(timestampUs)
                                                .set_rotation(webrtc::kVideoRotation_0)
                                                .build();
    OnFrame(webrtcFrame);
}

void CallVideoTrackSource::setIsScreencast(bool isScreencast) {
    isScreencast_ = isScreencast;
}

bool CallVideoTrackSource::is_screencast() const {
    return isScreencast_;
}

std::optional<bool> CallVideoTrackSource::needs_denoising() const {
    return std::nullopt;
}

CallVideoTrackSource::SourceState CallVideoTrackSource::state() const {
    return kLive;
}

bool CallVideoTrackSource::remote() const {
    return false;
}

}  // namespace devicehub
