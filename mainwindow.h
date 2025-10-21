#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QListWidget>
#include <QMainWindow>
#include <QtMultimedia>
#include "fileuploader.h"
#include "videoplayer.h"
#include "tmyvideowidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FileUploader;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public:
    void connectServer();
    void initSlots();
    void uploadFile(QString fileName);
    void updateVideoMess();

private slots:
    void onFrameReady(const QImage &frame);
    void onStatusChanged(int status);
    void onErrorOccurred(const QString &errorMessage);

    void do_stateChanged(QMediaPlayer::State state);
    void do_durationChanged(qint64 duration);
    void do_positionChanged(qint64 position);

    void on_btnAdd_clicked();

    void on_btnPlay_clicked();

    void on_btnPause_clicked();

    void on_btnStop_clicked();

    void on_sliderVolumn_valueChanged(int value);

    void on_btnSound_clicked();

    void on_sliderPosition_valueChanged(int value);

    void on_videoListWidget_itemDoubleClicked(QListWidgetItem *item);

private:
    Ui::MainWindow *ui;
    FileUploader uploader;
    VideoPlayer *m_playerThread;
    QMediaPlayer *player;
    QString currentFile;
    QString durationTime;
    QString positionTime;
};
#endif // MAINWINDOW_H
