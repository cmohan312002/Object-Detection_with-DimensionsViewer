#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QWidget>
#include <QPainterPath>
#include <vector>
#include <array>

struct ShapeObject {
    int id;
    QString shape;
    double widthMM, heightMM, diagonalMM, perimMM, areaMM2, aspectRatio, angleDeg;
    double centerXmm, centerYmm;
    QPainterPath pathCanvas;
};

class CanvasWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = nullptr);
    void setObjects(const std::vector<ShapeObject> &objs, double pxMM, bool cal);
    void setSelectedIndex(int idx);
    int getSelectedIndex() const;
    void fitToView();
    void zoomToFit(const QRectF &rect);
    void zoomIn();
    void zoomOut();
    void setShowDimensions(bool on);
    void setShowGrid(bool on);
    static const std::array<QColor,6> COLORS;

signals:
    void objectClicked(int index);
    void objectMoved(int index);
    void objectMoveFinished(int idx, QPointF oldCenter, QPointF newCenter,
                            QPainterPath oldPath, QPainterPath newPath);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void clampZoom();
    QTransform viewTransform() const;
    int hitTest(const QPointF &canvasPt) const;
    void drawDimensionTag(QPainter &p, const ShapeObject &obj, int colorIdx, bool selected);

    std::vector<ShapeObject> objects;
    double pxPerMM = 10.0;
    bool calibrated = false;
    double canvasW = 2100, canvasH = 2970;
    double zoom = 1.0;
    QPointF panOffset;
    bool showDims = true;
    bool showGrid = false;
    int selected = -1;

    // panning
    bool panning = false;
    QPointF lastMouse;

    // dragging move
    bool draggingObject = false;
    int draggedIndex = -1;
    QPointF dragStartCanvas;
    QPointF dragOrigCenter;
    QPainterPath dragOrigPath;
};

#endif // CANVASWIDGET_H
