#pragma once
#include "clipboard/core/models.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <functional>
class QMenu;
class QWidget;
namespace jietuba::clipboard { class ClipboardManager; }
namespace jietuba::plugins {
struct ClipboardType {QString key,label,plugin,id;};
QList<ClipboardType> clipboard_types(const QString &root={});
bool has_item_choices(const clipboard::ClipboardItem &item,const QString &root={});
void add_item_choices(QMenu *menu,const clipboard::ClipboardItem &item,clipboard::ClipboardManager &manager,
                      std::function<void()> changed,const QString &root={});
QJsonObject metadata_with_types(const QString &plugin,QJsonObject metadata,const QString &root={});
bool apply_action_payload(const QJsonObject &payload,qint64 item_id,const QString &plugin,
                          clipboard::ClipboardManager &manager);
}
