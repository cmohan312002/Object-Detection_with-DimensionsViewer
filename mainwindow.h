#pragma once

#include <QMainWindow>
#include <QPainterPath>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFileDialog>
#include <QWidget>
#include <vector>
#include <array>

/* ── one detected object ─────────────────────────────────────────────── */
struct ShapeObject {
    int         id          = 0;
    QString     shape;
    double      widthMM     = 0;
    double      heightMM    = 0;
    double      diagonalMM  = 0;
    double      perimMM     = 0;
    double      areaMM2     = 0;
    double      aspectRatio = 0;
    double      angleDeg    = 0;
    double      centerXmm   = 0;
    double      centerYmm   = 0;
    QPainterPath pathCanvas; // in canvas-pixel space (px = mm * pxPerMM)
};

/* ── zoomable/pannable canvas ────────────────────────────────────────── */
class CanvasWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    void setObjects(const std::vector<ShapeObject> &objs, double pxPerMM,
                    bool calibrated);
    void setSelectedIndex(int idx);
    void setShowDimensions(bool v) { showDims = v; update(); }
    void setShowGrid(bool v)       { showGrid = v; update(); }
    void fitToView();
    void zoomIn()  { zoom *= 1.2; clampZoom(); update(); }
    void zoomOut() { zoom /= 1.2; clampZoom(); update(); }
    void resetZoom(){ fitToView(); }
    static const std::array<QColor,6> COLORS;

signals:
    void objectClicked(int index);

protected:
    void paintEvent   (QPaintEvent  *) override;
    void wheelEvent   (QWheelEvent  *) override;
    void mousePressEvent  (QMouseEvent *) override;
    void mouseMoveEvent   (QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent  (QResizeEvent *) override;

private:
    /* helpers */
    QTransform viewTransform() const;
    void       clampZoom();
    void       drawDimensionTag(QPainter &p, const ShapeObject &obj,
                          int colorIdx, bool selected);
    void       drawLeaderLine(QPainter &p, const ShapeObject &obj,
                        QPointF tagCenter, int colorIdx);

    std::vector<ShapeObject> objects;
    double  pxPerMM    = 10.0;
    bool    calibrated = false;
    int     selected   = -1;
    bool    showDims   = true;
    bool    showGrid   = false;

    /* pan/zoom state */
    double  zoom       = 1.0;
    QPointF panOffset  = {0,0};
    bool    panning    = false;
    QPointF lastMouse;

    /* A4 canvas size in px */
    double canvasW = 2100;
    double canvasH = 2970;

    // static const std::array<QColor,6> COLORS;
};

/* ── main window ─────────────────────────────────────────────────────── */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void openFile();
    void onObjectSelected(int row);
    void onCanvasClicked(int index);

private:
    void buildUI();
    void applyTheme();
    void loadJson(const QString &path);
    void populateSidebar();
    void showObjectDetails(int idx);
    void updateStatusBar();

    /* data */
    std::vector<ShapeObject> objects;
    double  pxPerMM    = 10.0;
    bool    calibrated = false;
    QString filePath;

    /* UI */
    CanvasWidget *canvas      = nullptr;
    QListWidget  *objList     = nullptr;
    QWidget      *detailPanel = nullptr;
    QLabel       *lblFile     = nullptr;
    QLabel       *lblCalib    = nullptr;
    QLabel       *lblCount    = nullptr;

    /* detail labels (right panel) */
    QLabel *dShape    = nullptr;
    QLabel *dWidth    = nullptr;
    QLabel *dHeight   = nullptr;
    QLabel *dDiag     = nullptr;
    QLabel *dArea     = nullptr;
    QLabel *dPerim    = nullptr;
    QLabel *dAspect   = nullptr;
    QLabel *dAngle    = nullptr;
    QLabel *dCenter   = nullptr;

    QPushButton *btnOpen   = nullptr;
    QPushButton *btnFit    = nullptr;
    QPushButton *btnZoomIn = nullptr;
    QPushButton *btnZoomOut= nullptr;
    QPushButton *chkDims   = nullptr;   // toggle button
    QPushButton *chkGrid   = nullptr;
};
