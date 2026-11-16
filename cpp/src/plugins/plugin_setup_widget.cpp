#include "plugins/plugin_setup_widget.h"
#include "plugins/plugin_manager_dialog.h"
#include "core/background_tasks.h"
#include "core/i18n.h"
#include "core/ui_scale.h"
#include "core/ui_theme.h"
#include "ui/fluent_lite/buttons.h"
#include <QApplication>
#include <QBuffer>
#include <QHideEvent>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QVBoxLayout>
#include <atomic>

namespace jietuba::plugins {
namespace {
QString text(const char *key){return i18n_tr(key,"PluginSetup");}
QString localized(const QJsonObject &values,const QString &fallback){return values.value(I18nManager::get_current_language()).toString(fallback);}
template<class Work,class Done> void run(PluginSetupWidget *owner,std::shared_ptr<std::atomic_bool> cancelled,Work work,Done done) {
    const QPointer<PluginSetupWidget> guard(owner);const QPointer<QObject> dispatcher(qApp);
    using Result=decltype(work());
    const auto callback=std::make_shared<Done>(std::move(done));
    try {background_tasks::start_thread([guard,dispatcher,cancelled,work=std::move(work),callback]()mutable{
        std::optional<Result> result;QString error;
        try{result=work();}catch(const std::exception &failure){error=QString::fromUtf8(failure.what());}
        if(!cancelled->load()&&dispatcher)QMetaObject::invokeMethod(dispatcher.data(),[guard,cancelled,result=std::move(result),error,callback]()mutable{
            if(guard&&!cancelled->load())(*callback)(guard.data(),std::move(result),error);
        },Qt::QueuedConnection);
    },"PluginSetup");}
    catch(const std::exception &error){if(!cancelled->load())(*callback)(owner,std::optional<Result>{},QString::fromUtf8(error.what()));}
}
}
struct PluginSetupWidget::Impl {
    QString root;std::shared_ptr<market::Client> client;
    std::shared_ptr<std::atomic_bool> cancelled=std::make_shared<std::atomic_bool>(false);
    QLabel *title=nullptr,*description=nullptr,*status=nullptr;
    QListWidget *list=nullptr;QPushButton *browse=nullptr,*stop=nullptr;
    QList<market::Entry> entries;QByteArray status_key;QString detail;
    bool busy=false,loaded=false;
};
PluginSetupWidget::PluginSetupWidget(QWidget *parent,QString root,std::shared_ptr<market::Client> client)
    :QWidget(parent),d_(std::make_unique<Impl>()) {
    setObjectName("pluginSetup");
    d_->root=root.isEmpty()?plugin_directory():std::move(root);d_->client=client?std::move(client):std::make_shared<market::Client>(d_->root);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(dialog_scaled(8));
    d_->title=new QLabel(this);auto title_font=d_->title->font();title_font.setBold(true);d_->title->setFont(title_font);d_->description=new QLabel(this);d_->description->setWordWrap(true);
    layout->addWidget(d_->title);layout->addWidget(d_->description);
    d_->browse=new SecondaryPushButton(QString(),this);d_->browse->setObjectName("browseSetupPlugins");layout->addWidget(d_->browse);
    d_->status=new QLabel(this);d_->status->setTextFormat(Qt::PlainText);d_->status->setWordWrap(true);d_->status->setObjectName("pluginSetupStatus");layout->addWidget(d_->status);
    d_->list=new QListWidget(this);d_->list->setObjectName("pluginSetupList");d_->list->setWordWrap(true);d_->list->setIconSize(QSize(dialog_scaled(24),dialog_scaled(24)));d_->list->setMinimumHeight(dialog_scaled(100));layout->addWidget(d_->list,1);
    d_->stop=new SecondaryPushButton(QString(),this);d_->stop->setObjectName("cancelSetupPlugins");d_->stop->hide();layout->addWidget(d_->stop);
    connect(d_->browse,&QPushButton::clicked,this,[this]{load();});
    connect(d_->stop,&QPushButton::clicked,this,[this]{cancel();});
    connect(d_->list,&QListWidget::itemChanged,this,[this]{emit selection_changed();});
    const auto theme=[this]{const auto &t=get_ui_theme()->tokens();
        apply_style_sheet(this,QString("QListWidget#pluginSetupList { background: %1; color: %2; border: none; border-radius: 8px; padding: 4px; } QListWidget#pluginSetupList::item { border: none; padding: 5px; border-radius: 6px; } QListWidget#pluginSetupList::item:selected { background: %3; color: %4; }").arg(t.window,t.text,t.accent_soft,t.accent_text));};
    theme();connect(get_ui_theme(),&UIThemeManager::theme_changed,this,theme);
    connect(I18nManager::instance().language_signals(),&I18nSignals::language_changed,this,[this]{retranslate();});
    retranslate();
}
PluginSetupWidget::~PluginSetupWidget(){d_->cancelled->store(true);}
void PluginSetupWidget::retranslate(){
    d_->title->setText(text("Optional plugins"));
    d_->description->setText(text("Choose features to add. You can skip installation and add plugins later in Settings."));
    d_->browse->setText(text(d_->loaded?"Refresh":"Browse official plugins"));d_->stop->setText(text("Cancel download"));
    d_->status->setText(d_->status_key.isEmpty()?QString():(d_->detail.isEmpty()?text(d_->status_key.constData()):text(d_->status_key.constData()).arg(d_->detail)));render();
}
void PluginSetupWidget::status(const char *key,const QString &detail){d_->status_key=key;d_->detail=detail;d_->status->setText(detail.isEmpty()?text(key):text(key).arg(detail));}
bool PluginSetupWidget::busy()const{return d_->busy;}
int PluginSetupWidget::selected_count()const{int count=0;for(int i=0;i<d_->list->count();++i)count+=d_->list->item(i)->checkState()==Qt::Checked;return count;}
void PluginSetupWidget::set_busy(bool value){d_->busy=value;d_->browse->setEnabled(!value);d_->list->setEnabled(!value);d_->stop->setVisible(value);emit busy_changed();}
void PluginSetupWidget::cancel(){d_->cancelled->store(true);if(d_->busy){set_busy(false);status("Cancelled. You can retry or finish without plugins.");}}
void PluginSetupWidget::hideEvent(QHideEvent *event){cancel();QWidget::hideEvent(event);}
void PluginSetupWidget::render(){
    QSet<QString> selected;for(int i=0;i<d_->list->count();++i)if(d_->list->item(i)->checkState()==Qt::Checked)selected.insert(d_->list->item(i)->data(Qt::UserRole).toString());
    d_->list->blockSignals(true);d_->list->clear();QMap<QString,InstalledPlugin> installed;for(const auto &p:installed_plugins(d_->root))installed.insert(p.id,p);
    for(const auto &entry:d_->entries){
        const auto found=installed.constFind(entry.id);const bool already=found!=installed.cend()&&found->compatible&&compare_plugin_versions(found->version,entry.version).value_or(-1)>=0;
        QString name=localized(entry.name_i18n,entry.name).left(120)+"  "+entry.version;
        if(found!=installed.cend())name+=" · "+text("Installed: %1").arg(found->version);
        const auto description=localized(entry.description_i18n,entry.description);
        auto *row=new QListWidgetItem(name+"\n"+description.left(80),d_->list);row->setData(Qt::UserRole,entry.id);row->setToolTip(description.toHtmlEscaped());
        row->setSizeHint(QSize(0,dialog_scaled(86)));
        if(already)row->setFlags(row->flags()&~Qt::ItemIsUserCheckable);
        else {row->setFlags(row->flags()|Qt::ItemIsUserCheckable);row->setCheckState(selected.contains(entry.id)?Qt::Checked:Qt::Unchecked);}
        QBuffer buffer;buffer.setData(entry.icon);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);const auto size=reader.size();
        if(size.isValid()&&size.width()<=512&&size.height()<=512)row->setIcon(QIcon(QPixmap::fromImage(reader.read())));
    }
    d_->list->blockSignals(false);emit selection_changed();
}
void PluginSetupWidget::load(){
    if(d_->busy)return;d_->cancelled=std::make_shared<std::atomic_bool>(false);set_busy(true);status("Loading plugins…");const auto client=d_->client;
    run(this,d_->cancelled,[client]{return client->catalog(market::official_source(),true);},[](PluginSetupWidget *self,std::optional<market::Catalog> catalog,const QString &error){
        self->set_busy(false);if(!catalog){self->status("Could not load plugins: %1",error);return;}
        self->d_->entries.clear();for(const auto &entry:market::select_entries(catalog->entries))if(entry.compatible)self->d_->entries.append(entry);
        self->d_->loaded=true;self->retranslate();self->status(self->d_->entries.isEmpty()?"No compatible plugins are available.":"Select plugins, then choose Install and finish.");
    });
}
void PluginSetupWidget::install_selected(){
    if(d_->busy)return;QSet<QString> selected;for(int i=0;i<d_->list->count();++i)if(d_->list->item(i)->checkState()==Qt::Checked)selected.insert(d_->list->item(i)->data(Qt::UserRole).toString());
    QList<market::Entry> entries;for(const auto &entry:d_->entries)if(selected.contains(entry.id))entries.append(entry);if(entries.isEmpty())return;
    d_->cancelled=std::make_shared<std::atomic_bool>(false);const auto token=d_->cancelled;set_busy(true);status("Downloading selected plugins…");const auto client=d_->client;
    struct Downloaded {QStringList paths;QMap<QString,PluginInstallOptions> options;};
    run(this,token,[client,token,entries]{Downloaded result;for(const auto &entry:entries){
        if(token->load())break;market::Source source;const auto path=client->download(entry,&source);if(token->load())break;
        PluginInstallOptions options;options.source={{"repository_id",source.id},{"full_name",source.full_name},{"asset_id",entry.asset_id},{"release_tag",entry.release_tag}};
        options.expected_id=entry.id;options.expected_version=entry.version;options.expected_sha256=entry.sha256;options.expected_host=entry.host;options.expected_platform=entry.platform;
        result.paths.append(path);result.options.insert(entry.id,options);
    }return result;},[entries](PluginSetupWidget *self,std::optional<Downloaded> result,const QString &error){
        if(!result){self->set_busy(false);self->status("Could not download plugins: %1",error);return;}
        for(const auto &p:installed_plugins(self->d_->root))if(result->options.contains(p.id))result->options[p.id].reinstall_equal=compare_plugin_versions(p.version,result->options[p.id].expected_version).value_or(-1)==0;
        const QPointer<PluginSetupWidget> guard(self);
        const bool changed=install_plugin_packages_interactively(self,result->paths,self->d_->root,{},false,result->options);
        if(!guard)return;
        self->set_busy(false);self->render();
        bool complete=changed;const auto installed=installed_plugins(self->d_->root);
        for(const auto &entry:entries){bool present=false;for(const auto &p:installed)if(p.id==entry.id&&p.compatible&&compare_plugin_versions(p.version,entry.version).value_or(-1)>=0)present=true;complete&=present;}
        if(complete){self->status("Plugins installed.");emit self->completed();}
        else self->status("Installation was cancelled or incomplete. You can retry or skip.");
    });
}
}
