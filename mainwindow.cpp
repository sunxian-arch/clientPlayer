#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    currentFile = "D:\\video\\002.mp4";

    //m_playerThread = new VideoPlayer(this);
    player = new QMediaPlayer(this);
    player->setVideoOutput(ui->videoWidget);

    // 与服务器建立连接
    connectServer();
    initSlots();
}

MainWindow::~MainWindow()
{
    //m_playerThread->stopPlayback();
    delete ui;
}

void MainWindow::connectServer()
{
    // 设置服务器地址和端口
    uploader.setServerInfo("172.23.206.96", 12345);
    uploader.connectServer();
}

void MainWindow::initSlots()
{
    connect(&uploader, &FileUploader::errorOccurred, [](const QString &error) {
       qDebug() << "错误:" << error;
       // QCoreApplication::quit();
    });

//    connect(m_playerThread, &VideoPlayer::frameReady,
//            this, &MainWindow::onFrameReady);
//    connect(m_playerThread, &VideoPlayer::statusChanged,
//            this, &MainWindow::onStatusChanged);
//    connect(m_playerThread, &VideoPlayer::errorOccurred,
//            this, &MainWindow::onErrorOccurred);
    connect(player, &QMediaPlayer::stateChanged, this, &MainWindow::do_stateChanged);
    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::do_positionChanged);
    connect(player, &QMediaPlayer::durationChanged, this, &MainWindow::do_durationChanged);
}

void MainWindow::uploadFile(QString fileName)
{
    // 开始上传文件
    uploader.uploadFile(fileName);
}

// 更新上传的视频信息
void MainWindow::updateVideoMess()
{

}

void MainWindow::onFrameReady(const QImage &frame)
{
//    if (!frame.isNull()) {
//        QPixmap pixmap = QPixmap::fromImage(frame);
//        ui->videoLabel->setPixmap(pixmap.scaled(ui->videoLabel->size(),
//                                              Qt::KeepAspectRatio));
//    }
}

void MainWindow::onStatusChanged(int status)
{

}

void MainWindow::onErrorOccurred(const QString &errorMessage)
{

}

void MainWindow::do_stateChanged(QMediaPlayer::State state)
{//播放器状态变化
    bool isPlaying = (state == QMediaPlayer::PlayingState);
    ui->btnPlay->setEnabled(!isPlaying);
    ui->btnPause->setEnabled(isPlaying);
    ui->btnStop->setEnabled(isPlaying);
}

void MainWindow::do_durationChanged(qint64 duration)
{//文件时长变化
    ui->sliderPosition->setMaximum(duration);

    int secs = duration / 1000;   //秒
    int mins = secs / 60;        //分钟
    secs = secs % 60;            //余数秒
    durationTime = QString::asprintf("%d:%d", mins, secs);
    ui->LabRatio->setText(positionTime + "/" + durationTime);
}

void MainWindow::do_positionChanged(qint64 position)
{//文件播放位置变化
    if (ui->sliderPosition->isSliderDown())
        return;  //如果正在拖动滑条，退出
    ui->sliderPosition->setSliderPosition(position);

    int secs = position / 1000;  //秒
    int mins = secs / 60;        //分钟
    secs = secs % 60;            //余数秒
    positionTime = QString::asprintf("%d:%d", mins, secs);
    ui->LabRatio->setText(positionTime + "/" + durationTime);
}

void MainWindow::on_btnAdd_clicked()
{
    QString strVideoPath = QFileDialog::getOpenFileName(this, "Open Video File",
                                                currentFile.isEmpty() ? QApplication::applicationDirPath() : currentFile,
                                                "Video Files(*.mp4 *.flv *.avi *.ts *.mkv *.rmvb *.kux)");
    if (strVideoPath.isEmpty()) {
        return;
    }
    currentFile = strVideoPath;
    this->setWindowTitle(this->windowTitle().split(" ").first() + " " + QFileInfo(strVideoPath).fileName());
    ui->videoListWidget->addItem(QFileInfo(strVideoPath).fileName());
    uploadFile(currentFile);
    //QtConcurrent::run(this, &MainWindow::uploadFile, strVideoPath);
}

void MainWindow::on_btnPlay_clicked()
{
    player->play();
}

void MainWindow::on_btnPause_clicked()
{
    player->pause();
}

void MainWindow::on_btnStop_clicked()
{
    player->stop();
}

void MainWindow::on_sliderVolumn_valueChanged(int value)
{
    player->setVolume(value);
}

void MainWindow::on_btnSound_clicked()
{
    // 获取当前静音状态
    bool mute = player->isMuted();

    // 切换静音状态
    player->setMuted(!mute);

    // 更新按钮图标
    if (mute) {
        ui->btnSound->setIcon(QIcon(":/images/images/volumn.bmp")); // 取消静音，显示音量图标
    } else {
        ui->btnSound->setIcon(QIcon(":/images/images/mute.bmp"));   // 静音，显示静音图标
    }
}

void MainWindow::on_sliderPosition_valueChanged(int value)
{
    player->setPosition(value);
}

void MainWindow::on_videoListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;  // 安全检查

    QString selectedText = item->text();
    qDebug() << "选中了：" << selectedText;
    ui->labCurMedia->setText(selectedText);

    if (player->state() != QMediaPlayer::StoppedState) {
        player->stop();            // 停止播放
        player->setMedia(QMediaContent());  // 清空媒体内容
    }

    QString hlsUrl = "http://172.23.206.96:8081/vod/";  // 基础 URL
    hlsUrl += QFileInfo(selectedText).baseName();  // 文件名
    QString clarity = ui->clarityComboBox->currentText();  // 选项是 "720p", "1080p"
    hlsUrl += '/' + clarity;
    hlsUrl += "/index.m3u8";
    qDebug() << "生成的 HLS URL：" << hlsUrl;

    // m_playerThread->play(hlsUrl);
    player->setMedia(QUrl(hlsUrl));
    //player->setMedia(QUrl::fromLocalFile("D:/video/002.mp4"));
    player->play();
}
