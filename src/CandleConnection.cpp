#include "CandleConnection.h"

#include <QMessageBox>
#include <QTcpSocket>
#include <QThread>

CandleConnection::CandleConnection(QObject* p)
    : QObject(p)
    , m_connImpl(nullptr)
{}

bool CandleConnection::openPort() {

    qDebug() << "CandleConnection::openPort(), type:" << m_connType;

    if(m_connType == CONN_SERIAL) {
        return openSerial();
    } else
    if(m_connType == CONN_TCPIP) {
        return openTcpIp();
    }

    return false;
}

int CandleConnection::write(const QByteArray& arr) {

    qDebug() << "CandleConnection::write() :" << arr;
    return write(arr.data(), arr.length());
}
int CandleConnection::write(const QString& str) {
  return write(str.toLatin1());
}

int CandleConnection::write(const char* data, int len) {
    if(!isOpen())
        return -1;
    return m_connImpl->write(data, len);
}

bool CandleConnection::canReadLine() {
    bool rc = false;
    if(m_connType == CONN_SERIAL) {
        rc = static_cast<QSerialPort*>(m_connImpl)->canReadLine();
    } else
    if(m_connType == CONN_TCPIP) {
        rc = static_cast<QTcpSocket*>(m_connImpl)->canReadLine();
    }
    return rc;
}
QByteArray CandleConnection::readLine() {
    if(m_connType == CONN_SERIAL) {
        return static_cast<QSerialPort*>(m_connImpl)->readLine(1024);
    } else
    if(m_connType == CONN_TCPIP) {
        return static_cast<QTcpSocket*>(m_connImpl)->readLine(1024);
    }
    return "";
}

bool CandleConnection::isOpen() {
    return (m_connImpl != nullptr && m_connImpl->isOpen());
}

bool CandleConnection::openSerial() {

    m_connImpl = new QSerialPort(parent());
    auto conn = static_cast<QSerialPort*>(m_connImpl);

    // Setup serial port
    conn->setParity(QSerialPort::NoParity);
    conn->setDataBits(QSerialPort::Data8);
    conn->setFlowControl(QSerialPort::NoFlowControl);
    conn->setStopBits(QSerialPort::OneStop);

    conn->setPortName(m_serialPort);
    conn->setBaudRate(m_baudrate);

    return conn->open(QIODevice::ReadWrite);
}

bool CandleConnection::openTcpIp() {
    m_connImpl = new QTcpSocket(parent());
    auto conn = static_cast<QTcpSocket*>(m_connImpl);

//    connect(conn, &QIODevice::readyRead, this, &CandleConnection::tcpReadyRead);
    connect(conn, SIGNAL(readyRead()), parent(), SLOT(onCommReadyRead()), Qt::QueuedConnection);

    connect(conn, &QAbstractSocket::errorOccurred, this, &CandleConnection::tcpConnError);

    qDebug() << "CandleConnection::openTcpIp() connecting to:" << m_tcpHost << ":" << m_tcpPort;
    conn->connectToHost(m_tcpHost, m_tcpPort, QIODevice::ReadWrite, QAbstractSocket::IPv4Protocol);

    if (!conn->waitForConnected(100)) {
        qDebug() << "Connection timeout" << conn->error() <<  conn->errorString();
    }

    qDebug() << "CandleConnection::openTcpIp() connection state:" << conn->state();

    if(conn->state() != QAbstractSocket::ConnectedState) {
        m_connImpl->close();
        delete m_connImpl;
        m_connImpl =  nullptr;
        return false;
    }

    return true;
}

void CandleConnection::close() {

    if(m_connType == CONN_SERIAL) {
        closeSerial();
    } else
    if(m_connType == CONN_TCPIP) {
        closeTcpIp();
    }

    m_connImpl->close();
    delete m_connImpl;
    m_connImpl = nullptr;
}

void CandleConnection::closeSerial() {
}

void CandleConnection::closeTcpIp() {

}

QString CandleConnection::errorString() {
   return m_connImpl->errorString();
}

void CandleConnection::tcpConnError(QAbstractSocket::SocketError socketError) {
    auto conn = static_cast<QTcpSocket*>(m_connImpl);

    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << tr("The host was not found. Please check the "
                                    "host name and port settings.");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << tr("The connection was refused by the peer. "
                                    "Make sure the server is running, "
                                    "and check that the host name and port "
                                    "settings are correct.");
        break;
    default:
        qDebug() << tr("The following error occurred: %1.") << conn->errorString();
    }
}

void CandleConnection::tcpReadyRead() {
    qDebug() << "CandleConnection::tcpReadyRead()";
}

void CandleConnection::registerReadHandler(QObject* obj, const char* method) {
}

void CandleConnection::registerErrorHandler(QObject* obj, const char* method) {
}
