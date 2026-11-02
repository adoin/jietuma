#include "plugins/plugin_manager_dialog.h"
#include "plugins/plugin_package.h"
#include "core/i18n.h"
#include "core/ui_scale.h"
#include "core/ui_theme.h"
#include "ui/dialogs.h"
#include "ui/fluent_lite/fluent_lite.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QImageReader>
#include <QStandardPaths>
#include "settings/tool_settings.h"

namespace jietuba::plugins {
namespace {
QString tr(const char *text) { return i18n_tr(text, "ClipboardWindow"); }

void style_dialog(QDialog &dialog) {
    const auto &t = get_ui_theme()->tokens();
    apply_style_sheet(&dialog, QString(R"(
        QDialog#pluginManagerDialog, QDialog#pluginSettingsDialog { background: %1; color: %2; }
        QListWidget#pluginList, QPlainTextEdit { background: %3; color: %2; border: 1px solid %4; border-radius: %5px; padding: %6px; }
        QListWidget#pluginList::item { padding: %7px; border-radius: %8px; }
        QListWidget#pluginList::item:selected { background: %9; color: %2; }
        QPushButton { background: %3; color: %2; border: 1px solid %4; border-radius: %8px; padding: %6px %10px; min-height: %11px; }
        QPushButton:hover { background: %9; }
        QPushButton:disabled { color: %12; }
    )").arg(t.window, t.text, t.input_background, t.border).arg(scaled(8)).arg(scaled(6)).arg(scaled(9)).arg(scaled(6))
              .arg(t.accent_soft).arg(scaled(14)).arg(scaled(24)).arg(t.text_disabled));
}

void edit_settings(QWidget *parent, const QString &id, const QJsonArray &settings, const QString &root) {
    QDialog dialog(parent);
    dialog.setObjectName("pluginSettingsDialog");
    style_dialog(dialog);
    dialog.setWindowTitle(tr("Plugin Settings"));
    dialog.resize(scaled(480), scaled(320));
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(scaled(16), scaled(16), scaled(16), scaled(16));
    layout->setSpacing(scaled(10));
    auto *scroll = new QScrollArea(&dialog);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto *fields = new QWidget;
    auto *fields_layout = new QVBoxLayout(fields);
    fields_layout->setContentsMargins(0, 0, 0, 0);
    fields_layout->setSpacing(scaled(8));
    scroll->setWidget(fields);
    layout->addWidget(scroll, 1);
    const auto current = read_plugin_config(id, root);
    QMap<QString, QWidget *> editors;
    const QRegularExpression valid_key("^[a-z][a-z0-9_]{0,63}$");
    for (const auto &value : settings) {
        const auto spec = value.toObject();
        const QString key = spec.value("key").toString();
        if (!valid_key.match(key).hasMatch() || !QStringList{"multiline","boolean","choice","number"}.contains(spec.value("type").toString()) || editors.contains(key)) continue;
        const QString label = spec.value("label_i18n").toObject().value(I18nManager::get_current_language()).toString(spec.value("label").toString(key));
        auto *caption = new QLabel(label, &dialog);
        caption->setTextFormat(Qt::PlainText);
        QWidget *editor=nullptr;
        const auto setting_value=current.value(key).isUndefined()?spec.value("default"):current.value(key);
        const auto type=spec.value("type").toString();
        if(type=="boolean") {auto *field=new CheckBox(QString(),fields);field->setChecked(setting_value.toBool());editor=field;}
        else if(type=="number") {auto *field=new QSpinBox(fields);field->setRange(spec.value("minimum").toInt(0),spec.value("maximum").toInt(1000000));field->setValue(setting_value.toInt());editor=field;}
        else if(type=="choice") {auto *field=new QComboBox(fields);for(const auto &entry:spec.value("choices").toArray()) {const auto choice=entry.toObject();field->addItem(choice.value("name_i18n").toObject().value(I18nManager::get_current_language()).toString(choice.value("name").toString()),choice.value("value").toString());}field->setCurrentIndex(qMax(0,field->findData(setting_value.toString())));editor=field;}
        else {auto *field=new QPlainTextEdit(fields);field->setPlainText(setting_value.toString());editor=field;}
        editor->setObjectName("pluginSetting_"+key);
        caption->setWordWrap(true);fields_layout->addWidget(caption);fields_layout->addWidget(editor,type=="multiline"?1:0);editors.insert(key,editor);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save Changes"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QJsonObject config = current;
        for(auto it=editors.cbegin();it!=editors.cend();++it) {
            if(auto *text_editor=qobject_cast<QPlainTextEdit *>(it.value())) config.insert(it.key(),text_editor->toPlainText());
            else if(auto *check_editor=qobject_cast<QCheckBox *>(it.value())) config.insert(it.key(),check_editor->isChecked());
            else if(auto *number_editor=qobject_cast<QSpinBox *>(it.value())) config.insert(it.key(),number_editor->value());
            else if(auto *choice_editor=qobject_cast<QComboBox *>(it.value())) config.insert(it.key(),QJsonValue::fromVariant(choice_editor->currentData()));
        }
        if (!save_plugin_config(id, config, root)) { show_warning_dialog(&dialog, tr("Plugin Settings"), tr("Could not save plugin settings.")); return; }
        dialog.accept();
    });
    exec_dialog(&dialog, parent);
}
}

void install_plugin_interactively(QWidget *parent, const QString &plugin_root) {
    QString folder=get_tool_settings_manager()->get_app_setting("plugin_install_directory").toString();
    if(!QDir(folder).exists()||folder.isEmpty())folder=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QFileDialog chooser(parent,tr("Install Plugin"),folder,tr("Plugin Packages (*.jtbplugin)"));
    chooser.setFileMode(QFileDialog::ExistingFiles);
    if(exec_dialog(&chooser)!=QDialog::Accepted || chooser.selectedFiles().isEmpty())return;
    get_tool_settings_manager()->set_app_setting("plugin_install_directory",QFileInfo(chooser.selectedFiles().front()).absolutePath());
    install_plugin_packages_interactively(parent,chooser.selectedFiles(),plugin_root);
}

void install_plugin_packages_interactively(QWidget *parent,const QStringList &paths,const QString &plugin_root) {
    const auto plan=plan_plugin_install(paths,plugin_root);if(plan.isEmpty())return;
    get_tool_settings_manager()->set_app_setting("plugin_install_directory",QFileInfo(paths.front()).absolutePath());
    StandardDialog dialog(parent,tr("Install Plugin"),tr("Plugins can access screenshots, clipboard contents and files. Install only plugins you trust."));
    dialog.setObjectName("standardDialog");dialog.resize(scaled(590),scaled(420));
    auto *list=new QListWidget(&dialog);list->setObjectName("pluginInstallList");list->setIconSize(QSize(scaled(36),scaled(36)));
    const auto &tokens=get_ui_theme()->tokens();
    apply_style_sheet(list,QString("QListWidget { background: %1; color: %2; border: none; border-radius: 8px; padding: 8px; } QListWidget::item { padding: 6px; border-radius: 6px; } QListWidget::item:selected { background: %3; }").arg(tokens.window,tokens.text,tokens.accent_soft));
    int eligible=0;
    for(const auto &entry:plan) {
        const auto &package=entry.package;
        const QString name=package.name.isEmpty()?QFileInfo(package.path).fileName():package.name;
        QString description=entry.status;
        if(package.ok)description=tr(entry.status.toUtf8().constData());
        auto *item=new QListWidgetItem(name+"  "+package.version+"\n"+(entry.installed_version.isEmpty()?QString():tr("Installed: %1").arg(entry.installed_version)+" · ")+description,list);
        QBuffer image_data;image_data.setData(package.icon);image_data.open(QIODevice::ReadOnly);QImageReader reader(&image_data);
        const auto size=reader.size();
        const auto image=size.isValid()&&size.width()<=1024&&size.height()<=1024?reader.read():QImage();
        item->setIcon(image.isNull()?QIcon(":/app.ico"):QIcon(QPixmap::fromImage(image)));
        item->setSizeHint(QSize(0,scaled(64)));item->setToolTip(package.id+"\n"+package.path);eligible+=entry.installable;
    }
    static_cast<QVBoxLayout *>(dialog.layout())->insertWidget(1,list,1);
    auto *install=dialog.add_button(tr("Install %1 plugins").arg(eligible),QDialogButtonBox::AcceptRole);install->setEnabled(eligible>0);
    auto *cancel=dialog.add_button(tr("Cancel"),QDialogButtonBox::RejectRole);cancel->setDefault(true);cancel->setFocus();
    QObject::connect(install,&QPushButton::clicked,&dialog,&QDialog::accept);QObject::connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
    if(exec_dialog(&dialog)!=QDialog::Accepted)return;
    int installed=0,skipped=0;QStringList errors;
    for(const auto &entry:plan) {
        if(!entry.installable){++skipped;continue;}
        const auto result=install_plugin_package(entry.package.path,plugin_root);
        if(!result.ok)errors<<entry.package.name+": "+tr(result.error.toUtf8().constData());
        else if(result.skipped)++skipped;else ++installed;
    }
    QString summary=tr("Installed: %1; skipped: %2").arg(installed).arg(skipped);
    if(!errors.isEmpty())show_warning_dialog(parent,tr("Install Plugin"),summary+"\n\n"+errors.join('\n'));
    else show_info_dialog(parent,tr("Install Plugin"),summary);
}

void show_plugin_manager(QWidget *parent, const QString &plugin_root) {
    QDialog dialog(parent);
    dialog.setObjectName("pluginManagerDialog");
    style_dialog(dialog);
    dialog.setWindowTitle(tr("Manage Plugins"));
    dialog.resize(scaled(480), scaled(360));
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(scaled(16), scaled(16), scaled(16), scaled(16));
    layout->setSpacing(scaled(12));
    auto *list = new QListWidget(&dialog);
    list->setObjectName("pluginList");
    list->setIconSize(QSize(scaled(32),scaled(32)));
    layout->addWidget(list);
    auto *buttons = new QHBoxLayout;
    auto *add = new PrimaryPushButton(tr("Install Plugin"), &dialog);
    add->setObjectName("installPlugins");
    auto *remove = new QPushButton(tr("Uninstall"), &dialog);
    remove->setObjectName("uninstallPlugin");
    auto *settings = new QPushButton(tr("Plugin Settings"), &dialog);
    settings->setObjectName("pluginSettings");
    auto *close = new QPushButton(tr("Close"), &dialog);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addWidget(settings);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    auto refresh = [&] {
        list->blockSignals(true);
        list->clear();
        for (const auto &plugin : installed_plugins(plugin_root)) {
            const QString caption = plugin.version.isEmpty() ? plugin.name : plugin.name + "  " + plugin.version;
            auto *row = new QListWidgetItem(caption, list);
            row->setIcon(plugin.icon_path.isEmpty()?QIcon(":/app.ico"):QIcon(plugin.icon_path));
            row->setData(Qt::UserRole, plugin.id);
            row->setData(Qt::UserRole + 1, QJsonDocument(plugin.settings).toJson(QJsonDocument::Compact));
            row->setToolTip(plugin.id);
            row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
            row->setCheckState(plugin.enabled ? Qt::Checked : Qt::Unchecked);
        }
        list->blockSignals(false);
        remove->setEnabled(list->currentItem() != nullptr);
        settings->setEnabled(list->currentItem() && !QJsonDocument::fromJson(list->currentItem()->data(Qt::UserRole + 1).toByteArray()).array().isEmpty());
    };
    QObject::connect(list, &QListWidget::currentItemChanged, &dialog, [=](QListWidgetItem *item) {
        remove->setEnabled(item != nullptr);
        settings->setEnabled(item && !QJsonDocument::fromJson(item->data(Qt::UserRole + 1).toByteArray()).array().isEmpty());
    });
    QObject::connect(list, &QListWidget::itemChanged, &dialog, [&](QListWidgetItem *item) {
        if (!set_plugin_enabled(item->data(Qt::UserRole).toString(), item->checkState() == Qt::Checked, plugin_root)) {
            show_warning_dialog(&dialog, tr("Manage Plugins"), tr("Could not change plugin state."));
            refresh();
        }
    });
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&] {
        auto *item = list->currentItem();
        if (!item || !show_confirm_dialog(&dialog, tr("Uninstall"), tr("Uninstall plugin %1?").arg(item->text()))) return;
        if (!uninstall_plugin(item->data(Qt::UserRole).toString(), plugin_root)) show_warning_dialog(&dialog, tr("Uninstall"), tr("Could not uninstall plugin."));
        refresh();
    });
    QObject::connect(settings, &QPushButton::clicked, &dialog, [&] {
        if (auto *item = list->currentItem()) edit_settings(&dialog, item->data(Qt::UserRole).toString(),
                                                           QJsonDocument::fromJson(item->data(Qt::UserRole + 1).toByteArray()).array(), plugin_root);
    });
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(add,&QPushButton::clicked,&dialog,[&]{install_plugin_interactively(&dialog,plugin_root);refresh();});
    refresh();
    exec_dialog(&dialog, parent);
}
}
