#include "PopupWindow.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>

namespace usnip {

namespace {

constexpr int kWidth = 320;
constexpr int kVisibleRows = 10;
constexpr int kDirectChoices = 9;
constexpr int kHintRole = Qt::UserRole + 1;

// Draws the 1-9 key hint, faint and right aligned, eliding the title so the two never overlap.
class KeyHintDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QString hint = index.data(kHintRole).toString();
        if (hint.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const int margin = 8;
        const int hintWidth = opt.fontMetrics.horizontalAdvance(hint) + 2 * margin;
        opt.text = opt.fontMetrics.elidedText(opt.text, Qt::ElideRight,
                                              opt.rect.width() - hintWidth - margin);
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        const bool selected = opt.state & QStyle::State_Selected;
        QColor color = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        color.setAlphaF(0.45f);
        painter->save();
        painter->setPen(color);
        painter->drawText(opt.rect.adjusted(0, 0, -margin, 0), Qt::AlignRight | Qt::AlignVCenter,
                          hint);
        painter->restore();
    }
};

// The list never takes the focus, so macOS draws its selection in the faint inactive color.
// Paint it in a color close to the text color instead: near black on a light appearance,
// near white on a dark one. Read from the application palette so it follows the appearance.
void useStrongSelection(QListWidget *list)
{
    QPalette palette = QApplication::palette(list);
    const QColor text = palette.color(QPalette::Active, QPalette::Text);
    const QColor base = palette.color(QPalette::Active, QPalette::Base);
    const auto mix = [](int a, int b) { return (a * 4 + b) / 5; };
    const QColor highlight(mix(text.red(), base.red()), mix(text.green(), base.green()),
                           mix(text.blue(), base.blue()));
    for (const auto group : {QPalette::Active, QPalette::Inactive}) {
        palette.setColor(group, QPalette::Highlight, highlight);
        palette.setColor(group, QPalette::HighlightedText, base);
    }
    list->setPalette(palette);
}

} // namespace

PopupWindow::PopupWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint), m_filter(new QLineEdit(this)),
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
    m_list->setItemDelegate(new KeyHintDelegate(m_list));

    connect(m_filter, &QLineEdit::textChanged, this, &PopupWindow::applyFilter);
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) { chooseRow(m_list->row(item)); });
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { chooseRow(m_list->row(item)); });

    // Clicking another app deactivates us. That app already has the focus, so only hide; Esc
    // is what hands the focus back. Qt's per-window ActivationChange cannot be used: when
    // another app takes over, Qt still reports this Qt::Tool window as active and visible.
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state != Qt::ApplicationActive) {
                    hide();
                }
            });
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

    useStrongSelection(m_list);
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
        emit dismissed();
        return true;
    default:
        break;
    }
    if (m_filter->text().isEmpty() && key->modifiers() == Qt::NoModifier &&
        key->key() >= Qt::Key_1 && key->key() < Qt::Key_1 + kDirectChoices) {
        chooseRow(key->key() - Qt::Key_1);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void PopupWindow::applyFilter(const QString &text)
{
    int firstVisible = -1;
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        const bool match = item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
        const bool direct = text.isEmpty() && row < kDirectChoices;
        item->setData(kHintRole, direct ? QString::number(row + 1) : QString());
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
