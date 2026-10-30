#include "PopupWindow.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>

namespace usnip {

namespace {

constexpr int kWidth = 320;
constexpr int kVisibleRows = 10;

} // namespace

PopupWindow::PopupWindow(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint), m_filter(new QLineEdit(this)),
      m_list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    layout->addWidget(m_filter);
    layout->addWidget(m_list);

    m_filter->setPlaceholderText(tr("Filter"));
    m_filter->installEventFilter(this);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setUniformItemSizes(true);

    connect(m_filter, &QLineEdit::textChanged, this, &PopupWindow::applyFilter);
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) { chooseRow(m_list->row(item)); });
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { chooseRow(m_list->row(item)); });
}

void PopupWindow::setTitles(const QStringList &titles)
{
    m_list->clear();
    for (int i = 0; i < titles.size(); ++i) {
        auto *item = new QListWidgetItem(titles.at(i), m_list);
        item->setData(Qt::UserRole, i);
    }
    m_filter->clear();
    applyFilter(QString());
}

void PopupWindow::showNear(const QPoint &globalPos)
{
    const int rowHeight = std::max(m_list->sizeHintForRow(0), fontMetrics().height() + 4);
    const int rows = std::clamp(m_list->count(), 1, kVisibleRows);
    resize(kWidth, m_filter->sizeHint().height() + rows * rowHeight + 24);

    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect area = screen->availableGeometry();
    const int x = std::clamp(globalPos.x(), area.left(), area.right() - width() + 1);
    const int y = std::clamp(globalPos.y(), area.top(), area.bottom() - height() + 1);
    move(x, y);

    m_choosing = false;
    show();
    raise();
    activateWindow();
    m_filter->setFocus();
}

bool PopupWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_filter || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    const auto *key = static_cast<QKeyEvent *>(event);
    switch (key->key()) {
    case Qt::Key_Up:
        moveSelection(-1);
        return true;
    case Qt::Key_Down:
        moveSelection(1);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        chooseRow(m_list->currentRow());
        return true;
    case Qt::Key_Escape:
        hide();
        return true;
    default:
        break;
    }
    if (m_filter->text().isEmpty() && key->modifiers() == Qt::NoModifier &&
        key->key() >= Qt::Key_1 && key->key() <= Qt::Key_9) {
        chooseRow(key->key() - Qt::Key_1);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void PopupWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (!m_choosing) {
        emit dismissed();
    }
}

void PopupWindow::applyFilter(const QString &text)
{
    int firstVisible = -1;
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        const bool match = item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
        if (match && firstVisible < 0) {
            firstVisible = row;
        }
    }
    m_list->setCurrentRow(firstVisible);
}

void PopupWindow::chooseRow(int row)
{
    QListWidgetItem *item = m_list->item(row);
    if (!item || item->isHidden()) {
        return;
    }
    m_choosing = true;
    hide();
    emit chosen(item->data(Qt::UserRole).toInt());
}

void PopupWindow::moveSelection(int delta)
{
    int row = m_list->currentRow();
    for (int i = 0; i < m_list->count(); ++i) {
        row += delta;
        if (row < 0 || row >= m_list->count()) {
            return;
        }
        if (!m_list->item(row)->isHidden()) {
            m_list->setCurrentRow(row);
            return;
        }
    }
}

} // namespace usnip
