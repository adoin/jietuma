#include "core/installer_exit.h"
#include "core/brand_config.h"
#include <QCoreApplication>
#include <QString>
#include <QWinEventNotifier>
#include <windows.h>

namespace jietuba {
QObject *listen_for_installer_exit(QObject *parent, std::function<void()> request_exit) {
    const auto name = QStringLiteral("Local\\" JTM_NAME ".InstallerExit.%1")
                          .arg(QCoreApplication::applicationPid()).toStdWString();
    const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, name.c_str());
    if (!event) return nullptr;
    auto *notifier = new QWinEventNotifier(event, parent);
    QObject::connect(notifier, &QWinEventNotifier::activated, notifier,
                     [request_exit = std::move(request_exit)] { request_exit(); });
    QObject::connect(notifier, &QObject::destroyed, [event] { CloseHandle(event); });
    return notifier;
}
}
