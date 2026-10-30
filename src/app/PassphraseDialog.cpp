#include "PassphraseDialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace usnip {

std::optional<SecureBytes> askPassphrase(QWidget *parent, const QString &title,
                                         const QString &message, PassphraseMode mode)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    auto *passphrase = new QLineEdit(&dialog);
    auto *repeat = new QLineEdit(&dialog);
    for (QLineEdit *edit : {passphrase, repeat}) {
        edit->setEchoMode(QLineEdit::Password);
        edit->setMinimumWidth(240);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);

    auto *layout = new QFormLayout(&dialog);
    auto *label = new QLabel(message, &dialog);
    label->setWordWrap(true);
    layout->addRow(label);
    layout->addRow(QObject::tr("Passphrase:"), passphrase);
    if (mode == PassphraseMode::Create) {
        layout->addRow(QObject::tr("Again:"), repeat);
    }
    else {
        repeat->hide();
    }
    layout->addRow(buttons);

    const auto updateOk = [&] {
        const bool matches = mode == PassphraseMode::Enter || passphrase->text() == repeat->text();
        ok->setEnabled(!passphrase->text().isEmpty() && matches);
    };
    QObject::connect(passphrase, &QLineEdit::textChanged, &dialog, updateOk);
    QObject::connect(repeat, &QLineEdit::textChanged, &dialog, updateOk);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    updateOk();

    std::optional<SecureBytes> result;
    if (dialog.exec() == QDialog::Accepted) {
        QByteArray utf8 = passphrase->text().toUtf8();
        result.emplace(reinterpret_cast<const std::uint8_t *>(utf8.constData()),
                       static_cast<std::size_t>(utf8.size()));
        secureWipe(utf8.data(), static_cast<std::size_t>(utf8.size()));
    }
    passphrase->clear();
    repeat->clear();
    return result;
}

} // namespace usnip
