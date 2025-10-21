#ifndef FFMPEGPROCESSOR_H
#define FFMPEGPROCESSOR_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QMutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
//#include <SDL.h>
}

class FFmpegProcessor : public QObject
{
    Q_OBJECT

public:
    enum class StreamStatus {
        Stopped,
        Connecting,
        Playing,
        Paused,
        Error
    };

    explicit FFmpegProcessor(QObject *parent = nullptr);
    ~FFmpegProcessor();

    // 流操作
    bool openStream(const QString &url);
    void closeStream();
    bool readFrame();
    QImage getCurrentFrame();

    // 流信息
    StreamStatus getStatus() const;
    QString getErrorString() const;
    int getVideoWidth() const;
    int getVideoHeight() const;
    double getFrameRate() const;
    QString getCodecName() const;

    // 控制操作
    void pause();
    void resume();
    void seek(double seconds);

public:
    // 新增音频相关函数
    bool initAudio();
    bool decodeAudioPacket(AVPacket *packet);

    // 新增音频信息获取函数
    int getAudioStreamIndex() const;
    int getAudioSampleRate() const;
    int getAudioChannels() const;
    QString getAudioCodecName() const;

signals:
    void frameReady(const QImage &frame);
    void statusChanged(int status);
    void errorOccurred(const QString &errorMessage);

    // 新增音频相关信号
    void audioReady(const QByteArray &audioData);
    void audioFormatChanged(int sampleRate, int channels);

private:
    void initFFmpeg();
    void cleanup();
    bool initCodec();
    bool initSwsContext();
    bool decodePacket(AVPacket *packet);
    void convertFrameToRGB();
    QImage avFrameToQImage(AVFrame *frame);

    // 新增音频相关私有函数
    bool initSwrContext();

    // FFmpeg 相关变量
    AVFormatContext *m_formatContext;
    AVCodecContext *m_codecContext;
    AVFrame *m_frame;
    AVFrame *m_frameRGB;
    AVPacket *m_packet;
    SwsContext *m_swsContext;

    // 状态变量
    StreamStatus m_status;
    QString m_errorString;
    int m_videoStreamIndex;
    uint8_t *m_rgbBuffer;

    // 视频信息
    int m_videoWidth;
    int m_videoHeight;
    double m_frameRate;
    QString m_codecName;

    // 新增音频相关成员变量
    int m_audioStreamIndex;
    AVCodecContext *m_audioCodecContext;
    SwrContext *m_swrContext;
    AVFrame *m_audioFrame;
    uint8_t *m_audioBuffer;
    int m_audioBufferSize;

    // 音频信息
    int m_audioSampleRate;
    int m_audioChannels;
    QString m_audioCodecName;
    AVSampleFormat m_audioSampleFormat;

    // 线程安全
    mutable QMutex m_mutex;
};

#endif // FFMPEGPROCESSOR_H
