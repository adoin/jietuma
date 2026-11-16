#include "stitch/capture_replacement.h"
#include "tools/action.h"
#include "core/save.h"
#include "settings/tool_settings.h"
#include "ui/dialogs.h"
#include "save_dialog_answerer.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QImage>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace jietuba;
namespace {
class Answer {
    QTimer timer_;
public:
    bool answered = false;
    explicit Answer(QDialogButtonBox::ButtonRole role, bool close = false, bool enter = false) {
        timer_.setInterval(10);
        QObject::connect(&timer_, &QTimer::timeout, [this, role, close, enter] {
            auto *dialog = qobject_cast<StandardDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            timer_.stop(); answered = true;
            if (close) { dialog->close(); return; }
            if (enter) { QTest::keyClick(dialog,Qt::Key_Return); return; }
            for (auto *button : dialog->button_box->buttons())
                if (dialog->button_box->buttonRole(button) == role) { button->click(); return; }
        });
        timer_.start();
    }
};
}
class CaptureReplacementTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    ToolSettingsManager *config_ = nullptr;
    QImage image_{24, 16, QImage::Format_RGB32};
private slots:
    void initTestCase() {
        QVERIFY(directory_.isValid());
        config_=get_tool_settings_manager(new QSettings(directory_.filePath("settings.ini"),QSettings::IniFormat,this));
        config_->set_screenshot_save_path(directory_.path());
        config_->set_save_dialog_style("modern");
        image_.fill(Qt::red);
    }
    void cancelAndWindowCloseKeepTheResult_data() {
        QTest::addColumn<bool>("close");
        QTest::addColumn<bool>("enter");
        QTest::newRow("cancel") << false << false;
        QTest::newRow("close") << true << false;
        QTest::newRow("enter-defaults-to-cancel") << false << true;
    }
    void cancelAndWindowCloseKeepTheResult() {
        QFETCH(bool,close);
        QFETCH(bool,enter);
        Answer answer(QDialogButtonBox::RejectRole,close,enter);
        bool saved=false;
        QVERIFY(!confirm_capture_replacement(nullptr,image_,[&](const QImage &){saved=true;return true;}));
        QVERIFY(answer.answered); QVERIFY(!saved);
        QCOMPARE(image_.pixelColor(0,0),QColor(Qt::red));
    }
    void discardRequiresAnExplicitChoice() {
        Answer answer(QDialogButtonBox::DestructiveRole);
        bool saved=false;
        QVERIFY(confirm_capture_replacement(nullptr,image_,[&](const QImage &){saved=true;return true;}));
        QVERIFY(answer.answered); QVERIFY(!saved);
    }
    void failedSaveDoesNotPermitReplacement() {
        Answer answer(QDialogButtonBox::AcceptRole);
        bool attempted=false, identical=false;
        QVERIFY(!confirm_capture_replacement(nullptr,image_,[&](const QImage &snapshot){attempted=true;identical=snapshot==image_;return false;}));
        QVERIFY(answer.answered); QVERIFY(attempted); QVERIFY(identical);
    }
    void cancellingTheActualSaveDialogKeepsTheResult() {
        Answer answer(QDialogButtonBox::AcceptRole);
        SaveDialogAnswerer save(SaveDialogAnswerer::CANCEL);
        QVERIFY(!confirm_capture_replacement(nullptr,image_,[&](const QImage &snapshot){
            return ActionTools::save_image_interactively(nullptr,snapshot,"previous",config_,std::make_shared<SaveService>(*config_));
        }));
        QVERIFY(answer.answered); QVERIFY(save.answered);
    }
    void successfulSavePreservesTheActualPixels() {
        Answer answer(QDialogButtonBox::AcceptRole);
        const QString path=directory_.filePath("previous.png");
        SaveDialogAnswerer save(path);
        QVERIFY(confirm_capture_replacement(nullptr,image_,[&](const QImage &snapshot){
            return ActionTools::save_image_interactively(nullptr,snapshot,"previous",config_,std::make_shared<SaveService>(*config_));
        }));
        QVERIFY(answer.answered); QVERIFY(save.answered);
        const QImage saved(path); QVERIFY(!saved.isNull());
        QCOMPARE(saved.size(),image_.size()); QCOMPARE(saved.pixelColor(5,5),QColor(Qt::red));
    }
    void anEmptyCaptureNeedsNoConfirmation() {
        QVERIFY(confirm_capture_replacement(nullptr,{},[](const QImage &){return false;}));
    }
};
QTEST_MAIN(CaptureReplacementTests)
#include "test_capture_replacement.moc"
