#include "core/updater_process.h"
#include "core/app_version.h"
#include "core/brand_config.h"
#include "core/installer_exit.h"
#include "core/windows_clipboard_shortcut.h"
#include <QSettings>
#include <QTemporaryDir>
#include <app_version.h>
#include <QTest>
#include <windows.h>
using namespace jietuba;
class BuildVariantTests:public QObject {
    Q_OBJECT
private slots:
    void winVRequiresEnabledClipboardAndAnExactShortcut() {
        using namespace windows_clipboard_shortcut;
        QVERIFY(uses_win_v(true, {"ctrl+2", "WIN + V"}));
        QVERIFY(!uses_win_v(false, {"win+v"}));
        QVERIFY(!uses_win_v(true, {"ctrl+win+v", "v", "invalid"}));
    }
    void winVRegistryChangePreservesExistingShortcutsAndIsIdempotent() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings registry(directory.filePath("registry.ini"), QSettings::IniFormat);
        registry.setValue("DisabledHotkeys", "RE");
        registry.setValue("Other", 42);
        QString error;
        QVERIFY(windows_clipboard_shortcut::block(registry, &error));
        QCOMPARE(registry.value("DisabledHotkeys").toString(), QString("REV"));
        QCOMPARE(registry.value("Other").toInt(), 42);
        QVERIFY(windows_clipboard_shortcut::block(registry, &error));
        QCOMPARE(registry.value("DisabledHotkeys").toString(), QString("REV"));
        registry.setValue("DisabledHotkeys", 123);
        QVERIFY(!windows_clipboard_shortcut::block(registry, &error));
        QCOMPARE(registry.value("DisabledHotkeys").toInt(), 123);
        QCOMPARE(error, QString("type"));
        registry.remove("DisabledHotkeys");
        QVERIFY(windows_clipboard_shortcut::block(registry, &error));
        QCOMPARE(registry.value("DisabledHotkeys").toString(), QString("V"));
    }
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
