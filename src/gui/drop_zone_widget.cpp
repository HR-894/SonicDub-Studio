#include "gui/drop_zone_widget.h"
#include <QPainter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QFileDialog>

namespace vd {

DropZoneWidget::DropZoneWidget(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setMinimumHeight(160);
    setCursor(Qt::PointingHandCursor);
}

void DropZoneWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg  = hover_ ? QColor(0x45,0x47,0x5a) : QColor(0x31,0x32,0x44);
    QColor border = hover_ ? QColor(0x89,0xb4,0xfa) : QColor(0x58,0x5b,0x70);

    p.setBrush(bg);
    p.setPen(QPen(border, 2, Qt::DashLine));
    p.drawRoundedRect(rect().adjusted(4,4,-4,-4), 12, 12);

    p.setPen(QColor(0xcd,0xd6,0xf4));
    p.setFont(QFont("Segoe UI", 28));
    p.drawText(rect().adjusted(0,-20,0,0), Qt::AlignCenter, "🎬");

    p.setFont(QFont("Segoe UI", 11));
    p.setPen(QColor(0xa6,0xad,0xc8));
    p.drawText(rect().adjusted(0,30,0,0), Qt::AlignCenter,
               "Drop video here or click to browse\n(MP4, MKV, AVI, MOV, WEBM)");
}

void DropZoneWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) { hover_ = true; update(); e->acceptProposedAction(); }
}

void DropZoneWidget::dropEvent(QDropEvent* e) {
    hover_ = false; update();
    for (auto& url : e->mimeData()->urls()) {
        if (url.isLocalFile()) Q_EMIT file_dropped(url.toLocalFile());
    }
}

void DropZoneWidget::mousePressEvent(QMouseEvent*) {
    auto files = QFileDialog::getOpenFileNames(this, "Open Video", {},
                    "Videos (*.mp4 *.mkv *.avi *.mov *.webm);;All (*)");
    for (auto& f : files) Q_EMIT file_dropped(f);
}

} // namespace vd
