#include "stitch/capture_replacement.h"
#include "core/i18n.h"
#include "ui/dialogs.h"
#include <QPointer>

namespace jietuba {
bool confirm_capture_replacement(QWidget *parent, const QImage &image,
                                 const std::function<bool(const QImage &)> &save) {
    if (image.isNull()) return true;
    const QPointer<QWidget> owner(parent);
    const auto tr = [](const char *text) { return i18n_tr(text, "ScrollCaptureWindow"); };
    const QString choice = show_custom_confirm_dialog(parent, tr("Previous long screenshot"),
        tr("Save the previous long screenshot before starting a new one?"),
        {{"cancel", tr("Cancel"), QDialogButtonBox::RejectRole, true},
         {"discard", tr("Discard and continue"), QDialogButtonBox::DestructiveRole, false},
         {"save", tr("Save and continue"), QDialogButtonBox::AcceptRole, false}}).toString();
    if (parent && !owner) return false;
    if (choice == "discard") return true;
    return choice == "save" && save && save(image);
}
}
