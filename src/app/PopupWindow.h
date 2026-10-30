#pragma once

#include <QStringList>
#include <QWidget>

class QLineEdit;
class QListWidget;

namespace usnip {

// Frameless list of snippet titles shown near the mouse cursor.
// A Qt::Tool window rather than Qt::Popup: on macOS a Qt popup never becomes the key window,
// and with no other window of ours to forward keys from, typing would go nowhere.
// Typing filters, Up/Down move, Return chooses, Esc closes, 1-9 choose directly while the
// filter is empty.
class PopupWindow : public QWidget {
    Q_OBJECT

public:
    explicit PopupWindow(QWidget *parent = nullptr);

    void setTitles(const QStringList &titles);
    void showNear(const QPoint &globalPos);

signals:
    // index refers to the list given to setTitles(), not to the filtered rows.
    void chosen(int index);
    // Esc closed the popup. Not emitted when it hides because another app took the focus.
    void dismissed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyFilter(const QString &text);
    void chooseRow(int row);
    void moveSelection(int delta);

    QLineEdit *m_filter;
    QListWidget *m_list;
};

} // namespace usnip
