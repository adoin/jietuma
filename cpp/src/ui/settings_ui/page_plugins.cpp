#include "ui/settings_ui/dialog.h"
#include "ui/settings_ui/pages.h"
#include "plugins/plugin_manager_dialog.h"

namespace jietuba {
QWidget *create_plugins_page(SettingsDialog *dialog) {
    auto *page=plugins::create_plugin_manager(dialog);
    dialog->set_attr("plugin_manager_page",page);
    return page;
}
}
