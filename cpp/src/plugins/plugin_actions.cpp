#include "plugins/plugin_actions.h"
#include "plugins/plugin_package.h"
#include "clipboard/core/manager.h"
#include "core/i18n.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <algorithm>
#include <memory>
#include <utility>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace jietuba::plugins {
namespace {
struct ActionSpec { QString id, name, folder, executable; QJsonObject manifest; };
QList<ActionSpec> actions(const clipboard::ClipboardItem &item, const QString &root) {
    QList<ActionSpec> result;
    for (const auto &dir : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name)) {
        if (dir.isJunction()) continue;
        if (QFileInfo::exists(QDir(dir.absoluteFilePath()).filePath(".disabled"))) continue;
        QFile file(QDir(dir.absoluteFilePath()).filePath("plugin.json"));
        if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) continue;
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        const auto capability = object.value("contributes").toObject().value("clipboard").toObject().value("action").toObject();
        const QString id = object.value("id").toString();
        if(capability.contains("when_formats_any") || capability.contains("when_text_contains")) {
            bool matches=!capability.value("when_text_contains").toString().isEmpty() && item.content.contains(capability.value("when_text_contains").toString());
            const auto names=clipboard::ClipboardManager::instance().format_names(item.id);
            for(const auto &format:capability.value("when_formats_any").toArray()) matches=matches||names.contains(format.toString(),Qt::CaseInsensitive);
            if(!matches) continue;
        }
        if (object.value("api_version").toInt() != 2 || object.value("type").toString() != "extension" || id != dir.fileName() ||
            !capability.value("content_types").toArray().contains(item.content_type) ||
            !object.value("contributes").toObject().value("commands").toArray().contains(capability.value("command"))) continue;
        const QString command = object.value("command").toString();
        if (command.isEmpty() || QFileInfo(command).isAbsolute()) continue;
        const QString folder = dir.canonicalFilePath();
        const QString executable = QFileInfo(QDir(folder).filePath(command)).canonicalFilePath();
        if (executable.isEmpty() || !executable.startsWith(folder + '/', Qt::CaseInsensitive) || !QFileInfo(executable).isFile()) continue;
        const auto translations = object.value("name_i18n").toObject();
        const QString name = translations.value(I18nManager::get_current_language()).toString(object.value("name").toString(id));
        const auto entries=capability.value("entries").toArray();
        if(entries.isEmpty()) result.append({id,name,folder,executable,capability});
        else for(const auto &entry_value:entries) {
            const auto entry=entry_value.toObject();auto action=capability;
            if(!object.value("contributes").toObject().value("commands").toArray().contains(entry.value("command"))) continue;
            action.insert("command",entry.value("command"));
            const QString title=entry.value("name_i18n").toObject().value(I18nManager::get_current_language()).toString(entry.value("name").toString());
            result.append({id+"."+entry.value("id").toString(),title,folder,executable,action});
        }
    }
    return result;
}
}

QList<ActionPlugin> action_plugins(const clipboard::ClipboardItem &item, const QString &root) {
    QList<ActionPlugin> result;
    for (const auto &spec : actions(item, root.isEmpty() ? plugin_directory() : root)) result.append({spec.id, spec.name});
    return result;
}

bool run_action_payload(const QString &id, const clipboard::ClipboardItem &item, clipboard::ClipboardManager &manager,
                QObject *context, std::function<void(std::optional<QJsonObject>)> callback, const QString &root) {
    if (!context || !callback) return false;
    const auto specs = actions(item, root.isEmpty() ? plugin_directory() : root);
    auto found = std::find_if(specs.begin(), specs.end(), [&id](const auto &spec) { return spec.id == id; });
    if (found == specs.end()) return false;
    QJsonObject input{{"language", I18nManager::get_current_language()},
                      {"content_type", item.content_type}, {"sub_kind", item.sub_kind}, {"content", item.content}};
    if (item.html_content) input.insert("html", *item.html_content);
    input.insert("metadata",manager.plugin_metadata(item.id,QFileInfo(found->folder).fileName()));
    if (item.source_app) input.insert("source_app", *item.source_app);
    QJsonArray paths;
    for (const auto &path : clipboard::item_paths(item)) paths.append(path);
    input.insert("paths", paths);
    QJsonArray available_formats;for(const auto &name:manager.format_names(item.id)) available_formats.append(name);input.insert("format_names",available_formats);
    QJsonObject formats;
    const auto requested = found->manifest.value("formats").toArray();
    qsizetype total = 0;
    int examined = 0;
    for (const auto &value : requested) {
        if (examined++ >= 8) break;
        if (!value.isString() || value.toString().isEmpty() || value.toString().size() > 128 || value.toString() == "*") continue;
        if (const auto data = manager.get_format(item.id, value.toString()); data && total + data->size() <= 4 * 1024 * 1024) {
            total += data->size();
            formats.insert(value.toString(), QString::fromLatin1(data->toBase64()));
        }
    }
    if (!formats.isEmpty()) input.insert("formats_base64", formats);
    const QJsonObject payload = item.content_type == "text" && requested.isEmpty()
        ? QJsonObject{{"kind", "text"}, {"data", item.content}}
        : QJsonObject{{"kind", "clipboard"}, {"data", input}};
    const QJsonObject details{{"source_app", item.source_app.value_or(QString())}, {"language", I18nManager::get_current_language()}};
    const QJsonObject envelope{{"api_version", 2}, {"type", "extension"}, {"event", "command"}, {"command", found->manifest.value("command")},
                               {"payload", payload}, {"context", details}};
    const QByteArray request = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    if (request.size() > 8 * 1024 * 1024) return false;
    auto *process = new QProcess(context);
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    process->setProgram(found->executable);
    process->setWorkingDirectory(found->folder);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("JIETUBA_PLUGIN_CONFIG", plugin_config_path(QFileInfo(found->folder).fileName(), root));
    process->setProcessEnvironment(environment);
    auto output = std::make_shared<QByteArray>();
    auto done = std::make_shared<bool>(false);
    auto finish = [process, callback, done](std::optional<QJsonObject> value) {
        if (std::exchange(*done, true)) return;
        process->deleteLater();
        callback(std::move(value));
    };
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, context, [process, finish] { process->kill(); finish(std::nullopt); });
    QObject::connect(process, &QProcess::started, context, [process, request] { process->write(request); process->closeWriteChannel(); });
    QObject::connect(process, &QProcess::readyReadStandardOutput, context, [process, output, finish] {
        *output += process->readAllStandardOutput();
        if (output->size() > 8 * 1024 * 1024) { process->kill(); finish(std::nullopt); }
    });
    QObject::connect(process, &QProcess::readyReadStandardError, context, [process] { process->readAllStandardError(); });
    QObject::connect(process, &QProcess::errorOccurred, context, [finish](QProcess::ProcessError) { finish(std::nullopt); });
    QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), context,
                     [process, output, finish](int code, QProcess::ExitStatus status) {
        *output += process->readAllStandardOutput();
        if (status != QProcess::NormalExit || code != 0 || output->size() > 8 * 1024 * 1024) { finish(std::nullopt); return; }
        const auto result = QJsonDocument::fromJson(*output).object();
        const auto payload = result.value("payload").toObject();
        const QString kind=payload.value("kind").toString();
        if(result.value("api_version").toInt()!=2 || !result.value("ok").toBool() ||
           !QStringList{"text","clipboard","original","error"}.contains(kind) ||
           (kind=="text" && (!payload.value("data").isString() || payload.value("data").toString().contains(QChar(u'\0'))))) {
            finish(std::nullopt);return;
        }
        finish(payload);
    });
    timer->start(5000);
    process->start();
    return true;
}
bool run_action(const QString &id,const clipboard::ClipboardItem &item,clipboard::ClipboardManager &manager,
                QObject *context,std::function<void(std::optional<QString>)> callback,const QString &root) {
    return run_action_payload(id,item,manager,context,[callback](std::optional<QJsonObject> result) {
        if(!result || result->value("kind")!="text") callback(std::nullopt);
        else callback(result->value("data").toString());
    },root);
}

}
