#include "plugins/plugin_file_open.h"
#include "core/brand_config.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>
#include <utility>
#include <windows.h>
#include <shellapi.h>
namespace jietuba::plugins {
namespace {
constexpr ULONG_PTR MESSAGE_TAG=0x4A544250;
const wchar_t *WINDOW_CLASS=L"DesktopPluginInstallReceiver";
std::wstring title(const QString &channel){return (QStringLiteral(JTM_NAME ".PluginInstall.")+channel).toStdWString();}
struct Receiver : QObject {
    HWND window=nullptr;QTimer timer;QStringList pending;bool busy=false,exit_when_idle=false;
    std::function<void(QStringList)> install;
    Receiver(QObject *parent):QObject(parent) {
        timer.setSingleShot(true);timer.setInterval(250);
        QObject::connect(&timer,&QTimer::timeout,this,[this] {
            if(busy){timer.start();return;}
            if(pending.isEmpty())return;
            busy=true;auto paths=std::exchange(pending,{});install(paths);busy=false;
            if(!pending.isEmpty())timer.start();
            else if(exit_when_idle)QTimer::singleShot(300,this,[this]{if(!busy&&pending.isEmpty())QCoreApplication::quit();});
        });
    }
    ~Receiver() override {if(window)DestroyWindow(window);}
    void enqueue(const QStringList &paths){for(const auto &path:paths)if(pending.size()<128&&!pending.contains(path))pending<<path;timer.start();}
};
LRESULT CALLBACK receive(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_NCCREATE){auto *create=reinterpret_cast<CREATESTRUCTW *>(l);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));}
    if(message==WM_COPYDATA) {
        auto *receiver=reinterpret_cast<Receiver *>(GetWindowLongPtrW(window,GWLP_USERDATA));auto *data=reinterpret_cast<COPYDATASTRUCT *>(l);
        if(!receiver||!data||data->dwData!=MESSAGE_TAG||!data->lpData||data->cbData>1024*1024)return 0;
        const auto document=QJsonDocument::fromJson(QByteArray(static_cast<const char *>(data->lpData),int(data->cbData)));
        if(!document.isArray()||document.array().size()>128)return 0;
        QStringList paths;for(const auto &entry:document.array())if(entry.isString()&&QFileInfo(entry.toString()).isAbsolute()&&QFileInfo(entry.toString()).suffix().compare("jtbplugin",Qt::CaseInsensitive)==0)paths<<entry.toString();
        if(paths.isEmpty())return 0;
        QPointer<Receiver> safe(receiver);QMetaObject::invokeMethod(receiver,[safe,paths]{if(safe)safe->enqueue(paths);},Qt::QueuedConnection);
        return 1;
    }
    return DefWindowProcW(window,message,w,l);
}
}
QStringList plugin_files_from_command_line() {
    int count=0;auto **args=CommandLineToArgvW(GetCommandLineW(),&count);QStringList paths;
    if(!args)return paths;
    for(int i=1;i<count;++i){const auto value=QString::fromWCharArray(args[i]);if(!value.startsWith('-')&&QFileInfo(value).suffix().compare("jtbplugin",Qt::CaseInsensitive)==0)paths<<QFileInfo(value).absoluteFilePath();}
    LocalFree(args);return paths.mid(0,128);
}
bool forward_plugin_files(const QStringList &paths,const QString &channel) {
    if(paths.isEmpty())return false;
    auto name=title(channel);const auto window=FindWindowExW(HWND_MESSAGE,nullptr,WINDOW_CLASS,name.c_str());if(!window)return false;
    DWORD owner_pid=0;GetWindowThreadProcessId(window,&owner_pid);if(owner_pid)AllowSetForegroundWindow(owner_pid);
    QJsonArray values;for(const auto &path:paths.mid(0,128))values.append(QFileInfo(path).absoluteFilePath());
    const auto json=QJsonDocument(values).toJson(QJsonDocument::Compact);if(json.size()>1024*1024)return false;
    COPYDATASTRUCT data{MESSAGE_TAG,DWORD(json.size()),const_cast<char *>(json.constData())};DWORD_PTR result=0;
    return SendMessageTimeoutW(window,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&result)&&result==1;
}
bool receive_plugin_files(QObject *owner,const QStringList &initial,std::function<void(QStringList)> install,bool exit_when_idle,const QString &channel) {
    auto name=title(channel);if(FindWindowExW(HWND_MESSAGE,nullptr,WINDOW_CLASS,name.c_str()))return false;
    WNDCLASSW cls{};cls.lpfnWndProc=receive;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=WINDOW_CLASS;
    if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    auto *receiver=new Receiver(owner);receiver->install=std::move(install);receiver->exit_when_idle=exit_when_idle;
    receiver->window=CreateWindowExW(0,WINDOW_CLASS,name.c_str(),0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,receiver);
    if(!receiver->window){delete receiver;return false;}
    if(!initial.isEmpty())receiver->enqueue(initial);return true;
}
}
