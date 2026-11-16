#pragma once
#include "plugins/plugin_market.h"
#include <QWidget>
#include <memory>

namespace jietuba::plugins {
// Optional, explicitly loaded official plugin selection for the welcome wizard.
class PluginSetupWidget : public QWidget {
    Q_OBJECT
public:
    explicit PluginSetupWidget(QWidget *parent=nullptr,QString root={},std::shared_ptr<market::Client> client={});
    ~PluginSetupWidget() override;
    int selected_count() const;
    bool busy() const;
    void install_selected();
    void cancel();
signals:
    void selection_changed();
    void busy_changed();
    void completed();
protected:
    void hideEvent(QHideEvent *event) override;
private:
    struct Impl;
    std::unique_ptr<Impl> d_;
    void load();
    void render();
    void set_busy(bool value);
    void retranslate();
    void status(const char *key,const QString &detail={});
};
}
