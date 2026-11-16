#include "plugins/plugin_package.h"
#include "core/app_version.h"
#include <QSysInfo>
#include <QRegularExpression>
#include <QMap>
#include <QFileInfo>
namespace jietuba::plugins {
namespace {
struct Version {QStringList numbers,pre;};
std::optional<Version> parse(const QString &s) {
    static const QRegularExpression pattern(R"(^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?\z)");
    if(s.size()>128)return std::nullopt;
    const auto match=pattern.match(s);if(!match.hasMatch())return std::nullopt;
    Version v{{match.captured(1),match.captured(2),match.captured(3)}, {}};
    if(!match.captured(4).isEmpty())v.pre=match.captured(4).split('.');
    for(const auto &part:v.pre)if(part.size()>1 && part[0]=='0' && part.toLatin1().toStdString().find_first_not_of("0123456789")==std::string::npos)return std::nullopt;
    return v;
}
bool numeric(const QString &s){for(const auto c:s)if(c<'0'||c>'9')return false;return !s.isEmpty();}
int number_compare(const QString &a,const QString &b){if(a.size()!=b.size())return a.size()<b.size()?-1:1;return QString::compare(a,b,Qt::CaseSensitive);}
}
std::optional<int> compare_plugin_versions(const QString &left,const QString &right) {
    const auto a=parse(left),b=parse(right);if(!a||!b)return std::nullopt;
    for(int i=0;i<3;++i)if(const auto cmp=number_compare(a->numbers[i],b->numbers[i]);cmp)return cmp<0?-1:1;
    if(a->pre.isEmpty()||b->pre.isEmpty())return a->pre.isEmpty()==b->pre.isEmpty()?0:a->pre.isEmpty()?1:-1;
    for(int i=0;i<qMin(a->pre.size(),b->pre.size());++i){const auto &x=a->pre[i],&y=b->pre[i];const bool nx=numeric(x),ny=numeric(y);int cmp=nx&&ny?number_compare(x,y):nx!=ny?(nx?-1:1):QString::compare(x,y,Qt::CaseSensitive);if(cmp)return cmp<0?-1:1;}
    return a->pre.size()==b->pre.size()?0:a->pre.size()<b->pre.size()?-1:1;
}
QList<PluginInstallPlanItem> plan_plugin_install(const QStringList &paths,const QString &root) {
    QList<PluginInstallPlanItem> plan;QMap<QString,int> best;QMap<QString,QString> installed;
    for(const auto &p:installed_plugins(root))installed.insert(p.id,p.version);
    for(const auto &path:paths) {
        PluginInstallPlanItem item;item.package=inspect_plugin_package(path);item.status=item.package.error;
        const int index=plan.size();plan.append(item);if(!item.package.ok)continue;
        const auto previous=best.constFind(item.package.id);
        if(previous==best.cend())best.insert(item.package.id,index);
        else if(compare_plugin_versions(item.package.version,plan[*previous].package.version).value_or(0)>0)best[item.package.id]=index;
    }
    for(int i=0;i<plan.size();++i){auto &item=plan[i];if(!item.package.ok)continue;
        item.installed_version=installed.value(item.package.id);
        if(best.value(item.package.id)!=i)item.status="A newer or identical package is selected";
        else if(const auto cmp=compare_plugin_versions(item.package.version,item.installed_version);cmp&&*cmp<=0)item.status="Latest version already installed";
        else {item.installable=true;item.status=item.installed_version.isEmpty()?"Install":"Update";}
    }
    return plan;
}

std::optional<bool> plugin_version_satisfies(const QString &version, const QString &range) {
    if (!compare_plugin_versions(version, version)) return std::nullopt;
    if (range.isEmpty()) return true;
    if (range.size() > 512) return std::nullopt;
    const auto clauses = range.split(',');
    if (clauses.size() > 8) return std::nullopt;
    bool matches = true;
    static const QRegularExpression part(R"(^(>=|<=|>|<|=)?\s*(\S+)$)");
    for (const auto &clause : clauses) {
        const auto parsed = part.match(clause.trimmed());
        if (!parsed.hasMatch()) return std::nullopt;
        const auto cmp = compare_plugin_versions(version, parsed.captured(2));
        if (!cmp) return std::nullopt;
        const auto op = parsed.captured(1);
        matches &= op == ">=" ? *cmp >= 0 : op == ">" ? *cmp > 0 : op == "<=" ? *cmp <= 0 : op == "<" ? *cmp < 0 : *cmp == 0;
    }
    return matches;
}
QString plugin_platform() {
    const auto arch = QSysInfo::buildCpuArchitecture();
    return arch == "arm64" ? "windows-arm64" : arch == "x86_64" ? "windows-x86_64" : QString();
}
bool plugin_runtime_compatible(const QJsonObject &manifest) {
    if (manifest.contains("host") && (!manifest.value("host").isString() || manifest.value("host").toString().isEmpty() ||
        !plugin_version_satisfies(APP_VERSION, manifest.value("host").toString()).value_or(false))) return false;
    if (manifest.contains("platform") && manifest.value("platform").toString() != plugin_platform()) return false;
    return true;
}
}
