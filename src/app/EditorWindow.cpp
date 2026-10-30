#include "EditorWindow.h"

#include "PassphraseDialog.h"
#include "core/ExportFile.h"
#include "core/JsonImport.h"
#include "core/SnippetStore.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <optional>

namespace usnip {

namespace {

using namespace std::chrono_literals;

constexpr auto kIdleTimeout = 2min;
constexpr int kIdRole = Qt::UserRole;
// Enough to recognise what is being imported without the message box outgrowing the screen.
constexpr int kMaxListedTitles = 15;

QToolButton *toolButton(const QString &text, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setAutoRaise(true);
    return button;
}

} // namespace

EditorWindow::EditorWindow(SnippetStore &store, const QString &storePath, QWidget *parent)
    : QWidget(parent, Qt::Window), m_store(store), m_storePath(storePath),
      m_list(new QListWidget(this)), m_title(new QLineEdit(this)), m_body(new QPlainTextEdit(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Snippets"));
    resize(640, 400);

    auto *add = toolButton(QStringLiteral("+"), this);
    auto *remove = toolButton(QStringLiteral("−"), this);
    auto *more = toolButton(QStringLiteral("⋯"), this);
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    menu->addAction(tr("Export to .usnip…"), this, &EditorWindow::exportUsnip);
    menu->addAction(tr("Import .usnip…"), this, &EditorWindow::importUsnip);
    menu->addAction(tr("Import JSON…"), this, &EditorWindow::importJson);
    more->setMenu(menu);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(0);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    buttons->addWidget(more);

    auto *left = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(2);
    leftLayout->addWidget(m_list);
    leftLayout->addLayout(buttons);

    auto *right = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_title);
    rightLayout->addWidget(m_body);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({200, 440});
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(splitter);

    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_title->setPlaceholderText(tr("Title"));
    // Passwords are easier to check when l, 1 and I look different.
    m_body->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_body->setPlaceholderText(tr("Body"));

    connect(add, &QToolButton::clicked, this, &EditorWindow::addSnippet);
    connect(remove, &QToolButton::clicked, this, &EditorWindow::removeSnippet);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        commit();
        showSnippet(row);
    });
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, &EditorWindow::applyListOrder);
    connect(m_title, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (QListWidgetItem *item = m_list->currentItem()) {
            item->setText(text);
        }
    });
    new QShortcut(QKeySequence::Save, this, this, &EditorWindow::commit);
    new QShortcut(QKeySequence::Close, this, this, &QWidget::close);
    // Quitting does not close windows, so an edit in progress would be lost.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &EditorWindow::commit);

    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(kIdleTimeout);
    connect(&m_idleTimer, &QTimer::timeout, this, [this] {
        // Closing under an open dialog would delete the window while the dialog still uses it.
        if (QApplication::activeModalWidget()) {
            m_idleTimer.start();
            return;
        }
        close();
    });
    qApp->installEventFilter(this);
    m_idleTimer.start();

    reloadList(0);
}

bool EditorWindow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::Wheel:
        if (auto *widget = qobject_cast<QWidget *>(watched); widget && widget->window() == this) {
            m_idleTimer.start();
        }
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void EditorWindow::closeEvent(QCloseEvent *event)
{
    if (!commit()) {
        event->ignore();
        return;
    }
    m_shownId.clear();
    m_title->clear();
    m_body->clear();
    event->accept();
}

// Fills the list from the store and selects row, which shows that snippet.
void EditorWindow::reloadList(int row)
{
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        for (const Snippet &snippet : m_store.snippets()) {
            auto *item = new QListWidgetItem(snippet.title, m_list);
            item->setData(kIdRole, snippet.id);
        }
    }
    const int count = m_list->count();
    m_list->setCurrentRow(count == 0 ? -1 : std::clamp(row, 0, count - 1));
    if (m_list->currentRow() < 0) {
        showSnippet(-1);
    }
}

void EditorWindow::showSnippet(int row)
{
    QListWidgetItem *item = m_list->item(row);
    const int index = item ? indexOf(item->data(kIdRole).toString()) : -1;
    std::optional<SecureBytes> body;
    if (index >= 0) {
        body = m_store.body(static_cast<std::size_t>(index));
    }
    if (!body) {
        m_shownId.clear();
        m_title->clear();
        m_body->clear();
        m_title->setEnabled(false);
        m_body->setEnabled(false);
        return;
    }
    m_shownId = item->data(kIdRole).toString();
    m_title->setEnabled(true);
    m_body->setEnabled(true);
    m_title->setText(m_store.snippets()[static_cast<std::size_t>(index)].title);
    m_body->setPlainText(QString::fromUtf8(reinterpret_cast<const char *>(body->data()),
                                           static_cast<qsizetype>(body->size())));
    m_body->document()->setModified(false);
}

// Writes the shown title and body back to the store and the file if they were edited.
bool EditorWindow::commit()
{
    const int index = indexOf(m_shownId);
    if (index < 0 || (!m_title->isModified() && !m_body->document()->isModified())) {
        return true;
    }
    QByteArray utf8 = m_body->toPlainText().toUtf8();
    const SecureBytes body(reinterpret_cast<const std::uint8_t *>(utf8.constData()),
                           static_cast<std::size_t>(utf8.size()));
    secureWipe(utf8.data(), static_cast<std::size_t>(utf8.size()));
    // An empty title would be invisible in the popup, and imports refuse it.
    if (m_title->text().trimmed().isEmpty()) {
        m_title->setText(tr("Untitled"));
        m_list->item(index)->setText(m_title->text());
    }
    if (!m_store.update(static_cast<std::size_t>(index), m_title->text(), body) || !save()) {
        return false;
    }
    m_title->setModified(false);
    m_body->document()->setModified(false);
    return true;
}

bool EditorWindow::save()
{
    if (!m_store.saveFile(m_storePath)) {
        QMessageBox::critical(this, windowTitle(), tr("Could not save the snippets."));
        return false;
    }
    return true;
}

void EditorWindow::addSnippet()
{
    commit();
    if (!m_store.add(tr("Untitled"), SecureBytes())) {
        return;
    }
    save();
    reloadList(m_list->count());
    m_title->setFocus();
    m_title->selectAll();
}

void EditorWindow::removeSnippet()
{
    const QListWidgetItem *item = m_list->currentItem();
    if (!item) {
        return;
    }
    const auto answer =
        QMessageBox::question(this, windowTitle(), tr("Delete \"%1\"?").arg(item->text()));
    if (answer != QMessageBox::Yes) {
        return;
    }
    const int row = m_list->currentRow();
    const int index = indexOf(item->data(kIdRole).toString());
    // The body on screen is being deleted, so there is nothing to commit.
    m_shownId.clear();
    m_store.remove(static_cast<std::size_t>(index));
    save();
    reloadList(row);
}

// Brings the store into the order the user dragged the list into.
void EditorWindow::applyListOrder()
{
    for (int row = 0; row < m_list->count(); ++row) {
        const int index = indexOf(m_list->item(row)->data(kIdRole).toString());
        if (index != row) {
            m_store.move(static_cast<std::size_t>(index), static_cast<std::size_t>(row));
        }
    }
    save();
}

void EditorWindow::importJson()
{
    commit();
    // The native file dialog does not show up as a modal widget, so the idle check cannot see it.
    m_idleTimer.stop();
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Import JSON"), QString(), tr("JSON (*.json)"));
    m_idleTimer.start();
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, windowTitle(), tr("Could not open %1.").arg(path));
        return;
    }
    QString error;
    const auto snippets = jsonimport::parse(file.readAll(), &error);
    file.close();
    if (!snippets) {
        QMessageBox::warning(this, windowTitle(), tr("Invalid JSON: %1").arg(error));
        return;
    }
    if (!confirmImport(*snippets) || !append(*snippets)) {
        return;
    }
    const auto trash = QMessageBox::question(
        this, windowTitle(), tr("The JSON file is not encrypted. Move it to the Trash?"));
    if (trash == QMessageBox::Yes) {
        QFile::moveToTrash(path);
    }
}

void EditorWindow::importUsnip()
{
    commit();
    m_idleTimer.stop();
    const QString path = QFileDialog::getOpenFileName(this, tr("Import .usnip"), QString(),
                                                      tr("μSnip export (*.usnip)"));
    m_idleTimer.start();
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, windowTitle(), tr("Could not open %1.").arg(path));
        return;
    }
    const QByteArray data = file.readAll();
    file.close();
    if (!exportfile::isExportFile(data)) {
        QMessageBox::warning(this, windowTitle(), tr("%1 is not a μSnip export file.").arg(path));
        return;
    }

    QString message = tr("Enter the passphrase of %1.").arg(QFileInfo(path).fileName());
    std::optional<std::vector<PlainSnippet>> snippets;
    while (!snippets) {
        const auto passphrase =
            askPassphrase(this, tr("Import .usnip"), message, PassphraseMode::Enter);
        if (!passphrase) {
            return;
        }
        QApplication::setOverrideCursor(Qt::WaitCursor);
        snippets = exportfile::read(data, *passphrase);
        QApplication::restoreOverrideCursor();
        message = tr("Wrong passphrase, or the file is damaged. Try again.");
    }
    if (confirmImport(*snippets)) {
        append(*snippets);
    }
}

void EditorWindow::exportUsnip()
{
    if (!commit()) {
        return;
    }
    if (m_store.snippets().empty()) {
        QMessageBox::information(this, windowTitle(), tr("There are no snippets to export."));
        return;
    }
    m_idleTimer.stop();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export to .usnip"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
            QStringLiteral("/snippets.usnip"),
        tr("μSnip export (*.usnip)"));
    m_idleTimer.start();
    if (path.isEmpty()) {
        return;
    }
    const auto passphrase = askPassphrase(
        this, tr("Export to .usnip"),
        tr("The export file is encrypted with this passphrase instead of the key in the "
           "Keychain, so it can be imported on another Mac. It cannot be recovered if forgotten."),
        PassphraseMode::Create);
    if (!passphrase) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::vector<PlainSnippet> snippets = m_store.decryptAll();
    // Older versions of the editor kept empty titles, which import refuses.
    for (PlainSnippet &snippet : snippets) {
        if (snippet.title.trimmed().isEmpty()) {
            snippet.title = tr("Untitled");
        }
    }
    const auto data = exportfile::write(snippets, *passphrase);
    snippets.clear();
    QApplication::restoreOverrideCursor();
    QSaveFile file(path);
    if (!data || !file.open(QIODevice::WriteOnly) || file.write(*data) != data->size() ||
        !file.commit()) {
        QMessageBox::critical(this, windowTitle(), tr("Could not write %1.").arg(path));
        return;
    }
    QMessageBox::information(
        this, windowTitle(),
        tr("Exported %n snippet(s).", nullptr, static_cast<int>(m_store.snippets().size())));
}

// Shows how many snippets will be added and their titles, and asks to go ahead.
bool EditorWindow::confirmImport(const std::vector<PlainSnippet> &snippets)
{
    if (snippets.empty()) {
        QMessageBox::information(this, windowTitle(), tr("The file holds no snippets."));
        return false;
    }
    QStringList titles;
    for (const PlainSnippet &snippet : snippets) {
        if (titles.size() == kMaxListedTitles) {
            titles.append(
                tr("and %n more", nullptr, static_cast<int>(snippets.size()) - kMaxListedTitles));
            break;
        }
        titles.append(snippet.title);
    }
    QMessageBox box(QMessageBox::Question, windowTitle(),
                    tr("Add %n snippet(s) after the existing ones?", nullptr,
                       static_cast<int>(snippets.size())),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setInformativeText(titles.join(QLatin1Char('\n')));
    return box.exec() == QMessageBox::Yes;
}

bool EditorWindow::append(const std::vector<PlainSnippet> &snippets)
{
    const int firstNew = m_list->count();
    for (const PlainSnippet &snippet : snippets) {
        m_store.add(snippet.title, snippet.body);
    }
    reloadList(firstNew);
    return save();
}

int EditorWindow::indexOf(const QString &id) const
{
    const auto &snippets = m_store.snippets();
    for (std::size_t i = 0; i < snippets.size(); ++i) {
        if (snippets[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace usnip
