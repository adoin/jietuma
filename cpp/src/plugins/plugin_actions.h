#pragma once
#include "clipboard/core/models.h"
#include <QList>
#include <QString>
#include <QJsonObject>
#include <functional>
#include <optional>

class QObject;
namespace jietuba::clipboard { class ClipboardManager; }
namespace jietuba::plugins {
struct ActionPlugin { QString id, name; };
QList<ActionPlugin> action_plugins(const clipboard::ClipboardItem &item, const QString &root = {});
bool run_action_payload(const QString &id,const clipboard::ClipboardItem &item,clipboard::ClipboardManager &manager,
                        QObject *context,std::function<void(std::optional<QJsonObject>)> callback,const QString &root={});
// Runs an installed action asynchronously. A null result denotes failure; empty
// text is a valid result. The callback is owned by context and is not called after it dies.
bool run_action(const QString &id, const clipboard::ClipboardItem &item, clipboard::ClipboardManager &manager,
                QObject *context, std::function<void(std::optional<QString>)> callback, const QString &root = {});
}
