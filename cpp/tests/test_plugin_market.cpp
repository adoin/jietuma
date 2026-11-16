#include "plugins/plugin_market.h"
#include "plugins/plugin_market_widget.h"
#include "plugins/plugin_setup_widget.h"
#include "ui/dialogs.h"
#include "ui/welcome/wizard.h"
#include "ui/welcome/pages.h"
#include <QSemaphore>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include "plugins/plugin_package.h"
#include "settings/tool_settings.h"
#include "core/background_tasks.h"
#include "core/ui_theme.h"
#include "core/i18n.h"
#include "core/app_version.h"
#include <QDateTime>
#include "ui/settings_ui/dialog.h"
#include "ui/settings_ui/search.h"
#include <QPushButton>
#include <QStackedWidget>
#include <QtCore/private/qzipwriter_p.h>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <atomic>
#include <algorithm>

using namespace jietuba;
using namespace jietuba::plugins;
namespace market=jietuba::plugins::market;
namespace {
QString hash(const QByteArray &data){return QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());}
QByteArray json(const QJsonObject &value){return QJsonDocument(value).toJson(QJsonDocument::Compact);}
QJsonObject entry(QString version="1.0.0",QString host=">=2.3.0,<3.0.0") {
    const auto platform=plugin_platform();const auto tag="fixture/v"+version;const auto name="fixture-"+version+"-"+platform+".jtbplugin";
    return {{"id","fixture"},{"name","Fixture"},{"version",version},{"host",host},{"platform",platform},{"asset",name},{"asset_id",100},
        {"size",8},{"sha256",hash("package!")},{"release_tag",tag},
        {"download_url","https://github.com/owner/plugins/releases/download/"+QString::fromLatin1(QUrl::toPercentEncoding(tag))+"/"+name}};
}
QByteArray catalog(qint64 id,const QJsonArray &entries){return json({{"schema_version",2},{"repository_id",id},{"revision",QString(40,'a')},{"plugins",entries}});}
market::Request transport(qint64 id,const QByteArray &body,std::shared_ptr<std::atomic<int>> calls={},bool changed=false) {
    return [id,body,calls,changed](const QString &endpoint,bool binary,qint64 limit)->QByteArray {
        if(calls)++*calls;
        if(endpoint=="repositories/"+QString::number(id))return json({{"id",id},{"full_name","renamed/plugins"}});
        if(endpoint.contains("/releases?"))return QJsonDocument(QJsonArray{QJsonObject{{"draft",false},{"prerelease",false},{"published_at","2026-01-01T00:00:00Z"},
            {"assets",QJsonArray{QJsonObject{{"id",200},{"name","plugin-market.json"},{"size",body.size()},{"digest","sha256:"+hash(body)}}}}}}).toJson();
        if(endpoint.endsWith("/releases/assets/200")){if(!binary||limit<body.size())throw market::Error("bad request");return body;}
        if(endpoint.contains("/releases/tags/"))return json({{"draft",false},{"prerelease",false},{"tag_name","fixture/v1.0.0"},
            {"assets",QJsonArray{QJsonObject{{"id",100},{"name",entry().value("asset")},{"size",changed?9:8},{"digest","sha256:"+hash("package!")}}}}});
        if(endpoint.endsWith("/releases/assets/100"))return "package!";
        throw market::Error("Unexpected test request");
    };
}
QString archive(const QString &folder,const QString &version,const QString &host=">=2.3.0,<3.0.0",bool forged=false,const QString &identity="fixture") {
    const auto path=QDir(folder).filePath(identity+"-"+version+".jtbplugin");QZipWriter writer(path);
    writer.addFile("plugin.json",json({{"id",identity},{"name",identity},{"version",version},{"api_version",2},{"type","extension"},
        {"host",host},{"platform",plugin_platform()},{"contributes",QJsonObject{{"clipboard",QJsonObject{{"metadata",QJsonObject{{"content_types",QJsonArray{"text"}},{"fields",QJsonArray{QJsonObject{{"key","note"},{"label","Note"},{"type","line"}}}}}}}}}}}));
    if(forged)writer.addFile(".market-source.json","{}");writer.close();return path;
}
PluginInstallOptions options(const QString &path,const QString &version,qint64 repository) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly))qFatal("Cannot read test package");PluginInstallOptions result;
    result.expected_id="fixture";result.expected_version=version;result.expected_platform=plugin_platform();result.expected_host=">=2.3.0,<3.0.0";
    result.expected_sha256=hash(file.readAll());result.source={{"repository_id",repository},{"full_name","owner/plugins"}};return result;
}
}
class PluginMarketTests:public QObject {
    Q_OBJECT
    QTemporaryDir root_;
private slots:
    void initTestCase() {
        QVERIFY(root_.isValid());get_tool_settings_manager(new QSettings(root_.filePath("settings.ini"),QSettings::IniFormat,this));
    }
    void rangesUseSemverAndRejectMalformedRequirements() {
        QCOMPARE(plugin_version_satisfies("2.3.2",">=2.3.0,<3.0.0"),std::optional(true));
        QCOMPARE(plugin_version_satisfies("3.0.0",">=2.3.0,<3.0.0"),std::optional(false));
        QVERIFY(!plugin_version_satisfies("2.3.2",">=02.0.0"));
        QVERIFY(!plugin_version_satisfies("2.3.2",">=2.3.0 || <3.0.0"));
    }
    void catalogIdentityAndUrlsCannotRedirectToAnotherAuthority() {
        const auto bytes=catalog(7,{entry()});const market::Source source{7,"owner/plugins"};
        QCOMPARE(market::parse_catalog(bytes,source,"2.3.2",plugin_platform()).size(),1);
        QVERIFY_THROWS_EXCEPTION(market::Error,market::parse_catalog(bytes,{8,"owner/plugins"},"2.3.2",plugin_platform()));
        auto bad=entry();bad.insert("download_url","https://evil.example/plugin");
        QVERIFY_THROWS_EXCEPTION(market::Error,market::parse_catalog(catalog(7,{bad}),source,"2.3.2",plugin_platform()));
        QVERIFY_THROWS_EXCEPTION(market::Error,market::parse_catalog(catalog(7,{entry(),entry()}),source,"2.3.2",plugin_platform()));
    }
    void newestCompatibleStableVersionWins() {
        const auto values=market::parse_catalog(catalog(7,{entry(),entry("2.0.0",">=3.0.0"),entry("1.5.0-beta.1")}),{7,"owner/plugins"},"2.3.2",plugin_platform());
        const auto selected=market::select_entries(values);QCOMPARE(selected.size(),1);QCOMPARE(selected.front().version,QString("1.0.0"));
    }
    void cacheIsBoundToRepositoryAndRefreshBypassesIt() {
        QTemporaryDir folder;auto calls=std::make_shared<std::atomic<int>>(0);market::Client client(folder.path(),transport(7,catalog(7,{entry()}),calls));
        const auto first=client.catalog({7,"owner/plugins"});QVERIFY(!first.cached);QCOMPARE(first.source.full_name,QString("renamed/plugins"));
        const int count=*calls;QVERIFY(client.catalog({7,"owner/plugins"}).cached);QCOMPARE(int(*calls),count);
        QVERIFY(!client.catalog({7,"owner/plugins"},true).cached);QVERIFY(*calls>count);
    }
    void downloadsResolveRepositoryRenamesAndRevalidateAssets() {
        QTemporaryDir folder;market::Client client(folder.path(),transport(7,catalog(7,{entry()})));
        const auto selected=client.catalog({7,"owner/plugins"}).entries.front();market::Source actual;
        const auto path=client.download(selected,&actual);QCOMPARE(actual.full_name,QString("renamed/plugins"));
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("package!"));
        market::Client changed(folder.path(),transport(7,{},nullptr,true));
        QVERIFY_THROWS_EXCEPTION(market::Error,changed.download(selected));
    }
    void installationSourceChangesAreAtomicAndPreserveUserData() {
        QTemporaryDir folder;const auto plugins=QDir(folder.path()).filePath("plugins");
        const auto first=archive(folder.path(),"1.0.0");auto context=options(first,"1.0.0",77);
        QVERIFY(install_plugin_package(first,plugins,context).ok);QCOMPARE(plugin_install_source("fixture",plugins).value("repository_id").toInteger(),qint64(77));
        context.reinstall_equal=true;context.expected_previous_repository=77;
        QVERIFY(!install_plugin_package(first,plugins,context).skipped);
        auto wrong=context;wrong.expected_sha256=QString(64,'0');QVERIFY(!install_plugin_package(first,plugins,wrong).ok);
        QVERIFY(save_plugin_config("fixture",{{"keep","yes"}},plugins));QVERIFY(set_plugin_enabled("fixture",false,plugins));
        const auto second=archive(folder.path(),"1.1.0");context=options(second,"1.1.0",78);context.expected_previous_repository=77;
        QVERIFY(!install_plugin_package(second,plugins,context).ok);QCOMPARE(installed_plugins(plugins).front().version,QString("1.0.0"));
        context.allow_source_change=true;context.expected_previous_repository=76;QVERIFY(!install_plugin_package(second,plugins,context).ok);
        context.expected_previous_repository=77;QVERIFY(install_plugin_package(second,plugins,context).ok);
        QCOMPARE(plugin_install_source("fixture",plugins).value("repository_id").toInteger(),qint64(78));
        QCOMPARE(read_plugin_config("fixture",plugins).value("keep").toString(),QString("yes"));QVERIFY(!installed_plugins(plugins).front().enabled);
        QVERIFY(install_plugin_package(second,plugins).skipped);QVERIFY(!plugin_install_source("fixture",plugins).isEmpty());
        QVERIFY(install_plugin_package(archive(folder.path(),"1.2.0"),plugins).ok);QVERIFY(plugin_install_source("fixture",plugins).isEmpty());
        QVERIFY(!inspect_plugin_package(archive(folder.path(),"2.0.0",">=9.0.0")).ok);
        QVERIFY(!inspect_plugin_package(archive(folder.path(),"3.0.0",">=2.3.0,<3.0.0",true)).ok);
    }
    void customSourcesRemainPinnedAndFailureDoesNotFallBack() {
        QTemporaryDir folder;const auto path=archive(folder.path(),"1.0.0");auto context=options(path,"1.0.0",77);
        QVERIFY(install_plugin_package(path,folder.filePath("plugins"),context).ok);
        auto official=market::parse_catalog(catalog(market::official_source().id,{entry("3.0.0",">=2.0.0")}),market::official_source(),"2.3.2",plugin_platform());
        auto custom=market::parse_catalog(catalog(77,{entry("1.1.0")}),{77,"custom/plugins"},"2.3.2",plugin_platform());
        const auto installed=installed_plugins(folder.filePath("plugins"));
        const auto updates=market::choose_updates(installed,{{market::official_source(),official},{market::Source{77,"custom/plugins"},custom}},{},folder.filePath("plugins"));
        QCOMPARE(updates.value("fixture").version,QString("1.1.0"));
        QVERIFY(market::choose_updates(installed,{{market::official_source(),official}},{77},folder.filePath("plugins")).isEmpty());
    }
    void officialPackagesPassHostInspection() {
        const auto folder=qEnvironmentVariable("JTM_MARKET_TEST_PACKAGES");
        if(folder.isEmpty())QSKIP("No external packages supplied");
        const auto files=QDir(folder).entryList({"*.jtbplugin"},QDir::Files);QVERIFY(!files.isEmpty());
        for(const auto &file:files) {
            const auto result=inspect_plugin_package(QDir(folder).filePath(file));
            QVERIFY2(result.ok,qPrintable(file+": "+result.error));QCOMPARE(result.platform,plugin_platform());
        }
    }
    void widgetLoadsOnlyTheSelectedSourceAndFiltersLocally() {
        QTemporaryDir folder;const auto path=archive(folder.path(),"1.0.0");
        QVERIFY(install_plugin_package(path,folder.path()).ok);QVERIFY(set_plugin_enabled("fixture",false,folder.path()));
        const auto source=market::official_source();auto calls=std::make_shared<std::atomic<int>>(0);
        auto client=std::make_shared<market::Client>(folder.path(),transport(source.id,catalog(source.id,{entry()}),calls));
        I18nManager::load_language("zh");get_ui_theme()->init(get_tool_settings_manager(),qApp);
        auto *installed=new QWidget;PluginMarketWidget widget(installed,folder.path(),client);widget.resize(740,500);widget.show();
        auto *tabs=widget.findChild<QTabWidget *>("pluginCenterTabs");QVERIFY(tabs);QCOMPARE(int(*calls),0);
        tabs->setCurrentIndex(1);auto *list=widget.findChild<QListWidget *>("pluginMarketList_"+QString::number(source.id));QVERIFY(list);
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),1,10000);const int fetched=*calls;
        QVERIFY(list->item(0)->text().contains(i18n_tr("Installed: %1","PluginMarket").arg("1.0.0")));
        QVERIFY(list->item(0)->text().contains(i18n_tr("Disabled","PluginMarket")));
        QVERIFY(uninstall_plugin("fixture",folder.path()));widget.refresh_installed();
        QVERIFY(!list->item(0)->text().contains(i18n_tr("Installed: %1","PluginMarket").arg("1.0.0")));
        auto *search=widget.findChild<QLineEdit *>("marketSearch_"+QString::number(source.id));QVERIFY(search);search->setText("no-match");QCOMPARE(list->count(),0);
        search->setText("Fixture");QCOMPARE(list->count(),1);QCOMPARE(int(*calls),fetched);
        const QString screenshots=qEnvironmentVariable("JTM_MARKET_SCREENSHOTS");
        if(!screenshots.isEmpty())for(const auto &mode:{QString("light"),QString("dark")}) {
            get_ui_theme()->set_mode(mode,false);QCoreApplication::processEvents();
            QVERIFY(widget.grab().save(QDir(screenshots).filePath("market-"+mode+".png")));
        }
        QTRY_VERIFY_WITH_TIMEOUT(!background_tasks::busy(),10000);
    }
    void settingsOwnsPluginPageAndRestoresItsRoute() {
        QTemporaryDir fixture;
        QVERIFY(install_plugin_package(archive(fixture.path(),"1.0.0"),plugin_directory()).ok);
        const auto source=market::official_source();
        const auto cache=QDir(plugin_directory()).filePath(".market-cache");QVERIFY(QDir().mkpath(cache));
        QFile file(QDir(cache).filePath(QString::number(source.id)+".json"));QVERIFY(file.open(QIODevice::WriteOnly));
        const auto body=json({{"fetched_at",QDateTime::currentMSecsSinceEpoch()},{"host_version",APP_VERSION},
            {"catalog",QJsonDocument::fromJson(catalog(source.id,{entry()})).object()}});
        QCOMPARE(file.write(body),body.size());file.close();
        SettingsDialog dialog(get_tool_settings_manager());
        QVERIFY(!dialog.has_attr("plugin_manager_page"));
        dialog.on_nav_changed(SettingsDialog::PLUGINS_PAGE,"plugins");
        auto *page=dialog.attr_widget("plugin_manager_page");QVERIFY(page);QVERIFY(!page->isWindow());
        QCOMPARE(dialog.content_stack->currentWidget(),page);
        QCOMPARE(dialog.view_state().route_key,QString("plugins"));
        QVERIFY(dialog.findChild<QListWidget *>("pluginList"));
        QVERIFY(dialog.findChild<QTabWidget *>("pluginCenterTabs"));
        dialog.on_nav_changed(2,"clipboard");dialog.restore_view_state({SettingsDialog::PLUGINS_PAGE,"plugins"});
        dialog.show();QCoreApplication::processEvents();
        QCOMPARE(dialog.content_stack->currentWidget(),page);
        for(auto *button:dialog.findChildren<QPushButton *>())if(button->text()==dialog.tr_text("Reset This Page"))QVERIFY(button->isHidden());
        const auto entries=dialog.search_entries();
        QVERIFY(std::any_of(entries.begin(),entries.end(),[](const auto &entry){return entry.route_key=="plugins";}));
        auto *tabs=dialog.findChild<QTabWidget *>("pluginCenterTabs");tabs->setCurrentIndex(1);
        auto *list=dialog.findChild<QListWidget *>("pluginMarketList_"+QString::number(source.id));QVERIFY(list);
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),1,10000);
        QVERIFY(list->item(0)->text().contains(i18n_tr("Installed: %1","PluginMarket").arg("1.0.0")));
        const auto folder=qEnvironmentVariable("JTM_MARKET_SCREENSHOTS");
        if(!folder.isEmpty())for(const auto &mode:{QString("light"),QString("dark")}) {
            get_ui_theme()->set_mode(mode,false);QCoreApplication::processEvents();
            QVERIFY(dialog.grab().save(QDir(folder).filePath("settings-plugins-"+mode+".png")));
        }
        QVERIFY(uninstall_plugin("fixture",plugin_directory()));
    }

    void setupLoadsOnlyOnRequestAndCancellationNeverInstalls() {
        QTemporaryDir folder;const auto source=market::official_source();
        const auto calls=std::make_shared<std::atomic<int>>(0);const auto started=std::make_shared<std::atomic_bool>(false);
        const auto gate=std::make_shared<QSemaphore>();
        auto base=transport(source.id,catalog(source.id,{entry()}),calls);
        market::Request request=[base,started,gate](const QString &endpoint,bool binary,qint64 limit){
            if(endpoint.endsWith("/releases/assets/100")){started->store(true);if(!gate->tryAcquire(1,5000))throw market::Error("Timed out waiting for test cancellation");}
            return base(endpoint,binary,limit);
        };
        auto release=qScopeGuard([gate]{gate->release();});
        plugins::PluginSetupWidget widget(nullptr,folder.path(),std::make_shared<market::Client>(folder.path(),request));widget.show();
        QCoreApplication::processEvents();QCOMPARE(int(*calls),0);QCOMPARE(widget.selected_count(),0);
        widget.findChild<QPushButton *>("browseSetupPlugins")->click();auto *list=widget.findChild<QListWidget *>("pluginSetupList");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),1,10000);QCOMPARE(widget.selected_count(),0);
        list->item(0)->setCheckState(Qt::Checked);QCOMPARE(widget.selected_count(),1);
        QSignalSpy done(&widget,&plugins::PluginSetupWidget::completed);widget.install_selected();
        QTRY_VERIFY_WITH_TIMEOUT(started->load(),10000);widget.cancel();gate->release();
        QTRY_VERIFY_WITH_TIMEOUT(!background_tasks::busy(),10000);
        QVERIFY(installed_plugins(folder.path()).isEmpty());QCOMPARE(done.count(),0);QVERIFY(!widget.busy());QVERIFY(!QApplication::activeModalWidget());
    }
    void setupReportsOfflineFailureWithoutBlockingCompletion() {
        QTemporaryDir folder;auto client=std::make_shared<market::Client>(folder.path(),[](const QString &,bool,qint64)->QByteArray{throw market::Error("offline");});
        plugins::PluginSetupWidget widget(nullptr,folder.path(),client);widget.show();
        widget.findChild<QPushButton *>("browseSetupPlugins")->click();QTRY_VERIFY_WITH_TIMEOUT(!widget.busy(),10000);
        QVERIFY(widget.findChild<QLabel *>("pluginSetupStatus")->text().contains("offline"));QCOMPARE(widget.selected_count(),0);
        QVERIFY(installed_plugins(folder.path()).isEmpty());
    }
    void setupInstallsTheSelectedBatchWithOneConfirmationAndBoundOrigins() {
        QTemporaryDir folder;const auto root=folder.filePath("plugins");
        QVERIFY(install_plugin_package(archive(folder.path(),"1.0.0"),root).ok);
        QVERIFY(set_plugin_enabled("fixture",false,root));QVERIFY(save_plugin_config("fixture",{{"keep","yes"}},root));
        QMap<int,QPair<QJsonObject,QByteArray>> packages;QJsonArray rows;
        for(int number=0;number<2;++number){
            const QString id=number?"second":"fixture";const QString version=number?"1.0.0":"1.1.0";
            const auto path=archive(folder.path(),version,">=2.3.0,<3.0.0",false,id);QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();
            auto e=entry(version);const auto tag=id+"/v"+version;const auto name=id+"-"+version+"-"+plugin_platform()+".jtbplugin";
            e.insert("id",id);e.insert("name",id);e.insert("release_tag",tag);e.insert("asset",name);e.insert("asset_id",100+number);e.insert("size",bytes.size());e.insert("sha256",hash(bytes));
            e.insert("download_url","https://github.com/owner/plugins/releases/download/"+QString::fromLatin1(QUrl::toPercentEncoding(tag))+"/"+name);
            packages.insert(100+number,{e,bytes});rows.append(e);
        }
        const auto source=market::official_source();const auto body=catalog(source.id,rows);const auto base=transport(source.id,body);
        market::Request request=[base,packages](const QString &endpoint,bool binary,qint64 limit)->QByteArray{
            for(auto it=packages.cbegin();it!=packages.cend();++it){const auto &e=it->first;
                if(endpoint.endsWith("/releases/assets/"+QString::number(it.key())))return it->second;
                if(endpoint.endsWith("/releases/tags/"+QString::fromLatin1(QUrl::toPercentEncoding(e.value("release_tag").toString()))))
                    return json({{"draft",false},{"prerelease",false},{"tag_name",e.value("release_tag")},{"assets",QJsonArray{QJsonObject{{"id",it.key()},{"name",e.value("asset")},{"size",e.value("size")},{"digest","sha256:"+e.value("sha256").toString()}}}}});
            }
            return base(endpoint,binary,limit);
        };
        plugins::PluginSetupWidget widget(nullptr,root,std::make_shared<market::Client>(root,request));widget.resize(304,430);widget.show();
        widget.findChild<QPushButton *>("browseSetupPlugins")->click();auto *list=widget.findChild<QListWidget *>("pluginSetupList");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),2,10000);QCOMPARE(widget.selected_count(),0);
        for(int row=0;row<list->count();++row)list->item(row)->setCheckState(Qt::Checked);
        const auto screenshots=qEnvironmentVariable("JTM_MARKET_SCREENSHOTS");
        if(!screenshots.isEmpty())for(const auto &mode:{QString("light"),QString("dark")}){get_ui_theme()->set_mode(mode,false);QCoreApplication::processEvents();QVERIFY(widget.grab().save(QDir(screenshots).filePath("plugin-setup-"+mode+".png")));}
        int confirmations=0;QTimer answer;answer.setInterval(10);
        connect(&answer,&QTimer::timeout,&widget,[&]{if(auto *dialog=qobject_cast<StandardDialog *>(QApplication::activeModalWidget())){
            if(auto *items=dialog->findChild<QListWidget *>("pluginInstallList")){++confirmations;QCOMPARE(items->count(),2);}
            for(auto *button:dialog->button_box->buttons())if(dialog->button_box->buttonRole(button)==QDialogButtonBox::AcceptRole){button->click();break;}
        }});answer.start();
        QSignalSpy done(&widget,&plugins::PluginSetupWidget::completed);widget.install_selected();QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,15000);answer.stop();
        QCOMPARE(confirmations,1);QCOMPARE(installed_plugins(root).size(),2);QCOMPARE(widget.selected_count(),0);
        for(int row=0;row<list->count();++row)QVERIFY(!(list->item(row)->flags()&Qt::ItemIsUserCheckable));
        for(const auto &p:installed_plugins(root))QCOMPARE(plugin_install_source(p.id,root).value("repository_id").toInteger(),source.id);
        QCOMPARE(read_plugin_config("fixture",root).value("keep").toString(),QString("yes"));
        for(const auto &p:installed_plugins(root))if(p.id=="fixture")QVERIFY(!p.enabled);
    }
    void wizardOffersSkipOnItsLastPageWithoutStartingNetworkRequests() {
        WelcomeWizard wizard(get_tool_settings_manager());wizard.show();wizard.go_to_page(wizard.page_count()-1);
        auto *setup=wizard.findChild<plugins::PluginSetupWidget *>();QVERIFY(setup);QCOMPARE(setup->selected_count(),0);QVERIFY(!setup->busy());
        auto *skip=wizard.findChild<QPushButton *>("btnSkip");QVERIFY(skip&&skip->isVisible());QCOMPARE(skip->text(),welcome_tr("跳过插件安装"));
        auto *next=wizard.findChild<QPushButton *>("btnNext");QVERIFY(next&&next->isEnabled());QCOMPARE(next->text(),welcome_tr("完成"));
        const auto folder=qEnvironmentVariable("JTM_MARKET_SCREENSHOTS");
        if(!folder.isEmpty())for(const auto &mode:{QString("light"),QString("dark")}){get_ui_theme()->set_mode(mode,false);QCoreApplication::processEvents();QVERIFY(wizard.grab().save(QDir(folder).filePath("wizard-plugin-setup-"+mode+".png")));}
        wizard.hide();
    }

    void liveAuthenticatedOfficialRepositoryResolve() {
        if(qEnvironmentVariable("JIETUMA_LIVE_GH_CHECK")!="1")QSKIP("Live authenticated gh check is opt-in");
        QTemporaryDir folder;market::Client client(folder.path());
        const auto source=client.resolve(market::official_source().id);
        QCOMPARE(source.id,market::official_source().id);QVERIFY(market::valid_repository_name(source.full_name));
    }

};
QTEST_MAIN(PluginMarketTests)
#include "test_plugin_market.moc"
