#include "core/windows_clipboard_shortcut.h"
#include "core/global_hotkey.h"
#include <QSettings>
#include <stdexcept>

namespace jietuba::windows_clipboard_shortcut {
bool uses_win_v(bool enabled, const QStringList &hotkeys) {
    if (!enabled) return false;
    for (const auto &hotkey : hotkeys) {
        try {
            const auto [mods, key] = global_hotkey::parse_hotkey(hotkey);
            if (mods == global_hotkey::MOD_WIN && key == 'V') return true;
        } catch (const std::invalid_argument &) {}
    }
    return false;
}
bool is_blocked(QSettings &registry) {
    const auto value = registry.value("DisabledHotkeys");
    return value.metaType().id() == QMetaType::QString && value.toString().contains('V', Qt::CaseInsensitive);
}
bool block(QSettings &registry, QString *error) {
    if (error) error->clear();
    registry.sync();
    const auto value = registry.value("DisabledHotkeys");
    if (registry.status() != QSettings::NoError) {
        if (error) *error = "read";
        return false;
    }
    if (value.isValid() && value.metaType().id() != QMetaType::QString) {
        if (error) *error = "type";
        return false;
    }
    if (is_blocked(registry)) return true;
    registry.setValue("DisabledHotkeys", value.toString() + 'V');
    registry.sync();
    if (registry.status() != QSettings::NoError || !is_blocked(registry)) {
        if (error) *error = "write";
        return false;
    }
    return true;
}
}
