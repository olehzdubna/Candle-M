#ifndef CANDLECONNECTION_H
#define CANDLECONNECTION_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QAbstractSocket>

class CandleConnection : public QObject
{
//    Q_OBJECT
public:
    typedef enum {CONN_NA, CONN_SERIAL, CONN_TCPIP} Type;
public:
    CandleConnection(QObject *parent=nullptr);
    void setConnType(Type connType)
    {m_connType = connType;}
    void setPortName(const QString& sPort)
    {m_serialPort = sPort;}
    const QString& portName()
    {return m_serialPort;}
    void setBaudRate(int baud)
    {m_baudrate = baud;}
    int baudRate()
    {return m_baudrate;}
    const QString& tcpHost()
    {return m_tcpHost;}
    void setTcpPort(uint16_t port)
    {m_tcpPort = port;}
    void setTcpHost(const QString& h)
    {m_tcpHost = h;}
    uint16_t tcpPort()
    {return m_tcpPort;}
    bool openPort();
    int write(const char* data, int len);
    int write(const QByteArray& arr);
    int write(const QString& str);
    bool canReadLine();
    QByteArray readLine();
    bool isOpen();
    void close();
    void registerReadHandler(QObject* obj, const char* method);
    void registerErrorHandler(QObject* obj, const char* method);
    QString errorString();
private:
    bool openSerial();
    bool openTcpIp();
    void closeSerial();
    void closeTcpIp();
signals:
    void tcpReadyRead();
    void tcpConnError(QAbstractSocket::SocketError socketError);
private:
    QIODevice* m_connImpl;
    Type m_connType;
    uint16_t m_tcpPort;
    QString  m_tcpHost;
    QString  m_serialPort;
    int m_baudrate;
};

#endif // CANDLECONNECTION_H
