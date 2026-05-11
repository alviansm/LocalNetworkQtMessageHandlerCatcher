#include "CommandListDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QModelIndex>
#include <QItemSelectionModel>

CommandListDialog::CommandListDialog(const QStringList &commands,
                                     bool connected,
                                     QWidget *parent)
    : QDialog(parent)
    , m_connected(connected)
{
    setWindowTitle("Command List");
    setMinimumSize(480, 380);
    setAttribute(Qt::WA_DeleteOnClose, false); // caller reads commands() after exec()

    m_model = new QStringListModel(commands, this);
    setupUi();
    syncButtonStates();
}

// ── UI construction ───────────────────────────────────────────────────────────

void CommandListDialog::setupUi()
{
    // ── List view ─────────────────────────────────────────────────────────────
    m_view = new QListView(this);
    m_view->setModel(m_model);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked |
                            QAbstractItemView::EditKeyPressed);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setDragDropMode(QAbstractItemView::NoDragDrop);

    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &CommandListDialog::onSelectionChanged);

    // ── Side button column ────────────────────────────────────────────────────
    m_btnMoveUp   = new QPushButton("▲  Move Up",   this);
    m_btnMoveDown = new QPushButton("▼  Move Down", this);
    m_btnSend     = new QPushButton("⮕  Send",      this);
    m_btnRemove   = new QPushButton("✕  Remove",    this);
    m_btnClose    = new QPushButton("Close",         this);

    m_btnSend->setEnabled(false);   // needs selection + connection
    m_btnMoveUp->setEnabled(false);
    m_btnMoveDown->setEnabled(false);
    m_btnRemove->setEnabled(false);

    // Style the Send button to stand out, and Remove to signal danger
    m_btnSend->setStyleSheet("QPushButton { font-weight: bold; }");
    m_btnRemove->setStyleSheet("QPushButton { color: #cc0000; }");

    auto *sideLayout = new QVBoxLayout;
    sideLayout->addWidget(m_btnMoveUp);
    sideLayout->addWidget(m_btnMoveDown);
    sideLayout->addSpacing(12);
    sideLayout->addWidget(m_btnSend);
    sideLayout->addSpacing(12);
    sideLayout->addWidget(m_btnRemove);
    sideLayout->addStretch();
    sideLayout->addWidget(m_btnClose);

    // ── Hint label ────────────────────────────────────────────────────────────
    auto *hint = new QLabel("Double-click a row to edit it in place.", this);
    hint->setStyleSheet("color: gray; font-size: 11px;");

    // ── Root layout ───────────────────────────────────────────────────────────
    auto *bodyLayout = new QHBoxLayout;
    bodyLayout->addWidget(m_view, 1);
    bodyLayout->addLayout(sideLayout);

    auto *root = new QVBoxLayout(this);
    root->addWidget(hint);
    root->addLayout(bodyLayout);

    // ── Signal connections ────────────────────────────────────────────────────
    connect(m_btnMoveUp,   &QPushButton::clicked, this, &CommandListDialog::onMoveUp);
    connect(m_btnMoveDown, &QPushButton::clicked, this, &CommandListDialog::onMoveDown);
    connect(m_btnSend,     &QPushButton::clicked, this, &CommandListDialog::onSend);
    connect(m_btnRemove,   &QPushButton::clicked, this, &CommandListDialog::onRemove);
    connect(m_btnClose,    &QPushButton::clicked, this, &QDialog::accept);
}

// ── Public accessor ───────────────────────────────────────────────────────────

QStringList CommandListDialog::commands() const
{
    return m_model->stringList();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void CommandListDialog::syncButtonStates()
{
    const QModelIndex idx  = m_view->currentIndex();
    const bool hasSelection = idx.isValid();
    const int  row          = hasSelection ? idx.row() : -1;
    const int  count        = m_model->rowCount();

    m_btnMoveUp->setEnabled(hasSelection && row > 0);
    m_btnMoveDown->setEnabled(hasSelection && row < count - 1);
    m_btnRemove->setEnabled(hasSelection);
    m_btnSend->setEnabled(hasSelection && m_connected
                          && !m_model->data(idx).toString().trimmed().isEmpty());
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void CommandListDialog::onSelectionChanged()
{
    syncButtonStates();
}

void CommandListDialog::onMoveUp()
{
    const QModelIndex idx = m_view->currentIndex();
    if (!idx.isValid() || idx.row() == 0)
        return;

    const int row = idx.row();
    const QStringList list = m_model->stringList();

    QStringList updated = list;
    updated.swapItemsAt(row, row - 1);
    m_model->setStringList(updated);

    // Restore selection on the moved item
    const QModelIndex newIdx = m_model->index(row - 1);
    m_view->setCurrentIndex(newIdx);
    m_view->scrollTo(newIdx);
    syncButtonStates();
}

void CommandListDialog::onMoveDown()
{
    const QModelIndex idx = m_view->currentIndex();
    if (!idx.isValid() || idx.row() >= m_model->rowCount() - 1)
        return;

    const int row = idx.row();
    QStringList updated = m_model->stringList();
    updated.swapItemsAt(row, row + 1);
    m_model->setStringList(updated);

    const QModelIndex newIdx = m_model->index(row + 1);
    m_view->setCurrentIndex(newIdx);
    m_view->scrollTo(newIdx);
    syncButtonStates();
}

void CommandListDialog::onRemove()
{
    const QModelIndex idx = m_view->currentIndex();
    if (!idx.isValid())
        return;

    m_model->removeRow(idx.row());

    // Try to keep a selection nearby
    const int newRow = qMin(idx.row(), m_model->rowCount() - 1);
    if (newRow >= 0)
        m_view->setCurrentIndex(m_model->index(newRow));

    syncButtonStates();
}

void CommandListDialog::onSend()
{
    const QModelIndex idx = m_view->currentIndex();
    if (!idx.isValid())
        return;

    const QString cmd = m_model->data(idx).toString().trimmed();
    if (cmd.isEmpty())
        return;

    emit sendRequested(cmd);
}

#include "CommandListDialog.moc"
