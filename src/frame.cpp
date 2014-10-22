#include "frame.h"

using namespace std;

extern "C" {
#include <libavutil/version.h>
#include <libavutil/imgutils.h>
#include <libavutil/avassert.h>
}


namespace {

#if LIBAVUTIL_VERSION_INT <= AV_VERSION_INT(52,48,101)

#define CHECK_CHANNELS_CONSISTENCY(frame) \
    av_assert2(!(frame)->channel_layout || \
               (frame)->channels == \
               av_get_channel_layout_nb_channels((frame)->channel_layout))

int frame_copy_video(AVFrame *dst, const AVFrame *src)
{
    const uint8_t *src_data[4];
    int i, planes;

    if (dst->width  < src->width ||
        dst->height < src->height)
        return AVERROR(EINVAL);

    planes = av_pix_fmt_count_planes(static_cast<AVPixelFormat>(dst->format));
    for (i = 0; i < planes; i++)
        if (!dst->data[i] || !src->data[i])
            return AVERROR(EINVAL);

    memcpy(src_data, src->data, sizeof(src_data));
    av_image_copy(dst->data, dst->linesize,
                  src_data, src->linesize,
                  static_cast<AVPixelFormat>(dst->format), src->width, src->height);

    return 0;
}

int frame_copy_audio(AVFrame *dst, const AVFrame *src)
{
    int planar   = av_sample_fmt_is_planar(static_cast<AVSampleFormat>(dst->format));
    int channels = dst->channels;
    int planes   = planar ? channels : 1;
    int i;

    if (dst->nb_samples     != src->nb_samples ||
        dst->channels       != src->channels ||
        dst->channel_layout != src->channel_layout)
        return AVERROR(EINVAL);

    CHECK_CHANNELS_CONSISTENCY(src);

    for (i = 0; i < planes; i++)
        if (!dst->extended_data[i] || !src->extended_data[i])
            return AVERROR(EINVAL);

    av_samples_copy(dst->extended_data, src->extended_data, 0, 0,
                    dst->nb_samples, channels, static_cast<AVSampleFormat>(dst->format));

    return 0;
}

int av_frame_copy(AVFrame *dst, const AVFrame *src)
{
    if (dst->format != src->format || dst->format < 0)
        return AVERROR(EINVAL);

    if (dst->width > 0 && dst->height > 0)
        return frame_copy_video(dst, src);
    else if (dst->nb_samples > 0 && dst->channel_layout)
        return frame_copy_audio(dst, src);

    return AVERROR(EINVAL);
}

#undef CHECK_CHANNELS_CONSISTENCY

#endif // LIBAVUTIL_VERSION_INT <= 53.5.0 (ffmpeg 2.2)

} // anonymous namespace

namespace av
{

VideoFrame2::VideoFrame2(AVPixelFormat pixelFormat, int width, int height, int align)
{
    m_raw->format = pixelFormat;
    m_raw->width  = width;
    m_raw->height = height;
    av_frame_get_buffer(m_raw, align);
}

VideoFrame2::VideoFrame2(const uint8_t *data, size_t size, AVPixelFormat pixelFormat, int width, int height, int align) throw(std::length_error)
    : VideoFrame2(pixelFormat, width, height, align)
{
    size_t calcSize = av_image_get_buffer_size(pixelFormat, width, height, align);
    if (calcSize != size)
        throw length_error("Data size and required buffer for this format/width/height/align not equal");

    uint8_t *buf[4];
    int      linesize[4];
    av_image_fill_arrays(buf, linesize, data, pixelFormat, width, height, align);

    // copy data
    for (size_t i = 0; i < 4 && buf[i]; ++i) {
        std::copy(buf[i], buf[i]+linesize[i], m_raw->data[i]);
    }
}

VideoFrame2::VideoFrame2(const VideoFrame2 &other)
    : Frame2<VideoFrame2>(other)
{
}

VideoFrame2::VideoFrame2(VideoFrame2 &&other)
    : Frame2<VideoFrame2>(move(other))
{
}

VideoFrame2 &VideoFrame2::operator=(const VideoFrame2 &rhs)
{
    return assignOperator(rhs);
}

VideoFrame2 &VideoFrame2::operator=(VideoFrame2 &&rhs)
{
    return moveOperator(move(rhs));
}

AVPixelFormat VideoFrame2::pixelFormat() const
{
    return static_cast<AVPixelFormat>(m_raw->format);
}

int VideoFrame2::width() const
{
    return m_raw->width;
}

int VideoFrame2::height() const
{
    return m_raw->height;
}

bool VideoFrame2::isKeyFrame() const
{
    return m_raw->key_frame;
}

void VideoFrame2::setKeyFrame(bool isKey)
{
    m_raw->key_frame = isKey;
}

int VideoFrame2::quality() const
{
    return m_raw->quality;
}

void VideoFrame2::setQuality(int quality)
{
    m_raw->quality = quality;
}

AVPictureType VideoFrame2::pictureType() const
{
    return m_raw->pict_type;
}

void VideoFrame2::setPictureType(AVPictureType type)
{
    m_raw->pict_type = type;
}


AudioSamples2::AudioSamples2(AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate, int align)
{
    m_raw->format      = sampleFormat;
    m_raw->sample_rate = sampleRate;
    m_raw->nb_samples  = samplesCount;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54,59,100) // 1.0
    av_frame_set_channel_layout(m_raw, av_get_default_channel_layout(channels));
#else
    av_frame_set_channels(m_raw, channels);
#endif

    av_frame_get_buffer(m_raw, align);
}

AudioSamples2::AudioSamples2(const uint8_t *data, size_t size, AVSampleFormat sampleFormat, int samplesCount, int channels, int sampleRate, int align) throw(std::length_error)
    : AudioSamples2(sampleFormat, samplesCount, channels, sampleRate, align)
{
    size_t calcSize = av_samples_get_buffer_size(nullptr, channels, samplesCount, sampleFormat, align);
    if (calcSize > size)
        throw length_error("Data size and required buffer for this format/nb_samples/nb_channels/align not equal");

    uint8_t *buf[channels];
    int      linesize[channels];
    av_samples_fill_arrays(buf, linesize, data, channels, samplesCount, sampleFormat, align);

    // copy data
    for (size_t i = 0; i < channels && i < AV_NUM_DATA_POINTERS; ++i) {
        std::copy(buf[i], buf[i]+linesize[i], m_raw->data[i]);
    }
}

AVSampleFormat AudioSamples2::sampleFormat() const
{
    return static_cast<AVSampleFormat>(m_raw->format);
}

int AudioSamples2::samplesCount() const
{
    return m_raw->nb_samples;
}

int AudioSamples2::channelsCount() const
{
    return m_raw->channels;
}

int64_t AudioSamples2::channelsLayout() const
{
    return m_raw->channel_layout;
}

int AudioSamples2::sampleRate() const
{
    return m_raw->sample_rate;
}

uint AudioSamples2::sampleBitDepth() const
{
    return (av_get_bytes_per_sample(static_cast<AVSampleFormat>(m_raw->format))) << 3;
}



Frame::Frame()
    : m_frame(0),
      m_timeBase(AV_TIME_BASE_Q),
      m_streamIndex(0),
      m_completeFlag(false),
      m_fakePts(AV_NOPTS_VALUE)
{
    allocFrame();


}


Frame::~Frame()
{
    if (!m_frame)
        av_frame_free(&m_frame);
}


void Frame::initFromAVFrame(const AVFrame *frame)
{
    if (!frame)
    {
        std::cout << "Try init from NULL frame" << std::endl;
        return;
    }

    // Setup pointers
    setupDataPointers(frame);

    // Copy frame
    av_frame_copy(m_frame, frame);
    av_frame_copy_props(m_frame, frame);
}


int64_t Frame::getPts() const
{
    return (m_frame ? m_frame->pts : AV_NOPTS_VALUE);
}

void Frame::setPts(int64_t pts)
{
    if (m_frame)
    {
        m_frame->pts = pts;
        setFakePts(pts);
    }
}

int64_t Frame::getBestEffortTimestamp() const
{
    return (m_frame ? m_frame->best_effort_timestamp : AV_NOPTS_VALUE);
}

int64_t Frame::getFakePts() const
{
    return m_fakePts;
}

void Frame::setFakePts(int64_t pts)
{
    m_fakePts = pts;
}

void Frame::setTimeBase(const Rational &value)
{
    if (m_timeBase == value)
        return;

    int64_t rescaledPts          = AV_NOPTS_VALUE;
    int64_t rescaledFakePts      = AV_NOPTS_VALUE;
    int64_t rescaledBestEffortTs = AV_NOPTS_VALUE;

    if (m_frame)
    {
        if (m_timeBase != Rational() && value != Rational())
        {
            if (m_frame->pts != AV_NOPTS_VALUE)
                rescaledPts = m_timeBase.rescale(m_frame->pts, value);

            if (m_frame->best_effort_timestamp != AV_NOPTS_VALUE)
                rescaledBestEffortTs = m_timeBase.rescale(m_frame->best_effort_timestamp, value);

            if (m_fakePts != AV_NOPTS_VALUE)
                rescaledFakePts = m_timeBase.rescale(m_fakePts, value);
        }
        else
        {
            rescaledPts          = m_frame->pts;
            rescaledFakePts      = m_fakePts;
            rescaledBestEffortTs = m_frame->best_effort_timestamp;
        }
    }

    m_timeBase = value;

    if (m_frame)
    {
        m_frame->pts                   = rescaledPts;
        m_frame->best_effort_timestamp = rescaledBestEffortTs;
        m_fakePts                      = rescaledFakePts;
    }
}

int Frame::getStreamIndex() const
{
    return m_streamIndex;
}

void Frame::setStreamIndex(int streamIndex)
{
    this->m_streamIndex = streamIndex;
}


const AVFrame *Frame::getAVFrame() const
{
    return m_frame;
}

AVFrame *Frame::getAVFrame()
{
    return m_frame;
}

const vector<uint8_t> &Frame::getFrameBuffer() const
{
    return m_frameBuffer;
}

void Frame::setComplete(bool isComplete)
{
    m_completeFlag = isComplete;
}

void Frame::dump()
{
    av_hex_dump(stdout, m_frameBuffer.data(), m_frameBuffer.size());
}

Frame &Frame::operator =(const AVFrame *frame)
{
    if (m_frame && frame)
        initFromAVFrame(frame);
    return *this;
}

Frame &Frame::operator =(const Frame &frame)
{
    if (m_frame == frame.m_frame || this == &frame)
        return *this;

    if (this->m_frame)
    {
        initFromAVFrame(frame.getAVFrame());
        m_timeBase     = frame.m_timeBase;
        m_completeFlag = frame.m_completeFlag;
        setPts(frame.getPts());
        m_fakePts      = frame.m_fakePts;
    }
    return *this;
}


void Frame::allocFrame()
{
    m_frame = av_frame_alloc();
    m_frame->opaque = this;
}


} // ::av
