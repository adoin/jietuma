#include "core/updater_process.h"
#include "core/app_version.h"
#include "core/brand_config.h"
#include "core/installer_exit.h"
#include <app_version.h>
#include <QTest>
#include <windows.h>
using namespace jietuba;
class BuildVariantTests:public QObject {
    Q_OBJECT
private slots:
    void editionAndVersionMatchTheConfiguredBuild() {
        QCOMPARE(app_variant(),QStringLiteral(EXPECTED_BUILD_VARIANT));
        QCOMPARE(APP_VERSION,QStringLiteral(JIETUBA_APP_VERSION));
    }
    void installerExitUsesTheApplicationEventLoop() {
        QObject owner;
        int requests = 0;
        auto *listener = listen_for_installer_exit(&owner, [&] { ++requests; });
        QVERIFY(listener);
        const auto name = QStringLiteral("Local\\" JTM_NAME ".InstallerExit.%1")
                              .arg(QCoreApplication::applicationPid()).toStdWString();
        HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
        QVERIFY(event);
        const bool sent = SetEvent(event);
        CloseHandle(event);
        QVERIFY(sent);
        QTRY_COMPARE(requests, 1);
        QTest::qWait(30);
        QCOMPARE(requests, 1);
        delete listener;
        event = OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
        if (event) CloseHandle(event);
        QVERIFY(!event);
    }
};
QTEST_GUILESS_MAIN(BuildVariantTests)
#include "test_build_variant.moc"
