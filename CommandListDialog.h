#ifndef COMMANDLISTDIALOG_H
#define COMMANDLISTDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QStringListModel>

class QListView;
class QPushButton;

class CommandListDialog : public QDialog
{
    Q_OBJECT

public:
    // commands: the current ordered list coming from the comboBox / QSettings
    // connected: whether the TCP socket is currently connected (gates Send button)
    explicit CommandListDialog(const QStringList &commands,
                               bool connected,
                               QWidget *parent = nullptr);

    // Call after exec() to retrieve the (possibly reordered/edited/deleted) list
    QStringList commands() const;

signals:
    // Emitted when the user clicks Send for a specific command.
    // MainWindow connects this to write on the TCP socket.
    void sendRequested(const QString &command);

private slots:
    void onMoveUp();
    void onMoveDown();
    void onRemove();
    void onSend();
    void onSelectionChanged();

private:
    void setupUi();
    void syncButtonStates();

    QStringListModel *m_model   = nullptr;
    QListView        *m_view    = nullptr;

    QPushButton *m_btnMoveUp   = nullptr;
    QPushButton *m_btnMoveDown = nullptr;
    QPushButton *m_btnSend     = nullptr;
    QPushButton *m_btnRemove   = nullptr;
    QPushButton *m_btnClose    = nullptr;

    bool m_connected = false;
};

#endif // COMMANDLISTDIALOG_H
