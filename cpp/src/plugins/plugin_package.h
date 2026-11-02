#pragma once
#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <optional>
class QReadWriteLock;

namespace jietuba::plugins {
struct PluginInstallResult {
    bool ok = false;
    QString id;
    QString error;
    bool skipped = false;
};
struct InstalledPlugin {
    QString id, name, version, type;
    bool enabled = true;
    QJsonArray settings;
    QString icon_path;
};
struct PluginPackageInfo {
    bool ok=false;
    QString path,id,name,version,error;
    QByteArray icon;
};
struct PluginInstallPlanItem {
    PluginPackageInfo package;
    QString installed_version,status;
    bool installable=false;
};
std::optional<int> compare_plugin_versions(const QString &left,const QString &right);
PluginPackageInfo inspect_plugin_package(const QString &path);
QList<PluginInstallPlanItem> plan_plugin_install(const QStringList &paths,const QString &root={});

// Installs a .jtbplugin ZIP, staging it alongside the destination and restoring
// the old version if replacement fails. Never executes package contents.
PluginInstallResult install_plugin_package(const QString &package_path, const QString &plugin_root = {});
QString plugin_directory();
QReadWriteLock &plugin_lock();
QList<InstalledPlugin> installed_plugins(const QString &plugin_root = {});
bool set_plugin_enabled(const QString &id, bool enabled, const QString &plugin_root = {});
bool uninstall_plugin(const QString &id, const QString &plugin_root = {});
QString plugin_config_path(const QString &id, const QString &plugin_root = {});
QJsonObject read_plugin_config(const QString &id, const QString &plugin_root = {});
bool save_plugin_config(const QString &id, const QJsonObject &config, const QString &plugin_root = {});
}
