#include "core/updater_process.h"
#include "core/app_version.h"
#include <app_version.h>
#include <QTest>
using namespace jietuba;
class BuildVariantTests:public QObject {
    Q_OBJECT
private slots:
    void editionAndVersionMatchTheConfiguredBuild() {
        QCOMPARE(app_variant(),QStringLiteral(EXPECTED_BUILD_VARIANT));
        QCOMPARE(APP_VERSION,QStringLiteral(JIETUBA_APP_VERSION));
    }
};
QTEST_GUILESS_MAIN(BuildVariantTests)
#include "test_build_variant.moc"
