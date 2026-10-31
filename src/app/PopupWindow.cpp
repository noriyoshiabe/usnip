#include "PopupWindow.h"

#include "platform/Panel.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
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

// Draws the Cmd+1-9 key hint, faint and right aligned, eliding the title so the two never overlap.
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

QRect availableArea(const QPoint &globalPos)
{
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen->availableGeometry();
}

} // namespace

PopupWindow::PopupWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint), m_filter(new QLineEdit(this)),
      m_list(new QListWidget(this)), m_emptyHint(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    layout->addWidget(m_filter);
    layout->addWidget(m_list);
    layout->addWidget(m_emptyHint);

    m_filter->setPlaceholderText(tr("Filter"));
    m_filter->installEventFilter(this);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setUniformItemSizes(true);
    m_list->setItemDelegate(new KeyHintDelegate(m_list));
    m_emptyHint->setText(tr("No snippets. Add them with Edit… in the menu bar."));
    m_emptyHint->setEnabled(false);

    connect(m_filter, &QLineEdit::textChanged, this, &PopupWindow::applyFilter);
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) { chooseRow(m_list->row(item)); });
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { chooseRow(m_list->row(item)); });

    platform::makeNonactivatingPanel(this);
    // Clicking another window takes the key status away. That window already has the focus,
    // so only hide. Qt's ActivationChange cannot be used: Qt keeps reporting this window as
    // active after another app takes over, and the app itself is never activated.
    platform::onResignKey(this, [this] { hide(); });
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
    // The window grows downwards as matches appear, so keep room for the tallest list.
    const QRect area = availableArea(globalPos);
    const int x = std::clamp(globalPos.x(), area.left(), area.right() - width() + 1);
    const int y = std::clamp(globalPos.y(), area.top(), area.bottom() - maxHeight() + 1);
    showAt({x, y});
}

void PopupWindow::showCentered(const QPoint &globalPos)
{
    // Centre the tallest list rather than the current window, so the filter stays put while
    // matches appear below it.
    const QRect area = availableArea(globalPos);
    showAt({area.left() + (area.width() - width()) / 2,
            area.top() + std::max(0, (area.height() - maxHeight()) / 2)});
}

void PopupWindow::showAt(const QPoint &topLeft)
{
    move(topLeft);
    useStrongSelection(m_list);
    platform::showAsKeyPanel(this);
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
    // Qt maps Cmd to Qt::ControlModifier on macOS. Plain digits are left to the filter, since
    // titles may contain them.
    if (key->modifiers() == Qt::ControlModifier && key->key() >= Qt::Key_1 &&
        key->key() < Qt::Key_1 + kDirectChoices) {
        chooseRow(visibleRow(key->key() - Qt::Key_1));
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void PopupWindow::applyFilter(const QString &text)
{
    // Every word must appear in the title, in any order. simplified() also turns the full-width
    // space a Japanese input method types into a plain one.
    const QStringList words = text.simplified().split(u' ', Qt::SkipEmptyParts);
    int matches = 0;
    int firstVisible = -1;
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        const QString title = item->text();
        const bool match =
            !words.isEmpty() && std::ranges::all_of(words, [&title](const QString &word) {
                return title.contains(word, Qt::CaseInsensitive);
            });
        item->setHidden(!match);
        const bool direct = match && matches < kDirectChoices;
        item->setData(kHintRole, direct ? QStringLiteral("⌘%1").arg(matches + 1) : QString());
        if (match) {
            if (firstVisible < 0) {
                firstVisible = row;
            }
            ++matches;
        }
    }
    m_list->setCurrentRow(firstVisible);
    updateHeight(matches);
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

int PopupWindow::maxHeight() const
{
    const QMargins margins = layout()->contentsMargins();
    return margins.top() + m_filter->sizeHint().height() + layout()->spacing() +
           kVisibleRows * rowHeight() + 2 * m_list->frameWidth() + margins.bottom();
}

int PopupWindow::rowHeight() const
{
    return std::max(m_list->sizeHintForRow(0), fontMetrics().height() + 4);
}

// Returns the row of the n-th visible item, or -1.
int PopupWindow::visibleRow(int n) const
{
    for (int row = 0; row < m_list->count(); ++row) {
        if (!m_list->item(row)->isHidden() && n-- == 0) {
            return row;
        }
    }
    return -1;
}

void PopupWindow::updateHeight(int matches)
{
    m_list->setVisible(matches > 0);
    m_list->setFixedHeight(std::min(matches, kVisibleRows) * rowHeight() +
                           2 * m_list->frameWidth());
    m_emptyHint->setVisible(m_list->count() == 0);
    layout()->activate();
    resize(kWidth, sizeHint().height());
}

} // namespace usnip
