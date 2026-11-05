#pragma once
#include <QStringList>
class QSettings;
namespace jietuba::windows_clipboard_shortcut {
inline constexpr auto registry_path = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
bool uses_win_v(bool enabled, const QStringList &hotkeys);
bool is_blocked(QSettings &registry);
// The caller must obtain consent. A supplied store lets tests avoid the real registry.
bool block(QSettings &registry, QString *error);
}
