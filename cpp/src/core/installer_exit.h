#pragma once
#include <QObject>
#include <functional>

namespace jietuba {
// A per-process Windows event; the installer verifies the executable path first.
// The callback uses the same shutdown path as the tray's Exit action.
QObject *listen_for_installer_exit(QObject *parent, std::function<void()> request_exit);
}
