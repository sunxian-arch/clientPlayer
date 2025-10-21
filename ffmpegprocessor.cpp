#include "ffmpegprocessor.h"
#include <QDebug>
#include <QDateTime>

FFmpegProcessor::FFmpegProcessor(QObject *parent)
    : QObject(parent),
      m_formatContext(nullptr),
      m_codecContext(nullptr),
      m_frame(nullptr),
      m_frameRGB(nullptr),
      m_packet(nullptr),
      m_swsContext(nullptr),
      m_status(StreamStatus::Stopped),
      m_videoStreamIndex(-1),
      m_rgbBuffer(nullptr),
      m_videoWidth(0),
      m_videoHeight(0),
      m_frameRate(0.0)
{
    initFFmpeg();
}

FFmpegProcessor::~FFmpegProcessor()
{
    closeStream();
}

void FFmpegProcessor::initFFmpeg()
{
    QMutexLocker locker(&m_mutex);

    // 初始化 FFmpeg 库
    avformat_network_init();
    avcodec_register_all();

    qDebug() << "FFmpeg initialized";
}

bool FFmpegProcessor::openStream(const QString &url)
{
    QMutexLocker locker(&m_mutex);

    if (m_status == StreamStatus::Playing || m_status == StreamStatus::Connecting) {
        m_errorString = "Stream is already open";
        return false;
    }

    m_status = StreamStatus::Connecting;
    emit statusChanged(static_cast<int>(m_status));

    // 清理之前的资源
    cleanup();

    // 打开视频流
    QByteArray urlBytes = url.toUtf8();
    const char *cUrl = urlBytes.constData();

    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0);

    int ret = avformat_open_input(&m_formatContext, cUrl, nullptr, &options);
    av_dict_free(&options);

    if (ret != 0) {
        m_errorString = QString("无法打开视频流: %1").arg(ret);
        emit errorOccurred(m_errorString);
        m_status = StreamStatus::Error;
        emit statusChanged(static_cast<int>(m_status));
        return false;
    }

    // 获取流信息
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        m_errorString = "无法获取流信息";
        emit errorOccurred(m_errorString);
        m_status = StreamStatus::Error;
        emit statusChanged(static_cast<int>(m_status));
        return false;
    }

    // 查找视频流
    m_videoStreamIndex = -1;
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }

    if (m_videoStreamIndex == -1) {
        m_errorString = "未找到视频流";
        emit errorOccurred(m_errorString);
        m_status = StreamStatus::Error;
        emit statusChanged(static_cast<int>(m_status));
        return false;
    }

    // 在成功打开视频流后，初始化音频
    if (m_videoStreamIndex != -1) {
        if (!initAudio()) {
            qDebug() << "音频初始化失败，继续播放视频";
        } else {
            qDebug() << "音频初始化成功";
            emit audioFormatChanged(m_audioSampleRate, m_audioChannels);
        }
    }

    // 初始化编解码器
    if (!initCodec()) {
        return false;
    }

    // 初始化图像转换上下文
    if (!initSwsContext()) {
        return false;
    }

    m_status = StreamStatus::Playing;
    emit statusChanged(static_cast<int>(m_status));
    return true;
}

void FFmpegProcessor::closeStream()
{
    QMutexLocker locker(&m_mutex);

    if (m_status == StreamStatus::Stopped) {
        return;
    }

    cleanup();

    m_status = StreamStatus::Stopped;
    emit statusChanged(static_cast<int>(m_status));
}

bool FFmpegProcessor::readFrame()
{
    if (m_status != StreamStatus::Playing) {
        return false;
    }

    if (!m_packet) {
        m_packet = av_packet_alloc();
    }

    int ret = av_read_frame(m_formatContext, m_packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            qDebug() << "End of stream";
        } else {
            m_errorString = QString("读取帧失败: %1").arg(ret);
            emit errorOccurred(m_errorString);
        }
        return false;
    }

    if (m_packet->stream_index == m_videoStreamIndex) {
        if (!decodePacket(m_packet)) {
            av_packet_unref(m_packet);
            return false;
        }
    }
    else if (m_packet->stream_index == m_audioStreamIndex) {
        if (!decodeAudioPacket(m_packet)) {
            av_packet_unref(m_packet);
            return false;
        }
    }

    av_packet_unref(m_packet);
    return true;
}

QImage FFmpegProcessor::getCurrentFrame()
{
    if (m_status != StreamStatus::Playing || !m_frameRGB) {
        return QImage();
    }

    return avFrameToQImage(m_frameRGB);
}

FFmpegProcessor::StreamStatus FFmpegProcessor::getStatus() const
{
    return m_status;
}

QString FFmpegProcessor::getErrorString() const
{
    return m_errorString;
}

int FFmpegProcessor::getVideoWidth() const
{
    return m_videoWidth;
}

int FFmpegProcessor::getVideoHeight() const
{
    return m_videoHeight;
}

double FFmpegProcessor::getFrameRate() const
{
    return m_frameRate;
}

QString FFmpegProcessor::getCodecName() const
{
    return m_codecName;
}

void FFmpegProcessor::pause()
{
    if (m_status == StreamStatus::Playing) {
        m_status = StreamStatus::Paused;
        emit statusChanged(static_cast<int>(m_status));
    }
}

void FFmpegProcessor::resume()
{
    if (m_status == StreamStatus::Paused) {
        m_status = StreamStatus::Playing;
        emit statusChanged(static_cast<int>(m_status));
    }
}

void FFmpegProcessor::seek(double seconds)
{
    if (m_formatContext && m_videoStreamIndex >= 0) {
        int64_t timestamp = seconds * AV_TIME_BASE;
        av_seek_frame(m_formatContext, -1, timestamp, AVSEEK_FLAG_BACKWARD);
    }
}

// 初始化音频相关资源
bool FFmpegProcessor::initAudio()
{
    QMutexLocker locker(&m_mutex);

    // 查找音频流
    m_audioStreamIndex = -1;
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_audioStreamIndex = i;
            break;
        }
    }

    if (m_audioStreamIndex == -1) {
        qDebug() << "未找到音频流";
        return false;
    }

    // 初始化音频编解码器
    AVCodecParameters *codecParameters = m_formatContext->streams[m_audioStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);

    if (!codec) {
        m_errorString = "不支持的音频解码器";
        return false;
    }

    m_audioCodecContext = avcodec_alloc_context3(codec);
    if (!m_audioCodecContext) {
        m_errorString = "无法分配音频解码器上下文";
        return false;
    }

    if (avcodec_parameters_to_context(m_audioCodecContext, codecParameters) < 0) {
        m_errorString = "无法复制音频编解码器参数";
        return false;
    }

    if (avcodec_open2(m_audioCodecContext, codec, nullptr) < 0) {
        m_errorString = "无法打开音频解码器";
        return false;
    }

    // 保存音频信息
    m_audioSampleRate = m_audioCodecContext->sample_rate;
    m_audioChannels = m_audioCodecContext->channels;
    m_audioSampleFormat = m_audioCodecContext->sample_fmt;
    m_audioCodecName = QString(codec->name);

    // 初始化音频重采样上下文
    if (!initSwrContext()) {
        return false;
    }

    // 初始化音频帧
    m_audioFrame = av_frame_alloc();
    if (!m_audioFrame) {
        m_errorString = "无法分配音频帧内存";
        return false;
    }

    return true;
}

// 解码音频包
bool FFmpegProcessor::decodeAudioPacket(AVPacket *packet)
{
    int ret = avcodec_send_packet(m_audioCodecContext, packet);
    if (ret < 0) {
        m_errorString = QString("发送音频数据包到解码器失败: %1").arg(ret);
        emit errorOccurred(m_errorString);
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(m_audioCodecContext, m_audioFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        } else if (ret < 0) {
            m_errorString = QString("接收解码音频帧失败: %1").arg(ret);
            emit errorOccurred(m_errorString);
            return false;
        }

        // 重采样音频
        int out_samples = av_rescale_rnd(swr_get_delay(m_swrContext, m_audioFrame->sample_rate) +
                                        m_audioFrame->nb_samples, 44100, m_audioFrame->sample_rate, AV_ROUND_UP);

        int buffer_size = av_samples_get_buffer_size(nullptr, 2, out_samples, AV_SAMPLE_FMT_S16, 1);
        if (buffer_size < 0) {
            m_errorString = "无法计算音频缓冲区大小";
            emit errorOccurred(m_errorString);
            return false;
        }

        if (!m_audioBuffer || m_audioBufferSize < buffer_size) {
            av_free(m_audioBuffer);
            m_audioBuffer = (uint8_t *)av_malloc(buffer_size);
            m_audioBufferSize = buffer_size;
        }

        uint8_t *out_data[1] = { m_audioBuffer };
        int samples = swr_convert(m_swrContext, out_data, out_samples,
                                (const uint8_t **)m_audioFrame->data, m_audioFrame->nb_samples);

        if (samples < 0) {
            m_errorString = "音频重采样失败";
            emit errorOccurred(m_errorString);
            return false;
        }

        // 发送音频数据信号
        QByteArray audioData((const char *)m_audioBuffer, samples * 2 * 2); // 16位立体声
        emit audioReady(audioData);

        av_frame_unref(m_audioFrame);
    }

    return true;
}

// 音频信息获取函数
int FFmpegProcessor::getAudioStreamIndex() const
{
    return m_audioStreamIndex;
}

int FFmpegProcessor::getAudioSampleRate() const
{
    return m_audioSampleRate;
}

int FFmpegProcessor::getAudioChannels() const
{
    return m_audioChannels;
}

QString FFmpegProcessor::getAudioCodecName() const
{
    return m_audioCodecName;
}

bool FFmpegProcessor::initCodec()
{
    AVCodecParameters *codecParameters = m_formatContext->streams[m_videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);

    if (!codec) {
        m_errorString = "不支持的解码器";
        emit errorOccurred(m_errorString);
        return false;
    }

    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        m_errorString = "无法分配解码器上下文";
        emit errorOccurred(m_errorString);
        return false;
    }

    if (avcodec_parameters_to_context(m_codecContext, codecParameters) < 0) {
        m_errorString = "无法复制编解码器参数";
        emit errorOccurred(m_errorString);
        return false;
    }

    if (avcodec_open2(m_codecContext, codec, nullptr) < 0) {
        m_errorString = "无法打开解码器";
        emit errorOccurred(m_errorString);
        return false;
    }

    // 保存视频信息
    m_videoWidth = m_codecContext->width;
    m_videoHeight = m_codecContext->height;
    m_codecName = QString(codec->name);

    // 计算帧率
    AVStream *stream = m_formatContext->streams[m_videoStreamIndex];
    if (stream->avg_frame_rate.den != 0) {
        m_frameRate = av_q2d(stream->avg_frame_rate);
    } else if (stream->r_frame_rate.den != 0) {
        m_frameRate = av_q2d(stream->r_frame_rate);
    } else {
        m_frameRate = 25.0; // 默认值
    }

    return true;
}

bool FFmpegProcessor::initSwsContext()
{
    m_frame = av_frame_alloc();
    m_frameRGB = av_frame_alloc();

    if (!m_frame || !m_frameRGB) {
        m_errorString = "无法分配帧内存";
        emit errorOccurred(m_errorString);
        return false;
    }

    m_swsContext = sws_getContext(
        m_videoWidth, m_videoHeight, m_codecContext->pix_fmt,
        m_videoWidth, m_videoHeight, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!m_swsContext) {
        m_errorString = "无法创建图像转换上下文";
        emit errorOccurred(m_errorString);
        return false;
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_videoWidth, m_videoHeight, 1);
    m_rgbBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(m_frameRGB->data, m_frameRGB->linesize, m_rgbBuffer,
                        AV_PIX_FMT_RGB24, m_videoWidth, m_videoHeight, 1);

    return true;
}

bool FFmpegProcessor::decodePacket(AVPacket *packet)
{
    int ret = avcodec_send_packet(m_codecContext, packet);
    if (ret < 0) {
        m_errorString = QString("发送数据包到解码器失败: %1").arg(ret);
        emit errorOccurred(m_errorString);
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(m_codecContext, m_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        } else if (ret < 0) {
            m_errorString = QString("接收解码帧失败: %1").arg(ret);
            emit errorOccurred(m_errorString);
            return false;
        }

        // 转换帧格式为 RGB
        sws_scale(m_swsContext, (uint8_t const * const *)m_frame->data,
                 m_frame->linesize, 0, m_videoHeight,
                 m_frameRGB->data, m_frameRGB->linesize);

        // 发出帧就绪信号
        QImage image = avFrameToQImage(m_frameRGB);
        emit frameReady(image);

        av_frame_unref(m_frame);
    }

    return true;
}

void FFmpegProcessor::convertFrameToRGB()
{
    if (m_swsContext && m_frame && m_frameRGB) {
        sws_scale(m_swsContext, (uint8_t const * const *)m_frame->data,
                 m_frame->linesize, 0, m_videoHeight,
                 m_frameRGB->data, m_frameRGB->linesize);
    }
}

QImage FFmpegProcessor::avFrameToQImage(AVFrame *frame)
{
    if (!frame || !frame->data[0]) {
        return QImage();
    }

    return QImage(frame->data[0], m_videoWidth, m_videoHeight,
            frame->linesize[0], QImage::Format_RGB888).copy();
}

// 初始化音频重采样上下文
bool FFmpegProcessor::initSwrContext()
{
    // 设置输出音频参数 (16位有符号PCM，立体声)
    int64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);

    // 设置输入音频参数
    int64_t in_channel_layout = m_audioCodecContext->channel_layout;
    AVSampleFormat in_sample_fmt = m_audioCodecContext->sample_fmt;
    int in_sample_rate = m_audioCodecContext->sample_rate;

    // 创建重采样上下文
    m_swrContext = swr_alloc_set_opts(nullptr,
                                     out_channel_layout, out_sample_fmt, out_sample_rate,
                                     in_channel_layout, in_sample_fmt, in_sample_rate,
                                     0, nullptr);
    if (!m_swrContext) {
        m_errorString = "无法创建音频重采样上下文";
        return false;
    }

    if (swr_init(m_swrContext) < 0) {
        m_errorString = "无法初始化音频重采样上下文";
        return false;
    }

    return true;
}

void FFmpegProcessor::cleanup()
{
    if (m_rgbBuffer) {
        av_free(m_rgbBuffer);
        m_rgbBuffer = nullptr;
    }

    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }

    if (m_frameRGB) {
        av_frame_free(&m_frameRGB);
        m_frameRGB = nullptr;
    }

    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }

    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }

    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
        m_formatContext = nullptr;
    }

    m_videoStreamIndex = -1;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_frameRate = 0.0;
    m_codecName.clear();

    // 新增音频资源清理
    if (m_audioBuffer) {
        av_free(m_audioBuffer);
        m_audioBuffer = nullptr;
        m_audioBufferSize = 0;
    }

    if (m_swrContext) {
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
    }

    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
        m_audioFrame = nullptr;
    }

    if (m_audioCodecContext) {
        avcodec_free_context(&m_audioCodecContext);
        m_audioCodecContext = nullptr;
    }

    m_audioStreamIndex = -1;
    m_audioSampleRate = 0;
    m_audioChannels = 0;
    m_audioCodecName.clear();
}
