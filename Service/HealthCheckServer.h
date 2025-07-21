#ifndef HEALTHCHECKSERVER_H
#define HEALTHCHECKSERVER_H

#include <QObject>
#include <QTcpServer>

// 前向声明 Service 类以避免循环引用
class Service;

class HealthCheckServer : public QObject
{
    Q_OBJECT
public:
    // 构造函数接收一个 Service 指针，以便访问其状态
    explicit HealthCheckServer(Service* service, QObject *parent = nullptr);
    ~HealthCheckServer();

    bool startListen(quint16 port);
    void stopListen();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpServer* m_server;
    Service* m_service; // 指向核心服务类的指针

    std::string getHealthStatus(); // 获取健康状态的核心函数
};

#endif // HEALTHCHECKSERVER_H
