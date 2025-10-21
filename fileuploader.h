#ifndef FILEUPLOADER_H
#define FILEUPLOADER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>

class FileUploader : public QObject
{
    Q_OBJECT
public:
    explicit FileUploader(QObject *parent = nullptr);
    ~FileUploader();

    // 设置服务器地址和端口
    void setServerInfo(const QString &ip, quint16 port);
    void connectServer();

    // 上传文件
    void uploadFile(const QString &filePath);

signals:
    // 错误信号
    void errorOccurred(const QString &errorString);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    QTcpSocket *m_socket;
    QString m_serverIp;
    quint16 m_serverPort;
    QFile *m_file;
    qint64 m_fileSize;
    qint64 m_bytesSent;
};

#endif // FILEUPLOADER_H
