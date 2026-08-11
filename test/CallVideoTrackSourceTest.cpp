#include "devices/CallVideoTrackSource.h"

#include <api/make_ref_counted.h>
#include <api/video/video_frame.h>
#include <api/video/video_frame_buffer.h>
#include <api/video/video_sink_interface.h>
#include <api/video/video_source_interface.h>

#include <gtest/gtest.h>

#include <QColor>
#include <QImage>
#include <QVideoFrame>

#include <optional>

namespace devicehub {
namespace {

class FakeVideoSink : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    void OnFrame(const webrtc::VideoFrame& frame) override {
        lastFrame_ = frame;
        ++frameCount_;
    }

    [[nodiscard]] int frameCount() const { return frameCount_; }
    [[nodiscard]] const std::optional<webrtc::VideoFrame>& lastFrame() const { return lastFrame_; }

private:
    int frameCount_ = 0;
    std::optional<webrtc::VideoFrame> lastFrame_;
};

}  // namespace

TEST(CallVideoTrackSourceTest, ConvertsSolidColorFrameToCorrectSizeAndApproximateColor) {
    const webrtc::scoped_refptr<CallVideoTrackSource> source = webrtc::make_ref_counted<CallVideoTrackSource>();
    webrtc::VideoTrackSourceInterface& sourceInterface = *source;

    FakeVideoSink sink;
    sourceInterface.AddOrUpdateSink(&sink, webrtc::VideoSinkWants{});

    QImage image(16, 12, QImage::Format_ARGB32);
    image.fill(QColor(255, 0, 0));  // pure red

    source->pushFrame(QVideoFrame(image));

    ASSERT_EQ(sink.frameCount(), 1);
    ASSERT_TRUE(sink.lastFrame().has_value());
    const webrtc::VideoFrame& frame = *sink.lastFrame();
    EXPECT_EQ(frame.width(), 16);
    EXPECT_EQ(frame.height(), 12);

    // Pure red in YUV (BT.601, libyuv's default conversion): high V,
    // low U — enough to confirm real color data made it through the
    // ARGB->I420 conversion, without pinning down exact Y/U/V values.
    const webrtc::scoped_refptr<webrtc::I420BufferInterface> buffer = frame.video_frame_buffer()->ToI420();
    EXPECT_LT(buffer->DataU()[0], 128);
    EXPECT_GT(buffer->DataV()[0], 128);

    sourceInterface.RemoveSink(&sink);
}

TEST(CallVideoTrackSourceTest, IsScreencastReflectsConstructorArgument) {
    const webrtc::scoped_refptr<CallVideoTrackSource> cameraSource =
        webrtc::make_ref_counted<CallVideoTrackSource>(/*isScreencast=*/false);
    const webrtc::scoped_refptr<CallVideoTrackSource> screenSource =
        webrtc::make_ref_counted<CallVideoTrackSource>(/*isScreencast=*/true);
    EXPECT_FALSE(cameraSource->is_screencast());
    EXPECT_TRUE(screenSource->is_screencast());
}

}  // namespace devicehub
