/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  drop_zone_widget.h — Drag-and-Drop Video File Widget                     ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * VISUAL APPEARANCE:
 *   ┌─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─┐
 *   │                                        │
 *   │              🎬                         │
 *   │   Drop video here or click to browse   │
 *   │    (MP4, MKV, AVI, MOV, WEBM)          │
 *   │                                        │
 *   └─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─┘
 *
 * FEATURES:
 *   - Custom QPainter rendering (dashed border, rounded corners)
 *   - Hover highlight when dragging a file over
 *   - Click-to-browse via QFileDialog
 *   - Emits file_dropped(path) signal for each accepted file
 */

#pragma once

#include <QWidget>

namespace vd {

class DropZoneWidget : public QWidget {
    Q_OBJECT

public:
    explicit DropZoneWidget(QWidget* parent = nullptr);

Q_SIGNALS:
    /// Emitted when a video file is dropped or selected via browse.
    void file_dropped(const QString& path);

protected:
    void paintEvent(QPaintEvent*)     override;   ///< Custom drawing
    void dragEnterEvent(QDragEnterEvent*) override; ///< Accept video drops
    void dropEvent(QDropEvent*)       override;   ///< Process drop
    void mousePressEvent(QMouseEvent*) override;  ///< Open file dialog

private:
    bool hover_{false};   ///< True when cursor is dragging over the zone
};

} // namespace vd
