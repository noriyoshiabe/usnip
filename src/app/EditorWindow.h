#pragma once

#include <QTimer>
#include <QWidget>

class QLineEdit;
class QListWidget;
class QPlainTextEdit;

namespace usnip {

class SnippetStore;

// Titles on the left, title and body of the selected snippet on the right.
// Open only after owner authentication: the body is shown in plain text.
//
// Changes are saved as they are made. Add, delete and reorder save at once; title and body
// edits are saved when another snippet is selected, on Cmd+S and when the window closes.
// Only the selected body is decrypted. Two minutes without input close the window, and
// closing deletes it, so the next open authenticates again.
class EditorWindow : public QWidget {
    Q_OBJECT

public:
    EditorWindow(SnippetStore &store, const QString &storePath, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void reloadList(int row);
    void showSnippet(int row);
    bool commit();
    bool save();
    void addSnippet();
    void removeSnippet();
    void applyListOrder();
    void importJson();
    int indexOf(const QString &id) const;

    SnippetStore &m_store;
    QString m_storePath;
    QListWidget *m_list;
    QLineEdit *m_title;
    QPlainTextEdit *m_body;
    QTimer m_idleTimer;
    // The snippet shown on the right, by id so that reordering the list cannot mix it up.
    QString m_shownId;
};

} // namespace usnip
