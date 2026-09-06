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
    image.fill(QColor(255, 0, 0));  // чистый красный

    source->pushFrame(QVideoFrame(image));

    ASSERT_EQ(sink.frameCount(), 1);
    ASSERT_TRUE(sink.lastFrame().has_value());
    const webrtc::VideoFrame& frame = *sink.lastFrame();
    EXPECT_EQ(frame.width(), 16);
    EXPECT_EQ(frame.height(), 12);

    // Чистый красный в YUV (BT.601, конверсия по умолчанию в libyuv): высокий V,
    // низкий U — этого достаточно, чтобы подтвердить, что реальные цветовые
    // данные прошли через конверсию ARGB->I420, не фиксируя точные значения Y/U/V.
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

TEST(CallVideoTrackSourceTest, SetIsScreencastFlipsTheHintAfterConstruction) {
    // Issue #112: CallManager использует один и тот же источник и для видео с
    // камеры, и для демонстрации экрана, переключая этот флаг при смене
    // активного режима.
    const webrtc::scoped_refptr<CallVideoTrackSource> source =
        webrtc::make_ref_counted<CallVideoTrackSource>(/*isScreencast=*/false);
    ASSERT_FALSE(source->is_screencast());

    source->setIsScreencast(true);
    EXPECT_TRUE(source->is_screencast());

    source->setIsScreencast(false);
    EXPECT_FALSE(source->is_screencast());
}

TEST(CallVideoTrackSourceTest, PushFrameDropsInvalidFrame) {
    const webrtc::scoped_refptr<CallVideoTrackSource> source = webrtc::make_ref_counted<CallVideoTrackSource>();
    webrtc::VideoTrackSourceInterface& sourceInterface = *source;

    FakeVideoSink sink;
    sourceInterface.AddOrUpdateSink(&sink, webrtc::VideoSinkWants{});

    source->pushFrame(QVideoFrame());  // создан по умолчанию: !isValid()

    EXPECT_EQ(sink.frameCount(), 0);
    sourceInterface.RemoveSink(&sink);
}

TEST(CallVideoTrackSourceTest, PushFrameWithNoInterestedSinksIsDropped) {
    const webrtc::scoped_refptr<CallVideoTrackSource> source = webrtc::make_ref_counted<CallVideoTrackSource>();

    QImage image(16, 12, QImage::Format_ARGB32);
    image.fill(QColor(0, 255, 0));

    // AddOrUpdateSink() вообще не вызывается — AdaptFrame() должен сообщить об
    // отсутствии заинтересованных получателей, а pushFrame() должен отбросить
    // кадр вместо того, чтобы конвертировать его впустую.
    source->pushFrame(QVideoFrame(image));

    // Проверять напрямую нечего (нет получателя, который мог бы принять кадр);
    // ценность этого теста в том, что pushFrame() не падает и не бросает
    // исключение, когда AdaptFrame() по этой причине возвращает false.
    SUCCEED();
}

TEST(CallVideoTrackSourceTest, NeedsDenoisingReturnsNullopt) {
    const webrtc::scoped_refptr<CallVideoTrackSource> source = webrtc::make_ref_counted<CallVideoTrackSource>();
    EXPECT_FALSE(source->needs_denoising().has_value());
}

TEST(CallVideoTrackSourceTest, StateIsLive) {
    const webrtc::scoped_refptr<CallVideoTrackSource> source = webrtc::make_ref_counted<CallVideoTrackSource>();
    EXPECT_EQ(source->state(), webrtc::MediaSourceInterface::kLive);
}

TEST(CallVideoTrackSourceTest, RemoteIsFalse) {
    const webrtc::scoped_refptr<CallVideoTrackSource> source = webrtc::make_ref_counted<CallVideoTrackSource>();
    EXPECT_FALSE(source->remote());
}

}  // namespace devicehub
