#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;

namespace usnip {

// Frameless list of snippet titles shown near the mouse cursor or at the centre of its screen.
// A Qt::Tool window rather than Qt::Popup: on macOS a Qt popup never becomes the key window,
// and with no other window of ours to forward keys from, typing would go nowhere.
// Titles stay hidden until something is typed, so work and private snippets are not shown
// side by side on screen. Typing filters, Up/Down move, Return chooses, Cmd+1-9 choose the
// first nine matches directly, Esc closes.
class PopupWindow : public QWidget {
    Q_OBJECT

public:
    explicit PopupWindow(QWidget *parent = nullptr);

    void setTitles(const QStringList &titles);
    void showNear(const QPoint &globalPos);
    // On the screen that holds globalPos.
    void showCentered(const QPoint &globalPos);

signals:
    // index refers to the list given to setTitles(), not to the filtered rows.
    void chosen(int index);
    // Esc closed the popup. Not emitted when it hides because another app took the focus.
    void dismissed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showAt(const QPoint &topLeft);
    int maxHeight() const;
    void applyFilter(const QString &text);
    void chooseRow(int row);
    void moveSelection(int delta);
    int rowHeight() const;
    int visibleRow(int n) const;
    void updateHeight(int matches);

    QLineEdit *m_filter;
    QListWidget *m_list;
    QLabel *m_emptyHint;
};

} // namespace usnip
