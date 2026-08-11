#pragma once

#include <media/base/adapted_video_track_source.h>

#include <QVideoFrame>

#include <optional>

namespace devicehub {

/**
 * @brief Bridges Qt camera/screen-capture frames into libwebrtc's video
 *        pipeline — the video-side counterpart to CallAudioDeviceModule
 *        (issue #64): Qt Multimedia has no built-in way to push frames
 *        into a webrtc::VideoTrackSourceInterface, so this class exists
 *        to be that bridge.
 *
 * pushFrame() converts each QVideoFrame to I420 via QImage as an
 * intermediate step (QVideoFrame::toImage(), then
 * libyuv::ARGBToI420()) rather than hand-parsing every possible native
 * camera pixel format directly — simpler and more robust at the cost
 * of an extra conversion pass, matching the "first pass, not yet
 * tuned for peak efficiency" scope of CallAudioDeviceModule. Feeds the
 * result to AdaptedVideoTrackSource::OnFrame(), which — via
 * AdaptFrame() — also handles per-sink resolution adaptation (e.g. a
 * receiver asking for a smaller frame under bandwidth pressure) and
 * all sink registration/management.
 */
class CallVideoTrackSource : public webrtc::AdaptedVideoTrackSource {
public:
    /// @p isScreencast should be true for a screen-share source — see
    /// is_screencast()'s contract in webrtc::VideoTrackSourceInterface
    /// (lets receivers apply screencast-appropriate defaults).
    explicit CallVideoTrackSource(bool isScreencast = false);

    /// Converts and pushes a captured frame into the pipeline. Safe to
    /// call from any thread (matches AdaptedVideoTrackSource's own
    /// OnFrame()/AdaptFrame() contract); a no-op if the frame is
    /// invalid or there are no interested sinks.
    void pushFrame(const QVideoFrame& frame);

    bool is_screencast() const override;
    std::optional<bool> needs_denoising() const override;
    SourceState state() const override;
    bool remote() const override;

private:
    bool isScreencast_;
};

}  // namespace devicehub
