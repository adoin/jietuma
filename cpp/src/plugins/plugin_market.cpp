#include "plugins/plugin_market.h"
#include "core/app_version.h"
#include "core/brand_config.h"
#include "settings/tool_settings.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

namespace jietuba::plugins::market {
namespace {
constexpr qint64 CATALOG_LIMIT = 4 * 1024 * 1024;
constexpr qint64 PACKAGE_LIMIT = 64 * 1024 * 1024;
void require(bool condition, const char *message) { if (!condition) throw Error(message); }
qint64 positive_id(const QJsonValue &value) {
    const double n = value.toDouble(-1);
    require(value.isDouble() && n > 0 && n <= 9007199254740991.0 && std::floor(n) == n, "Invalid GitHub resource identity.");
    return qint64(n);
}
QJsonObject object(const QByteArray &bytes) {
    const auto document = QJsonDocument::fromJson(bytes);
    require(document.isObject(), "Invalid marketplace response.");
    return document.object();
}
QString sha256(const QByteArray &bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); }
QByteArray read(const QString &path, qint64 limit) {
    if(QFileInfo(path).isSymLink())return {};
    QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>limit)return {};
    return file.readAll();
}
void write(const QString &path,const QByteArray &bytes) {
    require(!QFileInfo(path).isSymLink(),"Unsafe marketplace cache path.");
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit(),"Could not save marketplace data.");
}
bool stable(const QString &version) { return !version.section('+',0,0).contains('-'); }
Source repository(const QJsonObject &data) {
    Source result{positive_id(data.value("id")),data.value("full_name").toString()};
    require(valid_repository_name(result.full_name),"Invalid GitHub repository name.");return result;
}
QString encoded(const QString &text) { return QString::fromLatin1(QUrl::toPercentEncoding(text)); }
}
Source official_source() { return {JTM_PLUGINS_REPOSITORY_ID,QStringLiteral(JTM_PLUGINS_REPOSITORY)}; }
bool valid_repository_name(const QString &name) {
    static const QRegularExpression pattern(R"(^[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9_.-]{1,100}\z)");
    return pattern.match(name).hasMatch() && name.section('/',1)!="." && name.section('/',1)!="..";
}
QString repository_input(const QString &text) {
    QString result=text.trimmed();
    if(result.startsWith("https://",Qt::CaseInsensitive)) {
        const QUrl url(result);
        require(url.isValid() && url.scheme()=="https" && url.host()=="github.com" && url.userInfo().isEmpty() && (url.port()==-1||url.port()==443),"Enter a GitHub repository URL or owner/repository.");
        result=url.path();if(result.startsWith('/'))result.remove(0,1);if(result.endsWith('/'))result.chop(1);
    }
    require(valid_repository_name(result),"Enter a GitHub repository URL or owner/repository.");return result;
}
QList<Source> sources() {
    QList<Source> result{official_source()};QSet<qint64> seen{official_source().id};
    const auto values=get_tool_settings_manager()->get_app_setting("plugin_market_sources").toList();
    for(const auto &value:values) {
        const auto row=QJsonObject::fromVariantMap(value.toMap());
        const auto id=qint64(row.value("id").toDouble());const auto name=row.value("full_name").toString();
        if(id<=0 || seen.contains(id) || !valid_repository_name(name) || result.size()>=16)continue;
        seen.insert(id);result.append({id,name});
    }
    return result;
}
bool save_sources(const QList<Source> &values) {
    QVariantList stored;QSet<qint64> seen;
    for(const auto &source:values) {
        if(source.id<=0 || seen.contains(source.id) || !valid_repository_name(source.full_name) || seen.size()>=16)return false;
        seen.insert(source.id);if(source.id==official_source().id)continue;
        stored.append(QJsonObject{{"id",source.id},{"full_name",source.full_name}}.toVariantMap());
    }
    get_tool_settings_manager()->set_app_setting("plugin_market_sources",stored);return true;
}
QJsonObject Entry::json() const {
    return {{"id",id},{"name",name},{"description",description},{"version",version},{"platform",platform},{"host",host},
        {"asset",asset},{"asset_id",asset_id},{"size",size},{"sha256",sha256},{"release_tag",release_tag},{"download_url",download_url},
        {"name_i18n",name_i18n},{"description_i18n",description_i18n}};
}
QList<Entry> parse_catalog(const QByteArray &bytes,const Source &source,const QString &host_version,const QString &platform) {
    require(bytes.size()<=CATALOG_LIMIT,"Marketplace catalog is too large.");const auto data=object(bytes);
    require(data.value("schema_version").toInt()==2 && positive_id(data.value("repository_id"))==source.id &&
        QRegularExpression("^[a-f0-9]{40}\\z").match(data.value("revision").toString()).hasMatch() && data.value("plugins").isArray(),"Marketplace repository identity does not match.");
    const auto plugins=data.value("plugins").toArray();require(plugins.size()<=4096,"Marketplace has too many entries.");
    QList<Entry> result;QSet<QString> keys;const auto icons=data.value("icons").toObject();
    for(const auto &value:plugins) {
        require(value.isObject(),"Invalid marketplace entry.");const auto p=value.toObject();Entry e;
        e.id=p.value("id").toString();e.version=p.value("version").toString();e.platform=p.value("platform").toString();
        e.host=p.value("host").toString();e.asset=p.value("asset").toString();e.release_tag=p.value("release_tag").toString();
        e.sha256=p.value("sha256").toString();e.download_url=p.value("download_url").toString();
        e.asset_id=positive_id(p.value("asset_id"));e.size=positive_id(p.value("size"));e.source=source;
        require(QRegularExpression("^[a-z0-9][a-z0-9._-]{0,63}\\z").match(e.id).hasMatch() && compare_plugin_versions(e.version,e.version).has_value() &&
            QStringList{"windows-x86_64","windows-arm64"}.contains(e.platform) && e.size<=PACKAGE_LIMIT &&
            QRegularExpression("^[a-f0-9]{64}\\z").match(e.sha256).hasMatch(),"Invalid marketplace package metadata.");
        require(!e.host.isEmpty() && plugin_version_satisfies(host_version,e.host).has_value(),"Invalid host version requirement.");
        require(e.release_tag==e.id+"/v"+e.version && e.asset==e.id+"-"+e.version+"-"+e.platform+".jtbplugin","Marketplace package tag or name does not match.");
        const QUrl url(e.download_url);const QString path=url.path(QUrl::FullyDecoded);const auto start=path.indexOf("/releases/download/");
        require(url.isValid() && url.scheme()=="https" && url.host()=="github.com" && url.userInfo().isEmpty() && (url.port()==-1||url.port()==443) &&
            url.query().isEmpty() && url.fragment().isEmpty() && start>0 && valid_repository_name(path.mid(1,start-1)) &&
            path.mid(start+19)==e.release_tag+"/"+e.asset,"Invalid marketplace package URL.");
        const auto key=e.id+'\n'+e.version+'\n'+e.platform;require(!keys.contains(key),"Duplicate marketplace package identity.");keys.insert(key);
        e.name=p.value("name").toString(e.id).left(160);e.description=p.value("description").toString().left(1000);
        e.name_i18n=p.value("name_i18n").toObject();e.description_i18n=p.value("description_i18n").toObject();
        const auto icon=icons.value(e.id).toString();if(icon.size()<=90*1024)e.icon=QByteArray::fromBase64(icon.toLatin1());
        e.platform_matches=e.platform==platform;e.compatible=e.platform_matches && plugin_version_satisfies(host_version,e.host).value_or(false);
        result.append(e);
    }
    return result;
}
QList<Entry> select_entries(const QList<Entry> &entries) {
    QMap<QString,Entry> best;
    for(const auto &entry:entries) {
        if(!stable(entry.version))continue;
        auto old=best.find(entry.id);
        if(old==best.end() || (entry.compatible&&!old->compatible) ||
           (entry.compatible==old->compatible && entry.platform_matches&&!old->platform_matches) ||
           (entry.compatible==old->compatible && entry.platform_matches==old->platform_matches && compare_plugin_versions(entry.version,old->version).value_or(0)>0))best[entry.id]=entry;
    }
    return best.values();
}
QMap<QString,Entry> choose_updates(const QList<InstalledPlugin> &installed,const QList<Catalog> &catalogs,const QSet<qint64> &failed,const QString &root) {
    QMap<QString,Entry> result;bool custom_failed=false;for(auto id:failed)custom_failed|=id!=official_source().id;
    for(const auto &plugin:installed) {
        const auto origin=plugin_install_source(plugin.id,root);const auto bound=qint64(origin.value("repository_id").toDouble());
        std::optional<Entry> official,custom;
        for(const auto &catalog:catalogs)for(const auto &entry:select_entries(catalog.entries)) {
            if(entry.id!=plugin.id||!entry.compatible)continue;
            if(bound && bound!=official_source().id && entry.source.id!=bound)continue;
            auto &candidate=entry.source.id==official_source().id?official:custom;
            if(!candidate || compare_plugin_versions(entry.version,candidate->version).value_or(0)>0)candidate=entry;
        }
        const auto chosen=custom?custom:custom_failed?std::nullopt:official;
        if(chosen) {
            const auto cmp=compare_plugin_versions(chosen->version,plugin.version).value_or(-1);
            if(cmp>0 || (cmp==0&&!plugin.compatible))result.insert(plugin.id,*chosen);
        }
    }
    return result;
}
Client::Client(QString root,Request request):root_(root.isEmpty()?plugin_directory():std::move(root)),request_(request?std::move(request):github_request) {}
QString Client::cache_directory() const {
    const QString path=QDir(root_).filePath(".market-cache");
    for(const auto &part:{root_,path})require(!QFileInfo(part).isSymLink()&&!QFileInfo(part).isJunction(),"Unsafe marketplace cache path.");
    require(QDir().mkpath(path),"Could not create marketplace cache.");return path;
}
Source Client::resolve(const QString &input) const { return repository(object(request_("repos/"+repository_input(input),false,CATALOG_LIMIT))); }
Source Client::resolve(qint64 id) const {
    require(id>0,"Invalid GitHub repository identity.");const auto result=repository(object(request_("repositories/"+QString::number(id),false,CATALOG_LIMIT)));
    require(result.id==id,"GitHub repository identity changed.");return result;
}
Catalog Client::catalog(const Source &source,bool refresh) const {
    const auto file=QDir(cache_directory()).filePath(QString::number(source.id)+".json");
    if(!refresh) {
        try {
            const auto cached=object(read(file,CATALOG_LIMIT+65536));const auto age=QDateTime::currentMSecsSinceEpoch()-qint64(cached.value("fetched_at").toDouble());
            if(age>=0&&age<15*60*1000 && cached.value("host_version").toString()==APP_VERSION) {
                const auto payload=QJsonDocument(cached.value("catalog").toObject()).toJson(QJsonDocument::Compact);
                return {source,parse_catalog(payload,source,APP_VERSION,plugin_platform()),true};
            }
        } catch(const std::exception &) {}
    }
    const auto canonical=resolve(source.id);QByteArray payload;QList<QPair<qint64,QJsonObject>> catalogs;
    for(int page=1;page<=10;++page) {
        const auto doc=QJsonDocument::fromJson(request_("repos/"+canonical.full_name+"/releases?per_page=100&page="+QString::number(page),false,CATALOG_LIMIT));
        require(doc.isArray(),"Invalid GitHub releases response.");const auto releases=doc.array();
        for(const auto &value:releases) {
            const auto release=value.toObject();if(release.value("draft").toBool()||release.value("prerelease").toBool())continue;
            for(const auto &asset_value:release.value("assets").toArray()) {
                const auto asset=asset_value.toObject();if(asset.value("name")!="plugin-market.json")continue;
                require(positive_id(asset.value("size"))<=CATALOG_LIMIT,"Marketplace catalog is too large.");
                catalogs.append({QDateTime::fromString(release.value("published_at").toString(),Qt::ISODate).toMSecsSinceEpoch(),asset});
                break;
            }
        }
        if(releases.size()<100)break;
    }
    if(!catalogs.isEmpty()) {
        std::stable_sort(catalogs.begin(),catalogs.end(),[](const auto &a,const auto &b){return a.first>b.first;});
        const auto asset=catalogs.front().second;
        payload=request_("repos/"+canonical.full_name+"/releases/assets/"+QString::number(positive_id(asset.value("id"))),true,CATALOG_LIMIT);
        const auto digest=asset.value("digest").toString();require(digest.isEmpty()||digest=="sha256:"+sha256(payload),"Marketplace catalog checksum mismatch.");
    }
    require(!payload.isEmpty(),"This repository has not published a plugin marketplace.");
    auto entries=parse_catalog(payload,canonical,APP_VERSION,plugin_platform());
    write(file,QJsonDocument(QJsonObject{{"fetched_at",QDateTime::currentMSecsSinceEpoch()},{"host_version",APP_VERSION},{"catalog",object(payload)}}).toJson(QJsonDocument::Compact));
    return {canonical,entries,false};
}
QString Client::download(const Entry &entry,Source *canonical_source) const {
    require(entry.compatible,"This plugin is not compatible with this application.");const auto source=resolve(entry.source.id);
    if(canonical_source)*canonical_source=source;
    const auto release=object(request_("repos/"+source.full_name+"/releases/tags/"+encoded(entry.release_tag),false,CATALOG_LIMIT));
    require(!release.value("draft").toBool()&&!release.value("prerelease").toBool()&&release.value("tag_name")==entry.release_tag,"Plugin release is not published.");
    QJsonObject asset;
    for(const auto &value:release.value("assets").toArray())if(qint64(value.toObject().value("id").toDouble())==entry.asset_id)asset=value.toObject();
    require(!asset.isEmpty()&&asset.value("name")==entry.asset&&positive_id(asset.value("size"))==entry.size,"Plugin release asset changed. Refresh the marketplace.");
    const auto digest=asset.value("digest").toString();require(digest.isEmpty()||digest=="sha256:"+entry.sha256,"Plugin package checksum mismatch.");
    const QString path=QDir(cache_directory()).filePath(entry.sha256+".jtbplugin");
    const auto old=read(path,PACKAGE_LIMIT);
    if(old.size()==entry.size && sha256(old)==entry.sha256)return path;
    const auto bytes=request_("repos/"+source.full_name+"/releases/assets/"+QString::number(entry.asset_id),true,PACKAGE_LIMIT);
    require(bytes.size()==entry.size&&sha256(bytes)==entry.sha256,"Plugin package checksum mismatch.");
    write(path,bytes);return path;
}
}
