#include "plugins/plugin_market_widget.h"
#include "plugins/plugin_manager_dialog.h"
#include "core/background_tasks.h"
#include "core/i18n.h"
#include "core/ui_scale.h"
#include "core/ui_theme.h"
#include "ui/dialogs.h"
#include "ui/fluent_lite/fluent_lite.h"
#include <QApplication>
#include <QBuffer>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace jietuba::plugins {
namespace {
QString market_text(const char *text){return i18n_tr(text,"PluginMarket");}
QString localized(const QJsonObject &values,const QString &fallback){return values.value(I18nManager::get_current_language()).toString(fallback);}
QIcon icon(const QByteArray &bytes) {
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);const auto size=reader.size();
    if(!size.isValid()||size.width()>512||size.height()>512)return QIcon(":/app.ico");
    const auto image=reader.read();return image.isNull()?QIcon(":/app.ico"):QIcon(QPixmap::fromImage(image));
}
template<class Work,class Done> void job(PluginMarketWidget *owner,Work work,Done done) {
    const QPointer<PluginMarketWidget> guard(owner);const QPointer<QObject> dispatcher(qApp);
    using Result=decltype(work());
    auto callback=std::make_shared<Done>(std::move(done));
    try { background_tasks::start_thread([guard,dispatcher,work=std::move(work),callback]()mutable{
        std::optional<Result> result;QString error;
        try{result=work();}catch(const std::exception &failure){error=QString::fromUtf8(failure.what());}
        if(dispatcher)QMetaObject::invokeMethod(dispatcher.data(),[guard,result=std::move(result),error,callback]()mutable{
            if(guard)(*callback)(guard.data(),std::move(result),error);
        },Qt::QueuedConnection);
    },"PluginMarket");
    } catch(const std::exception &error) { (*callback)(owner,std::optional<Result>{},QString::fromUtf8(error.what())); }
}
}
struct PluginMarketWidget::Impl {
    struct Page {
        market::Source source;QWidget *widget=nullptr;LineEdit *search=nullptr;QListWidget *list=nullptr;
        QLabel *status=nullptr;QPushButton *refresh=nullptr,*install=nullptr;QList<market::Entry> entries;bool loaded=false;
    };
    QString root;std::shared_ptr<market::Client> client;QTabWidget *tabs=nullptr;LineEdit *repository=nullptr;
    QPushButton *add=nullptr,*remove=nullptr;QLabel *status=nullptr;QMap<qint64,Page> pages;bool busy=false;
    QMap<QString,market::Entry> updates;
    std::optional<market::Entry> selected(const Page &page) const {
        if(!page.list->currentItem())return std::nullopt;
        const auto id=page.list->currentItem()->data(Qt::UserRole).toString();
        for(const auto &entry:page.entries)if(entry.id==id)return entry;return std::nullopt;
    }
    void selection(Page &page) {
        const auto entry=selected(page);page.install->setEnabled(!busy&&entry&&entry->compatible);
        page.install->setText(market_text("Install"));if(!entry)return;
        for(const auto &installed:installed_plugins(root))if(installed.id==entry->id) {
            const auto cmp=compare_plugin_versions(entry->version,installed.version).value_or(-1);
            const auto origin=plugin_install_source(entry->id,root);
            const bool same=qint64(origin.value("repository_id").toDouble())==entry->source.id;
            if(cmp<0 || (cmp==0&&same&&installed.compatible)) {page.install->setText(market_text("Installed"));page.install->setEnabled(false);}
            else page.install->setText(cmp==0?market_text("Reinstall from this source"):market_text("Update"));
        }
    }
    void render(Page &page) {
        const auto previous=page.list->currentItem()?page.list->currentItem()->data(Qt::UserRole).toString():QString();
        page.list->clear();const QString needle=page.search->text().trimmed();
        QMap<QString,InstalledPlugin> installed;for(const auto &plugin:installed_plugins(root))installed.insert(plugin.id,plugin);
        for(const auto &entry:page.entries) {
            const auto name=localized(entry.name_i18n,entry.name).left(160);
            const auto description=localized(entry.description_i18n,entry.description).left(300);
            if(!needle.isEmpty()&&!(name+' '+entry.id+' '+description).contains(needle,Qt::CaseInsensitive))continue;
            QString text=name+"  "+entry.version;
            if(const auto found=installed.constFind(entry.id);found!=installed.cend()) {
                text+=" · "+market_text("Installed: %1").arg(found->version);
                if(!found->enabled)text+=" · "+market_text("Disabled");
            }
            text+="\n"+description;
            text+="\n"+(entry.compatible?entry.platform:market_text("Requires: %1 · %2").arg(entry.host,entry.platform));
            auto *row=new QListWidgetItem(icon(entry.icon),text,page.list);row->setData(Qt::UserRole,entry.id);row->setSizeHint(QSize(0,scaled(92)));
            row->setToolTip(entry.id.toHtmlEscaped());if(entry.id==previous)page.list->setCurrentItem(row);
        }
        if(!page.list->currentItem()&&page.list->count())page.list->setCurrentRow(0);
        selection(page);
    }
};
PluginMarketWidget::PluginMarketWidget(QWidget *installed,QString root,std::shared_ptr<market::Client> client,QWidget *parent)
    :QWidget(parent),d_(std::make_unique<Impl>()) {
    d_->root=root.isEmpty()?plugin_directory():std::move(root);d_->client=client?std::move(client):std::make_shared<market::Client>(d_->root);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(scaled(10));
    auto *repositories=new QHBoxLayout;d_->repository=new LineEdit(this);d_->repository->setObjectName("marketRepositoryInput");
    d_->repository->setPlaceholderText(market_text("GitHub owner/repository or repository URL"));
    d_->add=new SecondaryPushButton(market_text("Add source"),this);d_->add->setObjectName("addMarketSource");
    d_->remove=new PushButton(market_text("Remove source"),this);d_->remove->setObjectName("removeMarketSource");
    repositories->addWidget(d_->repository,1);repositories->addWidget(d_->add);repositories->addWidget(d_->remove);layout->addLayout(repositories);
    d_->tabs=new QTabWidget(this);d_->tabs->setObjectName("pluginCenterTabs");d_->tabs->addTab(installed,market_text("Installed"));layout->addWidget(d_->tabs,1);
    d_->status=new QLabel(this);d_->status->setObjectName("pluginMarketStatus");d_->status->setTextFormat(Qt::PlainText);d_->status->setWordWrap(true);layout->addWidget(d_->status);
    const auto apply_theme=[this] {
        const auto &t=get_ui_theme()->tokens();
        apply_style_sheet(this,QString("QTabWidget::pane { border: none; background: %1; } QTabBar::tab { border: none; background: transparent; color: %2; padding: 8px 12px; border-radius: 6px; } QTabBar::tab:selected { background: %3; color: %4; } QListWidget[pluginMarketList=\"true\"] { background: %1; color: %2; border: none; border-radius: 8px; padding: 6px; } QListWidget[pluginMarketList=\"true\"]::item { border: none; border-radius: 6px; padding: 8px; } QListWidget[pluginMarketList=\"true\"]::item:selected { background: %3; color: %4; }").arg(t.window,t.text,t.accent_soft,t.accent_text));
    };
    apply_theme();connect(get_ui_theme(),&UIThemeManager::theme_changed,this,apply_theme);
    connect(d_->tabs,&QTabWidget::currentChanged,this,[this]{load_current();set_busy(d_->busy);});
    connect(d_->add,&QPushButton::clicked,this,[this]{
        if(d_->busy)return;const auto text=d_->repository->text();set_busy(true);d_->status->setText(market_text("Looking up repository…"));
        const auto client=d_->client;
        job(this,[client,text]{return client->resolve(text);},[](PluginMarketWidget *self,std::optional<market::Source> source,const QString &error){
            self->set_busy(false);if(!source){self->d_->status->setText(market_text("Could not add source: %1").arg(market_text(error.toUtf8().constData())));return;}
            auto values=market::sources();for(const auto &existing:values)if(existing.id==source->id){self->d_->status->setText(market_text("This repository is already added."));return;}
            values.append(*source);if(!market::save_sources(values)){self->d_->status->setText(market_text("Could not save plugin sources."));return;}
            self->d_->repository->clear();self->refresh_sources();self->d_->tabs->setCurrentWidget(self->d_->pages[source->id].widget);
        });
    });
    connect(d_->remove,&QPushButton::clicked,this,[this]{
        if(d_->busy)return;const int index=d_->tabs->currentIndex();if(index<=0)return;
        const auto id=d_->tabs->widget(index)->property("repository_id").toLongLong();if(id==market::official_source().id)return;
        if(!show_confirm_dialog(this,market_text("Remove source"),market_text("Remove this repository from the plugin center? Installed plugins and their data will be kept.")))return;
        auto values=market::sources();values.erase(std::remove_if(values.begin(),values.end(),[id](const auto &s){return s.id==id;}),values.end());
        if(market::save_sources(values))refresh_sources();
    });
    refresh_sources();set_busy(false);
}
PluginMarketWidget::~PluginMarketWidget()=default;
void PluginMarketWidget::refresh_sources() {
    const auto values=market::sources();QSet<qint64> present;
    for(const auto &source:values) {
        present.insert(source.id);if(d_->pages.contains(source.id))continue;
        Impl::Page page;page.source=source;page.widget=new QWidget(d_->tabs);page.widget->setProperty("repository_id",source.id);
        auto *layout=new QVBoxLayout(page.widget);layout->setContentsMargins(scaled(4),scaled(8),scaled(4),0);
        auto *row=new QHBoxLayout;page.search=new LineEdit(page.widget);page.search->setPlaceholderText(market_text("Search plugins"));page.search->setObjectName("marketSearch_"+QString::number(source.id));
        page.refresh=new SecondaryPushButton(market_text("Refresh"),page.widget);page.refresh->setObjectName("refreshMarket_"+QString::number(source.id));row->addWidget(page.search,1);row->addWidget(page.refresh);layout->addLayout(row);
        page.status=new QLabel(source.full_name,page.widget);page.status->setTextFormat(Qt::PlainText);page.status->setWordWrap(true);layout->addWidget(page.status);
        page.list=new QListWidget(page.widget);page.list->setObjectName("pluginMarketList_"+QString::number(source.id));page.list->setIconSize(QSize(scaled(36),scaled(36)));page.list->setWordWrap(true);layout->addWidget(page.list,1);
        page.list->setProperty("pluginMarketList",true);
        page.install=new PrimaryPushButton(market_text("Install"),page.widget);page.install->setObjectName("installMarketPlugin_"+QString::number(source.id));page.install->setEnabled(false);
        auto *buttons=new QHBoxLayout;buttons->addStretch();buttons->addWidget(page.install);layout->addLayout(buttons);
        d_->pages.insert(source.id,page);d_->tabs->addTab(page.widget,source.id==market::official_source().id?market_text("Official plugins"):source.full_name);
        connect(page.search,&QLineEdit::textChanged,this,[this,id=source.id]{d_->render(d_->pages[id]);});
        connect(page.refresh,&QPushButton::clicked,this,[this,id=source.id]{d_->tabs->setCurrentWidget(d_->pages[id].widget);load_current(true);});
        connect(page.list,&QListWidget::currentItemChanged,this,[this,id=source.id]{d_->selection(d_->pages[id]);});
        connect(page.install,&QPushButton::clicked,this,[this,id=source.id]{if(const auto entry=d_->selected(d_->pages[id]))install(*entry);});
    }
    for(auto it=d_->pages.begin();it!=d_->pages.end();) {
        if(present.contains(it.key())){++it;continue;}
        d_->tabs->removeTab(d_->tabs->indexOf(it->widget));delete it->widget;it=d_->pages.erase(it);
    }
}
void PluginMarketWidget::set_busy(bool busy) {
    d_->busy=busy;d_->add->setEnabled(!busy);d_->repository->setEnabled(!busy);
    auto *current=d_->tabs->currentWidget();const auto id=current?current->property("repository_id").toLongLong():0;
    d_->remove->setEnabled(!busy&&id>0&&id!=market::official_source().id);
    for(auto &page:d_->pages){page.refresh->setEnabled(!busy);d_->selection(page);}
}
void PluginMarketWidget::load_current(bool refresh) {
    if(d_->busy || d_->tabs->currentIndex()<=0)return;auto *widget=d_->tabs->currentWidget();const auto id=widget->property("repository_id").toLongLong();
    if(!d_->pages.contains(id) || (d_->pages[id].loaded&&!refresh))return;
    auto &page=d_->pages[id];page.status->setText(market_text("Loading plugins…"));set_busy(true);const auto client=d_->client;const auto source=page.source;
    job(this,[client,source,refresh]{return client->catalog(source,refresh);},[id](PluginMarketWidget *self,std::optional<market::Catalog> catalog,const QString &error){
        self->set_busy(false);if(!self->d_->pages.contains(id))return;auto &page=self->d_->pages[id];
        if(!catalog){page.status->setText(market_text("Could not load plugins: %1").arg(market_text(error.toUtf8().constData())));return;}
        page.source=catalog->source;page.entries=market::select_entries(catalog->entries);page.loaded=true;
        page.status->setText(catalog->source.full_name+(catalog->cached?" · "+market_text("Cached"):QString()));self->d_->render(page);
        if(id!=market::official_source().id)self->d_->tabs->setTabText(self->d_->tabs->indexOf(page.widget),catalog->source.full_name);
        if(self->d_->tabs->currentWidget()!=page.widget)QTimer::singleShot(0,self,[self]{self->load_current();});
    });
}
std::optional<market::Entry> PluginMarketWidget::update_for(const QString &id) const {
    const auto found=d_->updates.constFind(id);if(found==d_->updates.cend())return std::nullopt;
    for(const auto &installed:installed_plugins(d_->root))if(installed.id==id) {
        const auto cmp=compare_plugin_versions(found->version,installed.version).value_or(-1);
        if(cmp>0 || (cmp==0&&!installed.compatible))return *found;
    }
    return std::nullopt;
}
void PluginMarketWidget::refresh_installed() {
    for(auto &page:d_->pages)d_->render(page);
}
void PluginMarketWidget::check_updates() {
    if(d_->busy)return;set_busy(true);d_->status->setText(market_text("Checking plugin updates…"));
    const auto client=d_->client;const auto repositories=market::sources();const auto installed=installed_plugins(d_->root);const auto root=d_->root;
    struct Checked {QMap<QString,market::Entry> updates;QSet<qint64> failed;};
    job(this,[client,repositories,installed,root]{
        QList<market::Catalog> catalogs;QSet<qint64> failed;
        for(const auto &source:repositories)try{catalogs.append(client->catalog(source));}catch(const std::exception &){failed.insert(source.id);}
        return Checked{market::choose_updates(installed,catalogs,failed,root),failed};
    },[](PluginMarketWidget *self,std::optional<Checked> checked,const QString &error){
        self->set_busy(false);if(!checked){self->d_->status->setText(market_text("Could not check updates: %1").arg(market_text(error.toUtf8().constData())));return;}
        self->d_->updates=checked->updates;self->d_->status->setText(checked->failed.isEmpty()?market_text("Updates available: %1").arg(checked->updates.size()):market_text("Some sources could not be checked. Source fallback was paused."));
        emit self->updates_changed();
    });
}
void PluginMarketWidget::install(const market::Entry &entry) {
    if(d_->busy||!entry.compatible)return;set_busy(true);d_->status->setText(market_text("Downloading %1…").arg(entry.name));const auto client=d_->client;
    struct Download {QString path;market::Source source;};
    job(this,[client,entry]{Download result;result.path=client->download(entry,&result.source);return result;},[entry](PluginMarketWidget *self,std::optional<Download> downloaded,const QString &error){
        self->set_busy(false);if(!downloaded){self->d_->status->setText(market_text("Could not download plugin: %1").arg(market_text(error.toUtf8().constData())));return;}
        PluginInstallOptions options;options.source={{"repository_id",downloaded->source.id},{"full_name",downloaded->source.full_name},{"asset_id",entry.asset_id},{"release_tag",entry.release_tag}};
        options.expected_id=entry.id;options.expected_version=entry.version;options.expected_sha256=entry.sha256;options.expected_platform=entry.platform;options.expected_host=entry.host;
        for(const auto &installed:installed_plugins(self->d_->root))if(installed.id==entry.id)options.reinstall_equal=compare_plugin_versions(installed.version,entry.version).value_or(-1)==0;
        const bool changed=install_plugin_packages_interactively(self,{downloaded->path},self->d_->root,options,false);
        self->d_->updates.remove(entry.id);for(auto &page:self->d_->pages)self->d_->render(page);
        self->d_->status->setText(changed?market_text("Installed: %1 %2").arg(entry.name,entry.version):market_text("Installation was cancelled or not completed."));
        if(changed)emit self->installed_changed();emit self->updates_changed();
    });
}
}
