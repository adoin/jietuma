#pragma once
#include <QString>
#include <QStringList>
class QWidget;
namespace jietuba::plugins {
void install_plugin_interactively(QWidget *parent, const QString &plugin_root = {});
void install_plugin_packages_interactively(QWidget *parent,const QStringList &paths,const QString &plugin_root={});
void show_plugin_manager(QWidget *parent, const QString &plugin_root = {});
}
