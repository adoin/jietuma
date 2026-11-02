#include "plugins/clipboard_extensions.h"
#include "plugins/plugin_package.h"
#include "clipboard/core/manager.h"
#include "core/i18n.h"
#include "core/native.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMenu>
#include <QRegularExpression>
namespace jietuba::plugins {
namespace {
QList<QJsonObject> manifests(const QString &root) {
    QList<QJsonObject> out;
    for(const auto &entry:QDir(root.isEmpty()?plugin_directory():root).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks)) {
        if(entry.isJunction() || QFileInfo::exists(entry.absoluteFilePath()+"/.disabled")) continue;
        QFile file(entry.absoluteFilePath()+"/plugin.json");if(!file.open(QIODevice::ReadOnly)||file.size()>65536) continue;
        const auto object=QJsonDocument::fromJson(file.readAll()).object();
        if(object.value("id").toString()==entry.fileName() && object.value("api_version").toInt()==2 && object.value("type")=="extension") out.append(object);
    }
    return out;
}
QString label(const QJsonObject &obj) {return obj.value("name_i18n").toObject().value(I18nManager::get_current_language()).toString(obj.value("name").toString());}
}
QList<ClipboardType> clipboard_types(const QString &root) {
    QList<ClipboardType> out;
    for(const auto &manifest:manifests(root)) for(const auto &value:manifest.value("contributes").toObject().value("clipboard").toObject().value("types").toArray()) {
        const auto type=value.toObject();const auto id=type.value("id").toString(),plugin=manifest.value("id").toString();
        if(!QRegularExpression("^[a-z][a-z0-9_-]{0,63}$").match(id).hasMatch()) continue;
        out.append({"plugin:"+plugin+":"+id,label(type),plugin,id});
    }
    return out;
}
QJsonObject metadata_with_types(const QString &plugin,QJsonObject metadata,const QString &root) {
    QJsonArray types;
    bool declared=false;
    for(const auto &manifest:manifests(root)) if(manifest.value("id")==plugin)
      for(const auto &value:manifest.value("contributes").toObject().value("clipboard").toObject().value("types").toArray()) {
        declared=true;const auto type=value.toObject();const QString field=type.value("metadata_field").toString();
        const auto selected=metadata.value(field).toString();
        if(!selected.isEmpty() && !type.value("exclude_values").toArray().contains(selected)) types.append(type.value("id"));
      }
    if(declared) metadata.insert("types",types);return metadata;
}
bool has_item_choices(const clipboard::ClipboardItem &item,const QString &root) {
    for(const auto &manifest:manifests(root)) {
        const auto spec=manifest.value("contributes").toObject().value("clipboard").toObject().value("metadata").toObject();
        if(!spec.value("content_types").toArray().contains(item.content_type)) continue;
        for(const auto &field:spec.value("fields").toArray()) if(field.toObject().value("type")=="choice") return true;
    }
    return false;
}
void add_item_choices(QMenu *menu,const clipboard::ClipboardItem &item,clipboard::ClipboardManager &manager,std::function<void()> changed,const QString &root) {
    for(const auto &manifest:manifests(root)) {
      const auto spec=manifest.value("contributes").toObject().value("clipboard").toObject().value("metadata").toObject();
      const QString plugin=manifest.value("id").toString();
      if(!spec.value("content_types").toArray().contains(item.content_type)) continue;
      for(const auto &value:spec.value("fields").toArray()) {
        const auto field=value.toObject();if(field.value("type")!="choice") continue;
        auto *sub=menu->addMenu(field.value("label_i18n").toObject().value(I18nManager::get_current_language()).toString(field.value("label").toString()));
        const QString key=field.value("key").toString();const auto existing=manager.plugin_metadata(item.id,plugin);
        for(const auto &choice_value:field.value("choices").toArray()) {
          const auto choice=choice_value.toObject();const QString selected=choice.value("value").toString();
          auto *action=sub->addAction(label(choice));action->setCheckable(true);action->setChecked(existing.value(key).toString()==selected);
          QObject::connect(action,&QAction::triggered,menu,[&,plugin,key,selected,item,root,changed] {
              auto data=manager.plugin_metadata(item.id,plugin);data.insert(key,selected);
              if(manager.set_plugin_metadata(item.id,plugin,metadata_with_types(plugin,data,root)) && changed) changed();
          });
        }
      }
    }
}
bool apply_action_payload(const QJsonObject &payload,qint64 id,const QString &plugin,clipboard::ClipboardManager &manager) {
    Q_UNUSED(plugin);
    const auto kind=payload.value("kind").toString();
    if(kind=="original") return manager.paste_item(id,true,false);
    if(kind=="text") return payload.value("data").isString() && clipboard::ClipboardManager::set_clipboard_text(payload.value("data").toString());
    if(kind!="clipboard" || !payload.value("data").isObject()) return false;
    const auto bytes=QJsonDocument(payload.value("data").toObject()).toJson(QJsonDocument::Compact);
    char error[1024]={};return jb_clipboard_set_formats(bytes.constData(),error,sizeof(error));
}
}
