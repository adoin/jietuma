#pragma once
#include "plugins/plugin_package.h"
#include <QMap>
#include <QSet>
#include <QUrl>
#include <functional>
#include <stdexcept>

namespace jietuba::plugins::market {
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Source {
    qint64 id = 0;
    QString full_name;
    bool operator==(const Source &) const = default;
};
struct Entry {
    QString id, name, description, version, platform, host, asset, sha256, release_tag, download_url;
    qint64 asset_id = 0, size = 0;
    QJsonObject name_i18n, description_i18n;
    QByteArray icon;
    Source source;
    bool compatible = false;
    bool platform_matches = false;
    QJsonObject json() const;
};
struct Catalog { Source source; QList<Entry> entries; bool cached = false; };
using Request = std::function<QByteArray(const QString &endpoint, bool binary, qint64 limit)>;
Source official_source();
QList<Source> sources();
bool save_sources(const QList<Source> &sources);
bool valid_repository_name(const QString &name);
QString repository_input(const QString &text);
QList<Entry> parse_catalog(const QByteArray &bytes, const Source &source, const QString &host_version,
                           const QString &platform);
QList<Entry> select_entries(const QList<Entry> &entries);
QMap<QString, Entry> choose_updates(const QList<InstalledPlugin> &installed, const QList<Catalog> &catalogs,
                                    const QSet<qint64> &failed_sources, const QString &root = {});
// gh authentication is used when available; public repositories also work via system WinHTTP.
QByteArray github_request(const QString &endpoint, bool binary, qint64 limit);
class Client {
public:
    explicit Client(QString root = {}, Request request = {});
    Source resolve(const QString &input) const;
    Source resolve(qint64 repository_id) const;
    Catalog catalog(const Source &source, bool refresh = false) const;
    QString download(const Entry &entry,Source *canonical_source=nullptr) const;
private:
    QString root_;
    Request request_;
    QString cache_directory() const;
};
}
