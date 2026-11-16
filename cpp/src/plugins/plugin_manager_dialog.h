#pragma once
#include <QString>
#include <QStringList>
#include "plugins/plugin_package.h"
class QWidget;
namespace jietuba::plugins {
void install_plugin_interactively(QWidget *parent, const QString &plugin_root = {});
bool install_plugin_packages_interactively(QWidget *parent,const QStringList &paths,const QString &plugin_root={},
                                           const PluginInstallOptions &options = {}, bool remember_directory = true);
QWidget *create_plugin_manager(QWidget *parent, const QString &plugin_root = {});
}
