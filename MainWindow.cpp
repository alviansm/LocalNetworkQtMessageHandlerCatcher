#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "CommandListDialog.h"

#include <QHostAddress>
#include <QScrollBar>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QCloseEvent>

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

    // ── QSettings: .ini file next to the binary ───────────────────────────────
    const QString iniPath = QCoreApplication::applicationDirPath()
                            + QDir::separator()
                            + "catcher.ini";
    m_settings = new QSettings(iniPath, QSettings::IniFormat, this);

    ui->lineEditPort->setPlaceholderText("e.g. 45000");
    ui->lineEditIPAddressInput->setPlaceholderText("e.g. 192.168.1.100");
    ui->pushButtonConnect->setEnabled(false);
    ui->pushButtonSend->setEnabled(false);
    ui->comboBox->setInsertPolicy(QComboBox::NoInsert); // we manage insertion manually

    // ── TCP socket ────────────────────────────────────────────────────────────
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected,          this, &MainWindow::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,       this, &MainWindow::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,          this, &MainWindow::onDataReady);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &MainWindow::onSocketError);

    setStatus("Idle — enter IP and port, then click Connect", "gray");

    // ── Restore saved state ───────────────────────────────────────────────────
    loadSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

// ── QSettings ────────────────────────────────────────────────────────────────

void MainWindow::loadSettings()
{
    ui->lineEditPort->setText(m_settings->value("connection/port").toString());
    ui->lineEditIPAddressInput->setText(m_settings->value("connection/ip").toString());

    const int count = m_settings->beginReadArray("commands");
    for (int i = 0; i < count; ++i) {
        m_settings->setArrayIndex(i);
        const QString cmd = m_settings->value("text").toString();
        if (!cmd.isEmpty())
            ui->comboBox->addItem(cmd);
    }
    m_settings->endArray();

    if (ui->comboBox->count() > 0)
        ui->comboBox->setCurrentIndex(0);

    updateConnectButton();
    updateSendButton();
}

void MainWindow::saveSettings()
{
    m_settings->setValue("connection/port", ui->lineEditPort->text().trimmed());
    m_settings->setValue("connection/ip",   ui->lineEditIPAddressInput->text().trimmed());

    m_settings->beginWriteArray("commands");
    for (int i = 0; i < ui->comboBox->count(); ++i) {
        m_settings->setArrayIndex(i);
        m_settings->setValue("text", ui->comboBox->itemText(i));
    }
    m_settings->endArray();

    m_settings->sync();
}

void MainWindow::saveCommandHistory(const QString &command)
{
    if (command.isEmpty())
        return;

    const int existing = ui->comboBox->findText(command);
    if (existing != -1)
        ui->comboBox->removeItem(existing);

    ui->comboBox->insertItem(0, command);
    ui->comboBox->setCurrentIndex(0);

    while (ui->comboBox->count() > kMaxCommandHistory)
        ui->comboBox->removeItem(ui->comboBox->count() - 1);

    saveSettings();
}

// Replaces the comboBox contents entirely with an externally supplied list
// (e.g. after the user reorders/edits/deletes entries in CommandListDialog).
void MainWindow::applyCommandList(const QStringList &commands)
{
    // Block signals briefly so editTextChanged doesn't fire for every removeItem
    ui->comboBox->blockSignals(true);
    ui->comboBox->clear();
    for (const QString &cmd : commands)
        ui->comboBox->addItem(cmd);
    if (ui->comboBox->count() > 0)
        ui->comboBox->setCurrentIndex(0);
    ui->comboBox->blockSignals(false);

    updateSendButton();
    saveSettings();
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
    bool portOk = false;
    const uint portVal = ui->lineEditPort->text().trimmed().toUInt(&portOk);
    portOk = portOk && portVal > 0 && portVal <= 65535;
    if (portOk)
        m_pendingPort = static_cast<quint16>(portVal);

    const QHostAddress addr(ui->lineEditIPAddressInput->text().trimmed());
    const bool addrOk = !addr.isNull()
                        && addr.protocol() == QAbstractSocket::IPv4Protocol;
    if (addrOk)
        m_pendingAddr = addr;

    const bool disconnected = m_socket->state() == QAbstractSocket::UnconnectedState;
    ui->pushButtonConnect->setEnabled(disconnected ? (portOk && addrOk) : true);
}

void MainWindow::updateSendButton()
{
    const bool connected = m_socket->state() == QAbstractSocket::ConnectedState;
    const bool hasText   = !ui->comboBox->currentText().trimmed().isEmpty();
    ui->pushButtonSend->setEnabled(connected && hasText);
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

// Shared send logic used by both the main Send button and the dialog's Send.
void MainWindow::sendCommand(const QString &command)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState || command.isEmpty())
        return;

    const QByteArray data = (command + "\n").toUtf8();
    const qint64 written  = m_socket->write(data);

    if (written == -1) {
        QMessageBox::warning(this, "Send failed",
                             QString("Failed to send command:\n%1")
                                 .arg(m_socket->errorString()));
        return;
    }

    const QString peer = m_socket->peerAddress().toString()
                         + ":"
                         + QString::number(m_socket->peerPort());
    const QString echo = QString(
                             "<span style=\"color:#555555; font-family:monospace; font-style:italic;\">"
                             "&gt; %1</span>"
                             "<span style=\"color:#888888; font-size:small;\"> &nbsp;[sent to %2]</span>")
                             .arg(command.toHtmlEscaped(), peer);
    ui->textEdit->append(echo);

    QScrollBar *sb = ui->textEdit->verticalScrollBar();
    sb->setValue(sb->maximum());

    saveCommandHistory(command);
}

// ── slots — input fields ──────────────────────────────────────────────────────

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

void MainWindow::on_comboBox_editTextChanged(const QString &arg1)
{
    Q_UNUSED(arg1)
    updateSendButton();
}

// ── slots — buttons ───────────────────────────────────────────────────────────

void MainWindow::on_pushButtonConnect_clicked()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
        return;
    }

    ui->lineEditPort->setEnabled(false);
    ui->lineEditIPAddressInput->setEnabled(false);
    ui->pushButtonConnect->setEnabled(false);
    setStatus(QString("Connecting to %1:%2 …")
                  .arg(m_pendingAddr.toString())
                  .arg(m_pendingPort),
              "orange");

    m_socket->connectToHost(m_pendingAddr, m_pendingPort);
}

void MainWindow::on_pushButtonSend_clicked()
{
    sendCommand(ui->comboBox->currentText().trimmed());
}

void MainWindow::on_pushButton_Clear_clicked()
{
    ui->textEdit->clear();
}

void MainWindow::on_pushButton_CmdList_clicked()
{
    // Collect current comboBox items as the starting list for the dialog
    QStringList currentList;
    for (int i = 0; i < ui->comboBox->count(); ++i)
        currentList << ui->comboBox->itemText(i);

    const bool connected = m_socket->state() == QAbstractSocket::ConnectedState;

    CommandListDialog dlg(currentList, connected, this);

    // Wire the dialog's Send button directly to our shared sendCommand()
    connect(&dlg, &CommandListDialog::sendRequested,
            this, &MainWindow::sendCommand);

    dlg.exec(); // blocks until the user clicks Close

    // Whether the user sent things or just reordered/edited, sync back the list
    const QStringList updated = dlg.commands();
    if (updated != currentList)
        applyCommandList(updated);
}

// ── TCP socket signal handlers ────────────────────────────────────────────────

void MainWindow::onConnected()
{
    const QString peer = m_socket->peerAddress().toString()
    + ":"
        + QString::number(m_socket->peerPort());
    setStatus(QString("Connected to %1").arg(peer), "green");
    ui->pushButtonConnect->setText("Disconnect");
    ui->pushButtonConnect->setEnabled(true);
    updateSendButton();
}

void MainWindow::onDisconnected()
{
    setStatus("Disconnected — host closed the connection", "gray");
    ui->pushButtonConnect->setText("Connect");
    ui->lineEditPort->setEnabled(true);
    ui->lineEditIPAddressInput->setEnabled(true);
    updateConnectButton();
    updateSendButton();

    QMessageBox::information(this, "Disconnected",
                             "The host closed the connection.");
}

void MainWindow::onDataReady()
{
    const QString peer = m_socket->peerAddress().toString()
    + ":"
        + QString::number(m_socket->peerPort());

    const QString raw = QString::fromUtf8(m_socket->readAll()).trimmed();
    if (raw.isEmpty())
        return;

    const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines)
        appendLog(line, peer);
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    if (socketError == QAbstractSocket::RemoteHostClosedError)
        return;

    const QString err = m_socket->errorString();
    setStatus(QString("Error: %1").arg(err), "red");

    ui->pushButtonConnect->setText("Connect");
    ui->lineEditPort->setEnabled(true);
    ui->lineEditIPAddressInput->setEnabled(true);
    updateConnectButton();
    updateSendButton();

    QMessageBox::warning(this, "Connection error",
                         QString("Could not connect to %1:%2\n\n%3")
                             .arg(m_pendingAddr.toString())
                             .arg(m_pendingPort)
                             .arg(err));
}
