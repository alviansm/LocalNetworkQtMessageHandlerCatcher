#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QHostAddress>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButtonConnect_clicked();
    void on_lineEditIPAddressInput_textEdited(const QString &arg1);
    void on_pushButton_Clear_clicked();
    void on_lineEditPort_textEdited(const QString &arg1);

    // TCP socket signals
    void onConnected();
    void onDisconnected();
    void onDataReady();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    void setStatus(const QString &text, const QString &color = "black");
    void updateConnectButton();
    void appendLog(const QString &line, const QString &fromAddr);

    Ui::MainWindow *ui;

    QTcpSocket  *m_socket      = nullptr;

    // Staging values — populated live as the user types
    quint16      m_pendingPort = 0;
    QHostAddress m_pendingAddr;
};

#endif // MAINWINDOW_H
