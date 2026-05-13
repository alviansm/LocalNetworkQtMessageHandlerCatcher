#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QHostAddress>
#include <QSettings>
#include <QFile>
#include <QTimer>
#include <QTextCursor>
#include <QList>

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

protected:
    void closeEvent(QCloseEvent *event) override;

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

    void on_pushButtonSend_clicked();
    void on_comboBox_editTextChanged(const QString &arg1);
    void on_pushButton_CmdList_clicked();
    void on_pushButtonSave_clicked();
    void on_pushButtonAutoConnect_clicked();

    // Auto-connect retry
    void onAutoConnectTimer();

    // Auto-scroll
    void onScrollBarValueChanged(int value);

    void on_pushButtonSearchLog_clicked();

private:
    void setStatus(const QString &text, const QString &color = "black");
    void updateConnectButton();
    void updateSendButton();
    void stopAutoConnect();
    void appendLog(const QString &line, const QString &fromAddr);

    // QSettings helpers
    void loadSettings();
    void saveSettings();
    void saveCommandHistory(const QString &command);

    // Shared send — used by both pushButtonSend and the dialog's sendRequested
    void sendCommand(const QString &command);

    // Rebuild comboBox items from a new ordered list (after dialog edits)
    void applyCommandList(const QStringList &commands);

    Ui::MainWindow *ui;

    QTcpSocket  *m_socket      = nullptr;
    QSettings   *m_settings    = nullptr;

    // Staging values — populated live as the user types
    quint16      m_pendingPort = 0;
    QHostAddress m_pendingAddr;

    // Auto-scroll: true when the scrollbar is pinned to the bottom
    bool         m_autoScroll      = true;

    // Auto-connect
    bool         m_autoConnecting  = false;
    QTimer      *m_autoConnectTimer = nullptr;
    static constexpr int kAutoConnectIntervalMs = 3000;

    static constexpr int kMaxCommandHistory = 20;

    // Log search state
    QString           m_lastSearchTerm;
    QList<QTextCursor> m_searchMatches;
    int               m_searchIndex = -1;
};

#endif // MAINWINDOW_H
