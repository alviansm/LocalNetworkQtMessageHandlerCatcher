#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QHostAddress>
#include <QScrollBar>
#include <QMessageBox>

// ── colour-coding by log level ────────────────────────────────────────────────
static QString colorForLine(const QString &line)
{
    if (line.contains("[FATAL]"))  return "#cc0000";
    if (line.contains("[ERROR]"))  return "#cc0000";
    if (line.contains("[WARN]"))   return "#cc6600";
    if (line.contains("[INFO]"))   return "#006600";
    if (line.contains("[DEBUG]"))  return "#000080";
    return "#000000";
}

// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->lineEditPort->setPlaceholderText("e.g. 45000");
    ui->lineEditIPAddressInput->setPlaceholderText("e.g. 192.168.1.100");
    ui->pushButtonConnect->setEnabled(false);

    m_socket = new QTcpSocket(this);

    // These fire only after the TCP three-way handshake completes (or fails),
    // so they are our natural acknowledgement that the host actually accepted us.
    connect(m_socket, &QTcpSocket::connected,       this, &MainWindow::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,    this, &MainWindow::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,        this, &MainWindow::onDataReady);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this,     &MainWindow::onSocketError);

    setStatus("Idle — enter IP and port, then click Connect", "gray");
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ── helpers ───────────────────────────────────────────────────────────────────

void MainWindow::setStatus(const QString &text, const QString &color)
{
    ui->labelNetworkStatus->setText(text);
    ui->labelNetworkStatus->setStyleSheet(
        QString("QLabel { color: %1; font-weight: bold; }").arg(color));
}

void MainWindow::updateConnectButton()
{
    // ── Port ──────────────────────────────────────────────────────────────────
    bool portOk = false;
    const uint portVal = ui->lineEditPort->text().trimmed().toUInt(&portOk);
    portOk = portOk && portVal > 0 && portVal <= 65535;
    if (portOk)
        m_pendingPort = static_cast<quint16>(portVal);

    // ── IP address ────────────────────────────────────────────────────────────
    const QHostAddress addr(ui->lineEditIPAddressInput->text().trimmed());
    const bool addrOk = !addr.isNull()
                        && addr.protocol() == QAbstractSocket::IPv4Protocol;
    if (addrOk)
        m_pendingAddr = addr;

    ui->pushButtonConnect->setEnabled(portOk && addrOk);
}

void MainWindow::appendLog(const QString &line, const QString &fromAddr)
{
    const QString color   = colorForLine(line);
    const QString escaped = line.toHtmlEscaped();
    const QString html    = QString(
                             "<span style=\"color:%1; font-family:monospace;\">%2</span>"
                             "<span style=\"color:#888888; font-size:small;\"> &nbsp;[from %3]</span>")
                             .arg(color, escaped, fromAddr);

    ui->textEdit->append(html);

    QScrollBar *sb = ui->textEdit->verticalScrollBar();
    sb->setValue(sb->maximum());
}

// ── slots ─────────────────────────────────────────────────────────────────────

void MainWindow::on_lineEditPort_textEdited(const QString &arg1)
{
    Q_UNUSED(arg1)
    updateConnectButton();
}

void MainWindow::on_lineEditIPAddressInput_textEdited(const QString &arg1)
{
    Q_UNUSED(arg1)
    updateConnectButton();
}

void MainWindow::on_pushButtonConnect_clicked()
{
    // Toggle: if already connected or connecting, disconnect
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort(); // immediate, no graceful FIN wait
        // onDisconnected() will handle UI reset
        return;
    }

    // Lock fields while attempting connection
    ui->lineEditPort->setEnabled(false);
    ui->lineEditIPAddressInput->setEnabled(false);
    ui->pushButtonConnect->setEnabled(false);
    setStatus(QString("Connecting to %1:%2 …")
                  .arg(m_pendingAddr.toString())
                  .arg(m_pendingPort),
              "orange");

    // connectToHost() is non-blocking — onConnected() fires when the TCP
    // handshake succeeds, onSocketError() fires if it fails.
    m_socket->connectToHost(m_pendingAddr, m_pendingPort);
}

void MainWindow::on_pushButton_Clear_clicked()
{
    ui->textEdit->clear();
}

// ── TCP socket signal handlers ────────────────────────────────────────────────

void MainWindow::onConnected()
{
    // TCP three-way handshake completed — the host acknowledged our connection.
    const QString peer = m_socket->peerAddress().toString()
                         + ":"
                         + QString::number(m_socket->peerPort());
    setStatus(QString("Connected to %1").arg(peer), "green");
    ui->pushButtonConnect->setText("Disconnect");
    ui->pushButtonConnect->setEnabled(true);
}

void MainWindow::onDisconnected()
{
    setStatus("Disconnected — host closed the connection", "gray");
    ui->pushButtonConnect->setText("Connect");
    ui->lineEditPort->setEnabled(true);
    ui->lineEditIPAddressInput->setEnabled(true);
    updateConnectButton(); // restore enabled state based on field content

    QMessageBox::information(this, "Disconnected",
                             "The host closed the connection.");
}

void MainWindow::onDataReady()
{
    const QString peer = m_socket->peerAddress().toString()
    + ":"
        + QString::number(m_socket->peerPort());

    // readAll() may contain multiple log lines separated by '\n'
    const QString raw = QString::fromUtf8(m_socket->readAll()).trimmed();
    if (raw.isEmpty())
        return;

    const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines)
        appendLog(line, peer);
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    // RemoteHostClosedError fires just before onDisconnected(), let that handle it
    if (socketError == QAbstractSocket::RemoteHostClosedError)
        return;

    const QString err = m_socket->errorString();
    setStatus(QString("Error: %1").arg(err), "red");

    ui->pushButtonConnect->setText("Connect");
    ui->lineEditPort->setEnabled(true);
    ui->lineEditIPAddressInput->setEnabled(true);
    updateConnectButton();

    QMessageBox::warning(this, "Connection error",
                         QString("Could not connect to %1:%2\n\n%3")
                             .arg(m_pendingAddr.toString())
                             .arg(m_pendingPort)
                             .arg(err));
}
