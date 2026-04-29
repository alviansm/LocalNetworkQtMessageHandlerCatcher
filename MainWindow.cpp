#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QDateTime>
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

    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead,
            this,     &MainWindow::onDatagramReceived);
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

bool MainWindow::validateInputs(quint16 &outPort, QHostAddress &outAddr)
{
    // ── Port ──────────────────────────────────────────────────────────────────
    const QString portText = ui->lineEditPort->text().trimmed();
    if (portText.isEmpty()) {
        QMessageBox::warning(this, "Missing port",
                             "Please enter a port number (e.g. 45000).");
        ui->lineEditPort->setFocus();
        return false;
    }
    bool portOk = false;
    uint portVal = portText.toUInt(&portOk);
    if (!portOk || portVal == 0 || portVal > 65535) {
        QMessageBox::warning(this, "Invalid port",
                             QString("'%1' is not a valid port number (1-65535).").arg(portText));
        ui->lineEditPort->setFocus();
        return false;
    }

    // ── IP address ────────────────────────────────────────────────────────────
    const QString ipText = ui->lineEditIPAddressInput->text().trimmed();
    if (ipText.isEmpty()) {
        QMessageBox::warning(this, "Missing IP address",
                             "Please enter the IPv4 address of the broadcasting device.");
        ui->lineEditIPAddressInput->setFocus();
        return false;
    }
    QHostAddress addr(ipText);
    if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv4Protocol) {
        QMessageBox::warning(this, "Invalid IP address",
                             QString("'%1' is not a valid IPv4 address.").arg(ipText));
        ui->lineEditIPAddressInput->setFocus();
        return false;
    }

    outPort = static_cast<quint16>(portVal);
    outAddr = addr;
    return true;
}

void MainWindow::bindSocket(quint16 port, const QHostAddress &senderFilter)
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->close();
        setStatus("Disconnecting...", "gray");
    }

    // Bind to AnyIPv4 so we receive broadcast packets on any interface.
    // The senderFilter is applied in onDatagramReceived to only show
    // traffic coming from the requested source IP.
    bool ok = m_socket->bind(QHostAddress::AnyIPv4, port,
                             QUdpSocket::ShareAddress |
                                 QUdpSocket::ReuseAddressHint);
    if (ok) {
        m_port         = port;
        m_senderFilter = senderFilter;
        setStatus(QString("Listening on port %1 — filtering for %2")
                      .arg(port)
                      .arg(senderFilter.toString()),
                  "green");
        ui->pushButtonConnect->setText("Disconnect");
    } else {
        const QString err = m_socket->errorString();
        setStatus(QString("Bind failed: %1").arg(err), "red");
        QMessageBox::critical(this, "Connection failed",
                              QString("Could not bind to port %1:\n%2").arg(port).arg(err));
    }
}

// ── slots ─────────────────────────────────────────────────────────────────────

void MainWindow::on_pushButtonConnect_clicked()
{
    // Toggle: if already bound, disconnect
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->close();
        m_senderFilter.clear();
        setStatus("Disconnected", "gray");
        ui->pushButtonConnect->setText("Connect");
        return;
    }

    quint16 port;
    QHostAddress addr;
    if (!validateInputs(port, addr))
        return;

    bindSocket(port, addr);
}

void MainWindow::on_lineEditIPAddressInput_textEdited(const QString &arg1)
{
    Q_UNUSED(arg1)
}

void MainWindow::on_lineEditPort_textEdited(const QString &arg1)
{
    Q_UNUSED(arg1)
}

void MainWindow::on_pushButton_Clear_clicked()
{
    ui->textEdit->clear();
}

void MainWindow::onDatagramReceived()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dg = m_socket->receiveDatagram();
        if (dg.isNull())
            continue;

        // Only accept packets from the requested sender IP
        if (!m_senderFilter.isNull() &&
            dg.senderAddress().toIPv4Address() != m_senderFilter.toIPv4Address())
            continue;

        const QString raw = QString::fromUtf8(dg.data()).trimmed();
        if (raw.isEmpty())
            continue;

        const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString sender = dg.senderAddress().toString()
            + ":"
                + QString::number(dg.senderPort());

            const QString color   = colorForLine(line);
            const QString escaped = line.toHtmlEscaped();
            const QString html    = QString(
                                     "<span style=\"color:%1; font-family:monospace;\">%2</span>"
                                     "<span style=\"color:#888888; font-size:small;\"> &nbsp;[from %3]</span>")
                                     .arg(color, escaped, sender);

            ui->textEdit->append(html);
        }

        // Auto-scroll to the bottom
        QScrollBar *sb = ui->textEdit->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    const QString err = m_socket->errorString();
    setStatus(QString("Socket error: %1").arg(err), "red");
    QMessageBox::warning(this, "Socket error",
                         QString("The UDP socket reported an error:\n%1").arg(err));
    ui->pushButtonConnect->setText("Connect");
}
