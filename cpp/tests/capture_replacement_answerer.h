#pragma once
#include "ui/dialogs.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>

// Desktop integration tests explicitly choose to replace the previous result.
class CaptureReplacementDiscardAnswerer {
    QTimer timer_;
public:
    bool answered = false;
    CaptureReplacementDiscardAnswerer() {
        timer_.setInterval(20);
        QObject::connect(&timer_, &QTimer::timeout, [this] {
            auto *dialog = qobject_cast<jietuba::StandardDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            for (auto *button : dialog->button_box->buttons())
                if (dialog->button_box->buttonRole(button) == QDialogButtonBox::DestructiveRole) {
                    timer_.stop();answered=true;button->click();return;
                }
        });
        timer_.start();
    }
};
