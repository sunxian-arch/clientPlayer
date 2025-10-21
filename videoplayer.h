#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QThread>
#include <QImage>
#include "ffmpegprocessor.h"

class VideoPlayer : public QThread
{
    Q_OBJECT
public:
    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    void play(const QString &url);
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();
    void seek(int position);

signals:
    void frameReady(const QImage &frame);
    void statusChanged(int status);
    void errorOccurred(const QString &errorMessage);

protected:
    void run() override;

private:
    FFmpegProcessor *m_processor;
    QString m_url;
    bool m_stopRequested;
    bool m_pauseRequested;
    qint64 m_seekPosition;
    QMutex m_mutex;
};

#endif // VIDEOPLAYER_H
