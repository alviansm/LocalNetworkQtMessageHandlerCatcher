#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
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

    void onDatagramReceived();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    void setStatus(const QString &text, const QString &color = "black");
    bool validateInputs(quint16 &outPort, QHostAddress &outAddr);
    void bindSocket(quint16 port, const QHostAddress &senderFilter);

    Ui::MainWindow *ui;

    QUdpSocket  *m_socket       = nullptr;
    quint16      m_port         = 0;
    QHostAddress m_senderFilter; // Only show datagrams from this IP
};

#endif // MAINWINDOW_H
