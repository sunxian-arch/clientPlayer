#include "fileuploader.h"
#include <QHostAddress>
#include <QDataStream>
#include <QFileInfo>
#include <QNetworkProxy>

FileUploader::FileUploader(QObject *parent) : QObject(parent),
    m_socket(nullptr),
    m_serverPort(0),
    m_file(nullptr),
    m_fileSize(0),
    m_bytesSent(0)
{
    m_socket = new QTcpSocket(this);
    // 禁用代理
    m_socket->setProxy(QNetworkProxy::NoProxy);
    // connect(m_socket, &QTcpSocket::connected, this, &FileUploader::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &FileUploader::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &FileUploader::onErrorOccurred);
}

FileUploader::~FileUploader()
{
    m_socket->disconnectFromHost();
    qDebug() << "服务器断开连接";
}

void FileUploader::setServerInfo(const QString &ip, quint16 port)
{
    m_serverIp = ip;
    m_serverPort = port;
}

void FileUploader::connectServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        emit errorOccurred("已经在连接状态");
        return;
    }

    // 连接到服务器
    m_socket->connectToHost(QHostAddress(m_serverIp), m_serverPort);
    qDebug() << "服务器连接成功";

    // 设置连接超时为5秒
    if (!m_socket->waitForConnected(5000)) {
        emit errorOccurred(QString("连接服务器超时: %1").arg(m_socket->errorString()));
        return;
    }
}

void FileUploader::uploadFile(const QString &filePath)
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "I am here";
        m_socket->connectToHost(m_serverIp, m_serverPort);
        qDebug() << "I am here3";
        if (!m_socket->waitForConnected(5000)) { // 5秒超时
            qDebug() << "I am here2";
            emit errorOccurred(QString("重连失败: %1").arg(m_socket->errorString()));
            return;
        }
    }

    m_file = new QFile(filePath, this);
    if (!m_file->open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("无法打开文件: %1").arg(m_file->errorString()));
        delete m_file;
        m_file = nullptr;
        return;
    }

    m_fileSize = m_file->size();
    m_bytesSent = 0;

    // 发送文件头信息(文件名和大小)
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    QByteArray fileNameUtf8 = QFileInfo(*m_file).fileName().toUtf8();

    // 手动写入，避免 QDataStream 自动添加长度
    stream << quint32(fileNameUtf8.size());
    stream.writeRawData(fileNameUtf8.constData(), fileNameUtf8.size());
    stream << m_fileSize;

    m_socket->write(header);

    // 分块发送文件内容
    const qint64 chunkSize = 64 * 1024; // 64KB
    while (!m_file->atEnd()) {
        QByteArray chunk = m_file->read(chunkSize);
        qint64 bytesWritten = m_socket->write(chunk);

        if (bytesWritten == -1) {
            emit errorOccurred(QString("发送失败: %1").arg(m_socket->errorString()));
            m_socket->disconnectFromHost();
            return;
        }

        m_bytesSent += bytesWritten;
        m_socket->waitForBytesWritten();

        // 计算并发送进度信号
        int progress = static_cast<int>((m_bytesSent * 100) / m_fileSize);
        qDebug() << "上传进度:" << progress << "%";
        qDebug() << "以发送字节数:" << m_bytesSent << "，总字节数:" << m_fileSize;
    }

    // 文件发送完成
    m_file->close();
    delete m_file;
    m_file = nullptr;

    qDebug() << "文件上传完成";
}

void FileUploader::onConnected()
{
    qDebug() << "已连接到服务器";
}

void FileUploader::onDisconnected()
{
    qDebug() << "已断开连接";
}

void FileUploader::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit errorOccurred(m_socket->errorString());

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}
