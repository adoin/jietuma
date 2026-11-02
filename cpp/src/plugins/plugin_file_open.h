#pragma once
#include <QStringList>
#include <functional>
class QObject;
namespace jietuba::plugins {
QStringList plugin_files_from_command_line();
bool forward_plugin_files(const QStringList &paths,const QString &channel={});
bool receive_plugin_files(QObject *owner,const QStringList &initial,std::function<void(QStringList)> install,bool exit_when_idle=false,const QString &channel={});
}
