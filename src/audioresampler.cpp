#include <cstdio>

#include "audioresampler.h"
#include "avlog.h"

using namespace std;

namespace av {

AudioResampler::AudioResampler()
{
}

AudioResampler::AudioResampler(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                               int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat,
                               error_code &ec)
    : AudioResampler()
{
    init(dstChannelsLayout, dstRate, dstFormat, srcChannelsLayout, srcRate, srcFormat, ec);
}

AudioResampler::AudioResampler(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                               int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat,
                               Dictionary &options,
                               error_code &ec)
    : AudioResampler()
{
    init(dstChannelsLayout, dstRate, dstFormat, srcChannelsLayout, srcRate, srcFormat, options, ec);
}

AudioResampler::AudioResampler(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                               int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat,
                               Dictionary &&options,
                               error_code &ec)
    : AudioResampler()
{
    init(dstChannelsLayout, dstRate, dstFormat, srcChannelsLayout, srcRate, srcFormat, options, ec);
}


AudioResampler::~AudioResampler()
{
    if (m_raw)
    {
        swr_free(&m_raw);
    }
}

AudioResampler::AudioResampler(AudioResampler && other)
    : AudioResampler()
{
    swap(other);
}

AudioResampler& AudioResampler::operator=(AudioResampler && rhs)
{
    if (this != &rhs) {
        swap(rhs);
        AudioResampler().swap(rhs);
    }
    return *this;
}

void AudioResampler::swap(AudioResampler & other)
{
    using std::swap;
#define SWAP(x) swap(x, other.x);
    SWAP(m_dstChannelsLayout);
    SWAP(m_dstRate);
    SWAP(m_dstFormat);
    SWAP(m_srcChannelsLayout);
    SWAP(m_srcRate);
    SWAP(m_srcFormat);
    SWAP(m_streamIndex);
    SWAP(m_prevPts);
    SWAP(m_nextPts);
#undef SWAP
}

int64_t AudioResampler::dstChannelLayout() const
{
    return m_dstChannelsLayout;
}

int AudioResampler::dstChannels() const
{
    return av_get_channel_layout_nb_channels(m_dstChannelsLayout);
}

int AudioResampler::dstSampleRate() const
{
    return m_dstRate;
}

AVSampleFormat AudioResampler::dstSampleFormat() const
{
    return m_dstFormat;
}

int64_t AudioResampler::srcChannelLayout() const
{
    return m_srcChannelsLayout;
}

int AudioResampler::srcChannels() const
{
    return av_get_channel_layout_nb_channels(m_srcChannelsLayout);
}

int AudioResampler::srcSampleRate() const
{
    return m_srcRate;
}

AVSampleFormat AudioResampler::srcSampleFormat() const
{
    return m_srcFormat;
}

bool AudioResampler::pop(AudioSamples2 &dst, bool getall, error_code &ec)
{
    clear_if(ec);

    if (!m_raw) {
        fflog(AV_LOG_ERROR, "SwrContext does not inited\n");
        throws_if(ec, Errors::ResamplerNotInited);
        return false;
    }

    if (dst.sampleRate() != dstSampleRate() ||
        dst.sampleFormat() != dstSampleFormat() ||
        dst.channelsCount() != dstChannels() ||
        dst.channelsLayout() != dstChannelLayout())
    {
        throws_if(ec, Errors::ResamplerOutputChanges);
        return false;
    }

    auto result = swr_get_delay(m_raw, m_dstRate);
    //clog << "  delay [pop]: " << result << endl;

    // Need more data
    if (result < dst.samplesCount() && getall == false)
    {
        return false;
    }

    auto sts = swr_convert_frame(m_raw, dst.raw(), nullptr);
    if (sts < 0)
    {
        throws_if(ec, sts, ffmpeg_category());
        return false;
    }

    dst.setTimeBase(Rational(1, m_dstRate));
    dst.setStreamIndex(m_streamIndex);
    dst.setComplete(true);

    // Ugly PTS handling. More clean one can be done by user code
    if (m_nextPts == AV_NOPTS_VALUE) {
        m_nextPts = 0;
    }
    dst.setPts(m_nextPts);
    m_nextPts = dst.pts() + dst.samplesCount();

    //result = swr_get_delay(m_raw, m_dstRate);
    //clog << "  delay [pop]: " << result << endl;

    // When no data, samples count sets to zero
    return dst.samplesCount() ? true : false;
}

AudioSamples2 AudioResampler::pop(size_t samplesCount, error_code &ec)
{
    clear_if(ec);

    if (!m_raw)
    {
        fflog(AV_LOG_ERROR, "SwrContext does not inited\n");
        throws_if(ec, Errors::ResamplerNotInited);
        return AudioSamples2(nullptr);
    }

    auto delay = swr_get_delay(m_raw, m_dstRate);

    // Need more data
    if ((size_t)delay < samplesCount && samplesCount)
    {
        return AudioSamples2(nullptr);
    }

    if (!samplesCount)
        samplesCount = delay; // Request all samples

    AudioSamples2 dst(dstSampleFormat(), samplesCount, dstChannelLayout(), dstSampleRate());
    if (!dst.isValid())
    {
        throws_if(ec, Errors::CantAllocateFrame);
        return AudioSamples2(nullptr);
    }

    auto sts = swr_convert_frame(m_raw, dst.raw(), nullptr);
    if (sts < 0)
    {
        throws_if(ec, sts, ffmpeg_category());
        return AudioSamples2(nullptr);
    }

    dst.setTimeBase(Rational(1, m_dstRate));
    dst.setStreamIndex(m_streamIndex);
    dst.setComplete(true);

    // Ugly PTS handling. More clean one can be done by user code
    if (m_nextPts == AV_NOPTS_VALUE) {
        m_nextPts = 0;
    }
    dst.setPts(m_nextPts);
    m_nextPts = dst.pts() + dst.samplesCount();

    return dst.samplesCount() ? std::move(dst) : AudioSamples2::null();
}

void AudioResampler::push(const AudioSamples2 &src, error_code &ec)
{
    if (!m_raw)
    {
        fflog(AV_LOG_ERROR, "SwrContext does not inited\n");
        throws_if(ec, Errors::ResamplerNotInited);
        return;
    }

    if (src.sampleRate() != srcSampleRate() ||
        src.sampleFormat() != srcSampleFormat() ||
        src.channelsCount() != srcChannels() ||
        src.channelsLayout() != srcChannelLayout())
    {
        throws_if(ec, Errors::ResamplerInputChanges);
        return;
    }

    auto sts = swr_convert_frame(m_raw, nullptr, src.raw());
    if (sts < 0)
    {
        throws_if(ec, sts, ffmpeg_category());
        return;
    }

    // TODO: need protection if we still work in scheme: One Resampler Per Channel
    m_streamIndex = src.streamIndex();

    // Need to restore PTS in output frames
    if (m_prevPts > src.pts()) // Reset case
        m_nextPts = AV_NOPTS_VALUE;
    m_prevPts     = src.pts();

    //auto result = swr_get_delay(m_raw, m_dstRate);
    //clog << "  delay [push]: " << result << endl;
}


bool AudioResampler::isValid() const
{
    return !!m_raw;
}

bool AudioResampler::init(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                          int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat,
                          error_code &ec)
{
    return init(dstChannelsLayout, dstRate, dstFormat,
                srcChannelsLayout, srcRate, srcFormat,
                nullptr, ec);
}

bool AudioResampler::init(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                          int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat,
                          Dictionary &options, error_code &ec)
{
    auto ptr = options.release();
    ScopeOutAction onReturn([&ptr, &options](){
        options.assign(ptr);
    });

    return init(dstChannelsLayout, dstRate, dstFormat,
                srcChannelsLayout, srcRate, srcFormat,
                &ptr, ec);
}

bool AudioResampler::init(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                          int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat,
                          Dictionary &&options, error_code &ec)
{
    return init(dstChannelsLayout, dstRate, dstFormat,
                srcChannelsLayout, srcRate, srcFormat,
                options, ec);
}

bool AudioResampler::validate(int64_t channelsLayout, int rate, AVSampleFormat format)
{
    if (channelsLayout <= 0)
        return false;

    if (rate <= 0)
        return false;

    if (format == AV_SAMPLE_FMT_NONE)
        return false;

    return true;
}

bool AudioResampler::init(int64_t dstChannelsLayout, int dstRate, AVSampleFormat dstFormat,
                          int64_t srcChannelsLayout, int srcRate, AVSampleFormat srcFormat,
                          AVDictionary **dict, error_code &ec)
{
    clear_if(ec);

    if (!validate(dstChannelsLayout, dstRate, dstFormat) ||
        !validate(srcChannelsLayout, srcRate, srcFormat))
    {
        throws_if(ec, Errors::ResamplerInvalidParameters);
        return false;
    }

    if (m_raw == nullptr)
    {
        m_raw = swr_alloc();
        if (m_raw == nullptr)
        {
            fflog(AV_LOG_FATAL, "Can't alloc SwrContext\n");
            throws_if(ec, ENOMEM, std::system_category());
            return false;
        }
    }

    int            sts = 0;
    ScopeOutAction onReturn([&sts, this](){
        if (sts < 0)
        {
            fflog(AV_LOG_ERROR, "Can't initalize Audio Resample context\n");
            swr_free(&m_raw);
        }
    });

    /* set options */
    sts = av_opt_set_channel_layout(m_raw, "in_channel_layout",     srcChannelsLayout, 0);
    if (sts < 0)
        goto ffmpeg_internal_fails;

    sts = av_opt_set_int(m_raw,            "in_sample_rate",        srcRate,           0);
    if (sts < 0)
        goto ffmpeg_internal_fails;

    sts = av_opt_set_sample_fmt(m_raw,     "in_sample_fmt",         srcFormat,         0);
    if (sts < 0)
        goto ffmpeg_internal_fails;

    sts = av_opt_set_channel_layout(m_raw, "out_channel_layout",    dstChannelsLayout, 0);
    if (sts < 0)
        goto ffmpeg_internal_fails;

    sts = av_opt_set_int(m_raw,            "out_sample_rate",       dstRate,           0);
    if (sts < 0)
        goto ffmpeg_internal_fails;

    sts = av_opt_set_sample_fmt(m_raw,     "out_sample_fmt",        dstFormat,         0);
    if (sts < 0)
        goto ffmpeg_internal_fails;

    // Set optional options
    if (dict)
    {
        sts = av_opt_set_dict(m_raw, dict);
        if (sts < 0)
            goto ffmpeg_internal_fails;
    }

    if ((sts = swr_init(m_raw)) < 0)
    {
        goto ffmpeg_internal_fails;
    }

    // Cache values
    m_dstChannelsLayout = dstChannelsLayout;
    m_dstRate           = dstRate;
    m_dstFormat         = dstFormat;
    m_srcChannelsLayout = srcChannelsLayout;
    m_srcRate           = srcRate;
    m_srcFormat         = srcFormat;

    return true;

ffmpeg_internal_fails:
    throws_if(ec, sts, ffmpeg_category());
    return false;
}


} // namespace av
