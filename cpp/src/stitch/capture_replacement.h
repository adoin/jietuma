#pragma once
#include <QImage>
#include <functional>
class QWidget;

namespace jietuba {
// True only after an explicit discard or a completed save. This never closes a capture.
bool confirm_capture_replacement(QWidget *parent, const QImage &image,
                                 const std::function<bool(const QImage &)> &save);
}
