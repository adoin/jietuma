#pragma once
#include "plugins/plugin_market.h"
#include <QWidget>
#include <memory>

namespace jietuba::plugins {
class PluginMarketWidget : public QWidget {
    Q_OBJECT
public:
    PluginMarketWidget(QWidget *installed_page,QString root={},std::shared_ptr<market::Client> client={},QWidget *parent=nullptr);
    ~PluginMarketWidget() override;
    std::optional<market::Entry> update_for(const QString &id) const;
    void check_updates();
    void install(const market::Entry &entry);
signals:
    void installed_changed();
    void updates_changed();
private:
    struct Impl;
    std::unique_ptr<Impl> d_;
    void load_current(bool refresh=false);
    void refresh_sources();
    void set_busy(bool busy);
};
}
