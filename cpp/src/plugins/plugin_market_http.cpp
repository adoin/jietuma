#include "plugins/plugin_market.h"
#include "core/brand_config.h"
#include <QElapsedTimer>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#include <winhttp.h>
#endif

namespace jietuba::plugins::market {
namespace {
#ifdef Q_OS_WIN
struct InternetHandle {
    HINTERNET value = nullptr;
    explicit InternetHandle(HINTERNET value):value(value) { if(!value)throw Error("Could not connect to GitHub."); }
    ~InternetHandle(){WinHttpCloseHandle(value);}
    InternetHandle(const InternetHandle &)=delete;
};
QByteArray http(const QString &endpoint,bool binary,qint64 limit) {
    QUrl url("https://api.github.com/"+endpoint);
    QElapsedTimer total;total.start();
    for(int redirect=0;redirect<6;++redirect) {
        const QString host=url.host();
        const bool trusted=host=="api.github.com" || (binary && QStringList{"github.com","release-assets.githubusercontent.com","objects.githubusercontent.com","github-releases.githubusercontent.com"}.contains(host));
        if(url.scheme()!="https" || !trusted || !url.userInfo().isEmpty() || (url.port()!=-1&&url.port()!=443))throw Error("GitHub returned an unsafe redirect.");
        const auto agent=QStringLiteral(JTM_DISPLAY_NAME "/plugin-market").toStdWString();
        InternetHandle session(WinHttpOpen(agent.c_str(),WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
        WinHttpSetTimeouts(session.value,10000,10000,20000,20000);
        const auto hostname=host.toStdWString();
        InternetHandle connection(WinHttpConnect(session.value,hostname.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));
        auto resource=url.path(QUrl::FullyEncoded);if(!url.query().isEmpty())resource+='?'+url.query(QUrl::FullyEncoded);
        const auto path=resource.toStdWString();
        InternetHandle request(WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
        DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if(!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy)))throw Error("Could not configure GitHub HTTPS.");
        const std::wstring headers=std::wstring(binary?L"Accept: application/octet-stream\r\n":L"Accept: application/vnd.github+json\r\n")+L"X-GitHub-Api-Version: 2026-03-10\r\n";
        if(!WinHttpSendRequest(request.value,headers.c_str(),DWORD(headers.size()),WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request.value,nullptr))throw Error("GitHub request timed out or failed.");
        DWORD status=0,length=sizeof(status);
        if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX))throw Error("Invalid GitHub HTTP response.");
        if(status>=300&&status<400) {
            wchar_t destination[8192]{};length=sizeof(destination);
            if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,destination,&length,WINHTTP_NO_HEADER_INDEX)||length>=sizeof(destination))throw Error("Invalid GitHub redirect.");
            url=url.resolved(QUrl(QString::fromWCharArray(destination)));continue;
        }
        if(status!=200)throw Error("GitHub returned an error. Check repository access and rate limits.");
        QByteArray bytes;char buffer[65536];DWORD count=0;
        while(true) {
            if(total.elapsed()>90000 || QThread::currentThread()->isInterruptionRequested())throw Error("GitHub request was cancelled or timed out.");
            if(!WinHttpReadData(request.value,buffer,sizeof(buffer),&count))throw Error("Could not download the GitHub response.");
            if(!count)break;
            if(bytes.size()+qint64(count)>limit)throw Error("GitHub response is too large.");
            bytes.append(buffer,qsizetype(count));
        }
        return bytes;
    }
    throw Error("Too many GitHub redirects.");
}
#endif
}
QByteArray github_request(const QString &endpoint,bool binary,qint64 limit) {
    if((!endpoint.startsWith("repos/")&&!endpoint.startsWith("repositories/")) || endpoint.contains('\\') || endpoint.section('?',0,0).split('/').contains("..") || endpoint.contains('#') || limit<=0)
        throw Error("Invalid GitHub API request.");
    const auto gh=QStandardPaths::findExecutable("gh");
    if(gh.isEmpty()) {
#ifdef Q_OS_WIN
        return http(endpoint,binary,limit);
#else
        throw Error("GitHub CLI is required on this platform.");
#endif
    }
    QProcess process;
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args){args->flags|=CREATE_NO_WINDOW;});
#endif
    process.start(gh,{"api","--hostname","github.com",endpoint,"-H",binary?"Accept: application/octet-stream":"Accept: application/vnd.github+json","-H","X-GitHub-Api-Version: 2026-03-10"});
    if(!process.waitForStarted(5000))throw Error("Could not start GitHub CLI.");
    QElapsedTimer timer;timer.start();QByteArray bytes;
    while(process.state()!=QProcess::NotRunning) {
        process.waitForReadyRead(50);bytes+=process.readAllStandardOutput();process.readAllStandardError();
        if(bytes.size()>limit || timer.elapsed()>90000 || QThread::currentThread()->isInterruptionRequested()) {
            process.kill();process.waitForFinished(1000);throw Error("GitHub response exceeded its limit or timed out.");
        }
    }
    bytes+=process.readAllStandardOutput();
    if(process.exitStatus()!=QProcess::NormalExit || process.exitCode()!=0)throw Error("GitHub CLI request failed. Check gh auth login and repository access.");
    if(bytes.size()>limit)throw Error("GitHub response is too large.");
    return bytes;
}
}
