#include "videoplayer.h"
#include <QDebug>

VideoPlayer::VideoPlayer(QObject *parent)
    : QThread(parent),
      m_processor(new FFmpegProcessor()),
      m_stopRequested(false),
      m_pauseRequested(false),
      m_seekPosition(-1)
{
    // 连接FFmpegProcessor的信号
    connect(m_processor, &FFmpegProcessor::frameReady,
            this, &VideoPlayer::frameReady);
    connect(m_processor, &FFmpegProcessor::statusChanged,
            this, &VideoPlayer::statusChanged);
    connect(m_processor, &FFmpegProcessor::errorOccurred,
            this, &VideoPlayer::errorOccurred);
}

VideoPlayer::~VideoPlayer()
{
    stopPlayback();
    quit();
    wait();
    delete m_processor;
}

void VideoPlayer::play(const QString &url)
{
    QMutexLocker locker(&m_mutex);
    m_url = url;
    m_stopRequested = false;
    m_pauseRequested = false;
    m_seekPosition = -1;

    if (!isRunning()) {
        start();
    } else {
        // 如果已经在运行，重新打开流
        m_processor->openStream(url);
    }
}

void VideoPlayer::pausePlayback()
{
    QMutexLocker locker(&m_mutex);
    m_pauseRequested = true;
}

void VideoPlayer::resumePlayback()
{
    QMutexLocker locker(&m_mutex);
    m_pauseRequested = false;
}

void VideoPlayer::stopPlayback()
{
    QMutexLocker locker(&m_mutex);
    m_stopRequested = true;
}

void VideoPlayer::seek(int position)
{
    QMutexLocker locker(&m_mutex);
    m_seekPosition = position;
}

void VideoPlayer::run()
{
    if (!m_processor->openStream(m_url)) {
        emit errorOccurred(m_processor->getErrorString());
        return;
    }

    while (!m_stopRequested) {
        {
            QMutexLocker locker(&m_mutex);

            if (m_pauseRequested) {
                m_processor->pause();
                QThread::msleep(100); // 暂停时降低CPU使用率
                continue;
            } else {
                m_processor->resume();
            }

            if (m_seekPosition >= 0) {
                m_processor->seek(m_seekPosition / 1000.0);
                m_seekPosition = -1;
            }
        }

        if (!m_processor->readFrame()) {
            // 读取失败或流结束
            if (m_processor->getStatus() == FFmpegProcessor::StreamStatus::Error) {
                emit errorOccurred(m_processor->getErrorString());
                break;
            }
            QThread::msleep(10); // 短暂休眠避免CPU占用过高
        }
    }

    m_processor->closeStream();
}
