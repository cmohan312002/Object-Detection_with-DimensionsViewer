#include "mainwindow.h"
#include "canvaswidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QFileDialog>
#include <QStandardPaths>
#include <QFileInfo>
#include <QApplication>
#include <QFrame>
#include <QGroupBox>
#include <QFontMetrics>
#include <QMessageBox>
#include <QShortcut>
#include <QMenu>
#include <QKeyEvent>
#include <QUndoCommand>
#include <cmath>
#include <algorithm>
#include <numeric>
/* ══════════════════════════════════════════════════════════════════════════
   Colour palette (same as original)
   ══════════════════════════════════════════════════════════════════════════ */
namespace C {
static const QColor BG       {245, 245, 242};
static const QColor SURFACE  {255, 255, 255};
static const QColor BORDER   {220, 210, 200};
static const QColor ACCENT   {235,  95,  20};
static const QColor SUCCESS  { 34, 160,  80};
static const QColor TEXT_PRI { 30,  30,  30};
static const QColor TEXT_SEC {110, 100,  90};
static const QColor GRID     {230, 220, 210};
static const QColor A4_BG   {255, 255, 255};
static const QColor A4_SHADE {200, 185, 170};
}

const std::array<QColor,6> CanvasWidget::COLORS {{
    {235,  95,  20},   // orange
    { 30, 130, 200},   // blue
    { 34, 160,  80},   // green
    {160,  40, 160},   // violet
    {200,  40,  60},   // red
    { 20, 160, 160},   // teal
}};

/* ══════════════════════════════════════════════════════════════════════════
   CanvasWidget (enhanced with move dragging)
   ══════════════════════════════════════════════════════════════════════════ */
CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(400, 400);
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void CanvasWidget::setObjects(const std::vector<ShapeObject> &objs, double pxMM, bool cal)
{
    objects  = objs;
    pxPerMM  = pxMM;
    calibrated = cal;
    canvasW  = 210.0 * pxMM;
    canvasH  = 297.0 * pxMM;
    fitToView();
}

void CanvasWidget::setSelectedIndex(int idx)
{
    selected = idx;
    update();
}

int CanvasWidget::getSelectedIndex() const { return selected; }

void CanvasWidget::fitToView()
{
    if (width() <= 0 || height() <= 0) { zoom = 1.0; panOffset = {0,0}; return; }
    double margin = 40.0;
    double zx = (width()  - margin*2) / canvasW;
    double zy = (height() - margin*2) / canvasH;
    zoom = std::min(zx, zy);
    clampZoom();
    panOffset = { (width()  - canvasW * zoom) * 0.5,
                 (height() - canvasH * zoom) * 0.5 };
    update();
}

void CanvasWidget::zoomToFit(const QRectF &rect)
{
    if (rect.isEmpty()) return;
    double margin = 20.0;
    double zx = (width() - margin*2) / (rect.width() * zoom);
    double zy = (height() - margin*2) / (rect.height() * zoom);
    double newZoom = std::min(zx, zy);
    newZoom = std::max(0.1, std::min(newZoom, 20.0));
    QPointF center = rect.center();
    panOffset = QPointF(width()/2.0, height()/2.0) - center * newZoom;
    zoom = newZoom;
    clampZoom();
    update();
}

void CanvasWidget::zoomIn()
{
    zoom *= 1.2;
    clampZoom();
    update();
}

void CanvasWidget::zoomOut()
{
    zoom /= 1.2;
    clampZoom();
    update();
}

void CanvasWidget::setShowDimensions(bool on) { showDims = on; update(); }
void CanvasWidget::setShowGrid(bool on) { showGrid = on; update(); }

void CanvasWidget::clampZoom()
{
    zoom = std::max(0.05, std::min(zoom, 20.0));
}

void CanvasWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    fitToView();
}

QTransform CanvasWidget::viewTransform() const
{
    QTransform t;
    t.translate(panOffset.x(), panOffset.y());
    t.scale(zoom, zoom);
    return t;
}

int CanvasWidget::hitTest(const QPointF &canvasPt) const
{
    for (int i = (int)objects.size()-1; i >= 0; --i) {
        if (objects[i].pathCanvas.contains(canvasPt))
            return i;
    }
    return -1;
}

/* ── paint (unchanged from original) ────────────────────────────────────── */
void CanvasWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.fillRect(rect(), C::BG);
    p.save();
    p.translate(panOffset.x(), panOffset.y());
    p.scale(zoom, zoom);

    // A4 shadow and sheet
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(180,160,140,60));
    p.drawRect(QRectF(6, 6, canvasW, canvasH));
    p.setBrush(C::A4_BG);
    p.setPen(QPen(C::BORDER, 1.0/zoom));
    p.drawRect(QRectF(0, 0, canvasW, canvasH));

    // Grid
    if (showGrid) {
        p.setPen(QPen(C::GRID, 0.5/zoom, Qt::DotLine));
        double step = 10.0 * pxPerMM;
        for (double x = step; x < canvasW; x += step)
            p.drawLine(QLineF(x, 0, x, canvasH));
        for (double y = step; y < canvasH; y += step)
            p.drawLine(QLineF(0, y, canvasW, y));
        p.setPen(QPen(QColor(210,200,190), 0.8/zoom));
        step = 50.0 * pxPerMM;
        for (double x = step; x < canvasW; x += step)
            p.drawLine(QLineF(x, 0, x, canvasH));
        for (double y = step; y < canvasH; y += step)
            p.drawLine(QLineF(0, y, canvasW, y));
    }

    // Ruler ticks
    p.setPen(QPen(QColor(180,160,140), 0.8/zoom));
    QFont rulerFont("Segoe UI", 7.0/zoom);
    p.setFont(rulerFont);
    p.setPen(QPen(C::TEXT_SEC, 0.5/zoom));
    for (int mm = 0; mm <= 210; mm += 10) {
        double x = mm * pxPerMM;
        p.drawLine(QLineF(x, 0, x, (mm%50==0 ? 12 : 6)/zoom));
        if (mm > 0 && mm < 210 && mm%50==0)
            p.drawText(QPointF(x+2/zoom, 16/zoom), QString::number(mm));
    }
    for (int mm = 0; mm <= 297; mm += 10) {
        double y = mm * pxPerMM;
        p.drawLine(QLineF(0, y, (mm%50==0 ? 12 : 6)/zoom, y));
    }

    // Draw shapes
    for (int i = 0; i < (int)objects.size(); ++i) {
        const auto &obj = objects[i];
        QColor col = COLORS[i % COLORS.size()];
        bool sel = (i == selected);
        p.setPen(QPen(col, sel ? 3.0/zoom : 1.8/zoom));
        p.setBrush(sel ? QColor(col.red(),col.green(),col.blue(),55)
                       : QColor(col.red(),col.green(),col.blue(),22));
        p.drawPath(obj.pathCanvas);
        if (sel) {
            p.setPen(QPen(col, 4.0/zoom, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            QRectF br = obj.pathCanvas.boundingRect();
            br.adjust(-6/zoom, -6/zoom, 6/zoom, 6/zoom);
            p.drawRect(br);
        }
        if (showDims) drawDimensionTag(p, obj, i, sel);
    }
    p.restore();
}

void CanvasWidget::drawDimensionTag(QPainter &p, const ShapeObject &obj, int colorIdx, bool selected)
{
    QColor col = COLORS[colorIdx % COLORS.size()];
    QRectF bb = obj.pathCanvas.boundingRect();
    QString line1 = QString("%1").arg(obj.shape);
    QString line2 = QString("W %1  H %2 mm").arg(obj.widthMM,0,'f',1).arg(obj.heightMM,0,'f',1);
    QString line3 = QString("A %1 mm²").arg(obj.areaMM2,0,'f',1);
    QFont f("Segoe UI", 8.5 / zoom);
    QFont fb("Segoe UI", 8.5 / zoom, QFont::Bold);
    QFontMetrics fm(f), fmb(fb);
    double pad = 6.0/zoom, lh = fmb.height();
    double tagW = std::max({(double)fmb.horizontalAdvance(line1),
                            (double)fm.horizontalAdvance(line2),
                            (double)fm.horizontalAdvance(line3)}) + pad*2;
    double tagH = lh*3 + pad*2 + lh*0.4;
    double gap = 14.0/zoom;
    double tagX = bb.right() + gap;
    double tagY = bb.center().y() - tagH*0.5;
    if (tagX + tagW > canvasW - 10/zoom) {
        tagX = bb.left() - tagW - gap;
    }
    tagY = std::max(tagY, 4.0/zoom);
    tagY = std::min(tagY, canvasH - tagH - 4.0/zoom);
    QRectF tagRect(tagX, tagY, tagW, tagH);
    QPointF lineStart = (tagX > bb.right()) ? QPointF(tagRect.left(), tagRect.center().y())
                                            : QPointF(tagRect.right(), tagRect.center().y());
    QPointF lineEnd   = (tagX > bb.right()) ? QPointF(bb.right(), bb.center().y())
                                          : QPointF(bb.left(),  bb.center().y());
    p.setPen(QPen(col, 0.8/zoom, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawLine(lineStart, lineEnd);
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    double r = 3.0/zoom;
    p.drawEllipse(lineEnd, r, r);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(160,140,120,30));
    p.drawRoundedRect(tagRect.adjusted(2/zoom,2/zoom,2/zoom,2/zoom), 5/zoom, 5/zoom);
    p.setBrush(QColor(255,255,255, selected ? 250 : 230));
    p.setPen(QPen(col, selected ? 1.5/zoom : 0.8/zoom));
    p.drawRoundedRect(tagRect, 5/zoom, 5/zoom);
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    p.drawRoundedRect(QRectF(tagX, tagY, 4.0/zoom, tagH), 2/zoom, 2/zoom);
    double bR = 8.0/zoom;
    QPointF bc(tagRect.right() - bR - 1/zoom, tagRect.top() + bR + 1/zoom);
    p.setBrush(col);
    p.drawEllipse(bc, bR, bR);
    p.setPen(Qt::white);
    p.setFont(QFont("Segoe UI", 6.5/zoom, QFont::Bold));
    p.drawText(QRectF(bc.x()-bR, bc.y()-bR, bR*2, bR*2), Qt::AlignCenter, QString::number(obj.id));
    double tx = tagX + 7.0/zoom;
    double ty = tagY + pad;
    p.setFont(fb);
    p.setPen(col);
    p.drawText(QRectF(tx, ty, tagW, lh), Qt::AlignLeft|Qt::AlignVCenter, line1);
    ty += lh + lh*0.2;
    p.setFont(f);
    p.setPen(C::TEXT_PRI);
    p.drawText(QRectF(tx, ty, tagW, lh), Qt::AlignLeft|Qt::AlignVCenter, line2);
    ty += lh + lh*0.2;
    p.setPen(C::TEXT_SEC);
    p.drawText(QRectF(tx, ty, tagW, lh), Qt::AlignLeft|Qt::AlignVCenter, line3);
}

/* ── Drag & drop move ───────────────────────────────────────────────────── */
void CanvasWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton ||
        (e->button() == Qt::LeftButton && e->modifiers() & Qt::AltModifier)) {
        panning = true;
        lastMouse = e->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() == Qt::LeftButton) {
        QTransform inv = viewTransform().inverted();
        QPointF pt = inv.map(e->position());
        int hit = hitTest(pt);
        if (hit >= 0 && hit == selected) {
            draggingObject = true;
            draggedIndex = hit;
            dragStartCanvas = pt;
            dragOrigCenter = QPointF(objects[hit].centerXmm, objects[hit].centerYmm);
            dragOrigPath = objects[hit].pathCanvas;
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        // else normal selection
        for (int i = (int)objects.size()-1; i >= 0; --i) {
            if (objects[i].pathCanvas.contains(pt)) {
                emit objectClicked(i);
                return;
            }
        }
        emit objectClicked(-1);
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (panning) {
        panOffset += e->position() - lastMouse;
        lastMouse = e->position();
        update();
    } else if (draggingObject && draggedIndex >= 0) {
        QTransform inv = viewTransform().inverted();
        QPointF curCanvas = inv.map(e->position());
        QPointF delta = curCanvas - dragStartCanvas;
        // apply movement
        ShapeObject &obj = objects[draggedIndex];
        obj.centerXmm = dragOrigCenter.x() + delta.x();
        obj.centerYmm = dragOrigCenter.y() + delta.y();
        QTransform trans;
        trans.translate(delta.x() * pxPerMM, delta.y() * pxPerMM);
        obj.pathCanvas = trans.map(dragOrigPath);
        update();
        emit objectMoved(draggedIndex);
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (draggingObject) {
        draggingObject = false;
        setCursor(Qt::ArrowCursor);
        emit objectMoveFinished(draggedIndex, dragOrigCenter, QPointF(objects[draggedIndex].centerXmm, objects[draggedIndex].centerYmm),
                                dragOrigPath, objects[draggedIndex].pathCanvas);
        draggedIndex = -1;
    }
    if (panning) {
        panning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void CanvasWidget::wheelEvent(QWheelEvent *e)
{
    double factor = (e->angleDelta().y() > 0) ? 1.15 : 1.0/1.15;
    QPointF mp = e->position();
    panOffset = mp - (mp - panOffset) * factor;
    zoom *= factor;
    clampZoom();
    update();
}

/* ══════════════════════════════════════════════════════════════════════════
   Undo/Redo Commands
   ══════════════════════════════════════════════════════════════════════════ */
class MoveShapeCommand : public QUndoCommand
{
public:
    MoveShapeCommand(std::vector<ShapeObject> *objs, int idx,
                     const QPointF &oldCenter, const QPainterPath &oldPath,
                     const QPointF &newCenter, const QPainterPath &newPath)
        : objects(objs), index(idx), oldCenter(oldCenter), newCenter(newCenter),
        oldPath(oldPath), newPath(newPath)
    {
        setText("Move Shape");
    }
    void undo() override {
        (*objects)[index].centerXmm = oldCenter.x();
        (*objects)[index].centerYmm = oldCenter.y();
        (*objects)[index].pathCanvas = oldPath;
    }
    void redo() override {
        (*objects)[index].centerXmm = newCenter.x();
        (*objects)[index].centerYmm = newCenter.y();
        (*objects)[index].pathCanvas = newPath;
    }
private:
    std::vector<ShapeObject> *objects;
    int index;
    QPointF oldCenter, newCenter;
    QPainterPath oldPath, newPath;
};

class DeleteShapeCommand : public QUndoCommand
{
public:
    DeleteShapeCommand(std::vector<ShapeObject> *objs, int idx, const ShapeObject &obj)
        : objects(objs), index(idx), deletedObject(obj) { setText("Delete Shape"); }
    void undo() override { objects->insert(objects->begin() + index, deletedObject); }
    void redo() override { objects->erase(objects->begin() + index); }
private:
    std::vector<ShapeObject> *objects;
    int index;
    ShapeObject deletedObject;
};

class AddShapeCommand : public QUndoCommand
{
public:
    AddShapeCommand(std::vector<ShapeObject> *objs, const ShapeObject &obj, int idx)
        : objects(objs), newObject(obj), index(idx) { setText("Add Shape"); }
    void undo() override { objects->erase(objects->begin() + index); }
    void redo() override { objects->insert(objects->begin() + index, newObject); }
private:
    std::vector<ShapeObject> *objects;
    ShapeObject newObject;
    int index;
};

/* ══════════════════════════════════════════════════════════════════════════
   MainWindow Implementation
   ══════════════════════════════════════════════════════════════════════════ */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), dirtyFlag(false)
{
    setWindowTitle("Shape Viewer • A4 Digitizer");
    resize(1280, 820);
    buildUI();
    applyTheme();

    connect(&undoStack, &QUndoStack::cleanChanged, this, &MainWindow::updateUndoRedoButtons);
    updateUndoRedoButtons();
}

MainWindow::~MainWindow() {}

void MainWindow::buildUI()
{
    auto mkBtn = [&](const QString &t, bool accent=false) {
        auto *b = new QPushButton(t);
        b->setFixedHeight(32);
        b->setCursor(Qt::PointingHandCursor);
        if (accent) b->setObjectName("accent");
        return b;
    };

    btnOpen    = mkBtn("📂 Open JSON", true);
    btnSave    = mkBtn("💾 Save");
    btnSaveAs  = mkBtn("📁 Save As");
    btnDelete  = mkBtn("🗑 Delete");
    btnDuplicate = mkBtn("📋 Duplicate");
    btnFit     = mkBtn("⊞ Fit View");
    btnZoomIn  = mkBtn("＋ Zoom");
    btnZoomOut = mkBtn("－ Zoom");
    btnUndo    = mkBtn("↩ Undo");
    btnRedo    = mkBtn("↪ Redo");
    chkDims    = mkBtn("📏 Dimensions ✓");
    chkGrid    = mkBtn("⊞ Grid");
    chkDims->setCheckable(true); chkDims->setChecked(true);
    chkGrid->setCheckable(true); chkGrid->setChecked(false);
    chkDims->setObjectName("toggle");
    chkGrid->setObjectName("toggle");

    lblFile  = new QLabel("No file loaded");
    lblCalib = new QLabel("–");
    lblCount = new QLabel("Objects: 0");

    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);
    toolbar->addWidget(btnOpen);
    toolbar->addWidget(btnSave);
    toolbar->addWidget(btnSaveAs);
    toolbar->addWidget(btnDelete);
    toolbar->addWidget(btnDuplicate);
    toolbar->addSpacing(8);
    toolbar->addWidget(btnFit);
    toolbar->addWidget(btnZoomIn);
    toolbar->addWidget(btnZoomOut);
    toolbar->addSpacing(8);
    toolbar->addWidget(btnUndo);
    toolbar->addWidget(btnRedo);
    toolbar->addSpacing(8);
    toolbar->addWidget(chkDims);
    toolbar->addWidget(chkGrid);
    toolbar->addStretch();
    toolbar->addWidget(lblCount);
    toolbar->addWidget(lblCalib);

    auto *tbFrame = new QWidget;
    tbFrame->setLayout(toolbar);
    tbFrame->setFixedHeight(46);
    tbFrame->setObjectName("toolbar");

    canvas = new CanvasWidget;

    objList = new QListWidget;
    objList->setFixedWidth(200);
    objList->setObjectName("objList");
    objList->setContextMenuPolicy(Qt::CustomContextMenu);

    // Right panel
    auto mkRow = [&](const QString &label, QLabel *&val) -> QWidget* {
        auto *row = new QWidget;
        auto *hl = new QHBoxLayout(row);
        hl->setContentsMargins(0,0,0,0);
        hl->setSpacing(6);
        auto *lbl = new QLabel(label);
        lbl->setObjectName("dimLabel");
        lbl->setFixedWidth(80);
        val = new QLabel("–");
        val->setObjectName("dimValue");
        val->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        hl->addWidget(lbl);
        hl->addWidget(val);
        return row;
    };

    auto *detailScroll = new QScrollArea;
    detailScroll->setFixedWidth(220);
    detailScroll->setWidgetResizable(true);
    detailScroll->setObjectName("detailScroll");
    detailScroll->setFrameShape(QFrame::NoFrame);
    auto *detailInner = new QWidget;
    auto *detailLayout = new QVBoxLayout(detailInner);
    detailLayout->setContentsMargins(12,12,12,12);
    detailLayout->setSpacing(4);

    auto *hdrLabel = new QLabel("Object Details");
    hdrLabel->setObjectName("detailHeader");
    QFont f = hdrLabel->font(); f.setBold(true); hdrLabel->setFont(f);
    detailLayout->addWidget(hdrLabel);
    detailLayout->addSpacing(8);
    dShape = new QLabel("–");
    dShape->setObjectName("shapeLabel");
    detailLayout->addWidget(dShape);
    detailLayout->addSpacing(6);

    auto mkSep = [&](const QString &t) {
        auto *s = new QLabel(t);
        s->setObjectName("sectionSep");
        return s;
    };

    detailLayout->addWidget(mkSep("DIMENSIONS"));
    detailLayout->addWidget(mkRow("Width",    dWidth));
    detailLayout->addWidget(mkRow("Height",   dHeight));
    detailLayout->addWidget(mkRow("Diagonal", dDiag));
    detailLayout->addSpacing(4);
    detailLayout->addWidget(mkSep("AREA & OUTLINE"));
    detailLayout->addWidget(mkRow("Area",     dArea));
    detailLayout->addWidget(mkRow("Perimeter",dPerim));
    detailLayout->addSpacing(4);
    detailLayout->addWidget(mkSep("GEOMETRY"));
    detailLayout->addWidget(mkRow("Aspect",   dAspect));
    detailLayout->addWidget(mkRow("Angle",    dAngle));
    detailLayout->addSpacing(4);
    detailLayout->addWidget(mkSep("POSITION ON SHEET"));
    detailLayout->addWidget(mkRow("Centre", dCenter));
    detailLayout->addStretch();
    detailScroll->setWidget(detailInner);

    // Status bar
    auto *statusBar = new QHBoxLayout;
    statusBar->addWidget(lblFile);
    statusBar->addStretch();
    auto *statusFrame = new QWidget;
    statusFrame->setLayout(statusBar);
    statusFrame->setFixedHeight(28);
    statusFrame->setObjectName("statusBar");

    // Splitter
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(objList);
    splitter->addWidget(canvas);
    splitter->addWidget(detailScroll);
    splitter->setStretchFactor(0,0);
    splitter->setStretchFactor(1,1);
    splitter->setStretchFactor(2,0);
    splitter->setSizes({200,860,220});

    auto *root = new QWidget;
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0,0,0,0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(tbFrame);
    rootLayout->addWidget(splitter,1);
    rootLayout->addWidget(statusFrame);
    setCentralWidget(root);

    // Connections
    connect(btnOpen, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::saveFile);
    connect(btnSaveAs, &QPushButton::clicked, this, &MainWindow::saveFileAs);
    connect(btnDelete, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    connect(btnDuplicate, &QPushButton::clicked, this, &MainWindow::duplicateSelected);
    connect(btnFit, &QPushButton::clicked, canvas, &CanvasWidget::fitToView);
    connect(btnZoomIn, &QPushButton::clicked, canvas, &CanvasWidget::zoomIn);
    connect(btnZoomOut, &QPushButton::clicked, canvas, &CanvasWidget::zoomOut);
    connect(btnUndo, &QPushButton::clicked, &undoStack, &QUndoStack::undo);
    connect(btnRedo, &QPushButton::clicked, &undoStack, &QUndoStack::redo);
    connect(chkDims, &QPushButton::toggled, canvas, &CanvasWidget::setShowDimensions);
    connect(chkGrid, &QPushButton::toggled, canvas, &CanvasWidget::setShowGrid);
    connect(objList, &QListWidget::currentRowChanged, this, &MainWindow::onObjectSelected);
    connect(canvas, &CanvasWidget::objectClicked, this, &MainWindow::onCanvasClicked);
    connect(canvas, &CanvasWidget::objectMoved, this, &MainWindow::onObjectMoved);
    connect(canvas, &CanvasWidget::objectMoveFinished, this,
            [this](int idx, QPointF oldC, QPointF newC, QPainterPath oldP, QPainterPath newP){
                undoStack.push(new MoveShapeCommand(&objects, idx, oldC, oldP, newC, newP));
                markDirty();
            });
    connect(canvas, &CanvasWidget::customContextMenuRequested, this, &MainWindow::showCanvasContextMenu);
    connect(objList, &QListWidget::customContextMenuRequested, this, &MainWindow::showListContextMenu);

    // Shortcuts
    new QShortcut(QKeySequence::Delete, this, SLOT(deleteSelected()));
    new QShortcut(Qt::Key_Plus, canvas, SLOT(zoomIn()));
    new QShortcut(Qt::Key_Minus, canvas, SLOT(zoomOut()));
    new QShortcut(Qt::Key_0, canvas, SLOT(fitToView()));
    new QShortcut(QKeySequence::Undo, this, SLOT(undo()));
    new QShortcut(QKeySequence::Redo, this, SLOT(redo()));

    // Add with your other mkBtn declarations:
    btnPack = mkBtn("📦 Pack Shapes");

    // Add to toolbar layout after btnDuplicate:
    toolbar->addWidget(btnPack);

    // Add to connections:
    connect(btnPack, &QPushButton::clicked, this, &MainWindow::packShapes);
    // Load default file as in original
    loadJson("C:/Users/shrad/Documents/digitized_20260325_095714.json");
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(R"(
        QWidget { background:#f5f5f2; color:#1e1e1e; font-family:"Segoe UI","Helvetica Neue",sans-serif; font-size:12px; }
        QWidget#toolbar { background:#ffffff; border-bottom:1px solid #e0d8d0; }
        QWidget#statusBar { background:#ffffff; border-top:1px solid #e0d8d0; padding:0 12px; }
        QPushButton { background:#fff; color:#1e1e1e; border:1px solid #d8cfc8; border-radius:6px; padding:0 14px; font-size:12px; }
        QPushButton:hover { background:#fff3ec; border-color:#eb5f14; color:#eb5f14; }
        QPushButton:pressed { background:#ffe8d8; }
        QPushButton:disabled { color:#b8b0a8; border-color:#e8e2dc; }
        QPushButton#accent { background:#eb5f14; border-color:#eb5f14; color:#fff; font-weight:600; }
        QPushButton#accent:hover { background:#d45210; }
        QPushButton#toggle { background:#fff; border:1px solid #d0c8c0; }
        QPushButton#toggle:checked { background:#fff3ec; border-color:#eb5f14; color:#eb5f14; font-weight:600; }
        QListWidget#objList { background:#fff; border:none; border-right:1px solid #e0d8d0; font-size:12px; }
        QListWidget#objList::item { padding:8px 12px; border-bottom:1px solid #f0e8e0; }
        QListWidget#objList::item:selected { background:#fff3ec; color:#eb5f14; font-weight:600; }
        QListWidget#objList::item:hover { background:#faf5f0; }
        QScrollArea#detailScroll { background:#fff; border-left:1px solid #e0d8d0; }
        QLabel#detailHeader { font-size:13px; font-weight:700; color:#1e1e1e; }
        QLabel#shapeLabel { font-size:15px; font-weight:700; color:#eb5f14; }
        QLabel#sectionSep { font-size:9px; font-weight:700; color:#9e9088; letter-spacing:1px; border-bottom:1px solid #e8e0d8; padding-bottom:2px; margin-top:4px; }
        QLabel#dimLabel { color:#7e7068; font-size:11px; }
        QLabel#dimValue { color:#1e1e1e; font-size:12px; font-weight:600; }
        QSplitter::handle { background:#e0d8d0; width:1px; }
        QLabel { color:#6e6460; }
        QScrollBar:vertical { width:6px; background:#f5f5f2; }
        QScrollBar::handle:vertical { background:#d0c8c0; border-radius:3px; }
    )");
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    int delta = 1; // mm per step
    if (e->modifiers() & Qt::ControlModifier) delta = 5;
    if (e->modifiers() & Qt::ShiftModifier) delta = 0.1;
    if (e->key() == Qt::Key_Left) nudgeSelected(-delta, 0);
    else if (e->key() == Qt::Key_Right) nudgeSelected(delta, 0);
    else if (e->key() == Qt::Key_Up) nudgeSelected(0, -delta);
    else if (e->key() == Qt::Key_Down) nudgeSelected(0, delta);
    else QMainWindow::keyPressEvent(e);
}

void MainWindow::nudgeSelected(int dx_mm, int dy_mm)
{
    int idx = canvas->getSelectedIndex();
    if (idx < 0 || idx >= (int)objects.size()) return;
    ShapeObject oldObj = objects[idx];
    objects[idx].centerXmm += dx_mm;
    objects[idx].centerYmm += dy_mm;
    QTransform trans;
    trans.translate(dx_mm * pxPerMM, dy_mm * pxPerMM);
    objects[idx].pathCanvas = trans.map(objects[idx].pathCanvas);
    undoStack.push(new MoveShapeCommand(&objects, idx,
                                        QPointF(oldObj.centerXmm, oldObj.centerYmm), oldObj.pathCanvas,
                                        QPointF(objects[idx].centerXmm, objects[idx].centerYmm), objects[idx].pathCanvas));
    canvas->setObjects(objects, pxPerMM, calibrated);
    canvas->setSelectedIndex(idx);
    showObjectDetails(idx);
    markDirty();
}

void MainWindow::openFile()
{
    QString path = QFileDialog::getOpenFileName(this, "Open Digitized JSON",
                                                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "JSON files (*.json)");
    if (!path.isEmpty()) loadJson(path);
}

void MainWindow::saveFile()
{
    if (filePath.isEmpty()) saveFileAs();
    else saveToFile(filePath);
}

void MainWindow::saveFileAs()
{
    QString path = QFileDialog::getSaveFileName(this, "Save JSON",
                                                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "JSON files (*.json)");
    if (!path.isEmpty()) {
        filePath = path;
        saveToFile(path);
    }
}

void MainWindow::saveToFile(const QString &path)
{
    if (objects.empty()) {
        QMessageBox::warning(this, "Export Failed", "No objects to save.");
        return;
    }

    QJsonObject root;
    root["version"]     = "3.0";
    root["timestamp"]   = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["unit"]        = "mm";
    root["calibrated"]  = calibrated;
    root["calib_rms_px"] = 0.0;  // not used in this viewer, keep placeholder
    root["pixelsPerMM"] = pxPerMM;

    QJsonObject canvas;
    canvas["width_mm"]  = 210.0;
    canvas["height_mm"] = 297.0;
    root["canvas"] = canvas;

    QJsonArray arr;
    for (int i = 0; i < (int)objects.size(); ++i) {
        const auto &obj = objects[i];
        QJsonObject jo;
        jo["id"]    = obj.id;
        jo["shape"] = obj.shape;

        // Dimensions block (already in mm)
        QJsonObject dim;
        dim["width_mm"]      = qRound(obj.widthMM  * 100) / 100.0;
        dim["height_mm"]     = qRound(obj.heightMM * 100) / 100.0;
        dim["diagonal_mm"]   = qRound(obj.diagonalMM * 100) / 100.0;
        dim["perimeter_mm"]  = qRound(obj.perimMM  * 10)  / 10.0;
        dim["area_mm2"]      = qRound(obj.areaMM2  * 10)  / 10.0;
        dim["aspect_ratio"]  = qRound(obj.aspectRatio * 1000) / 1000.0;
        dim["angle_deg"]     = qRound(obj.angleDeg * 10) / 10.0;
        jo["dimensions"] = dim;

        // Centre position
        QJsonObject cen;
        cen["x_mm"] = qRound(obj.centerXmm * 100) / 100.0;
        cen["y_mm"] = qRound(obj.centerYmm * 100) / 100.0;
        jo["center_on_sheet"] = cen;

        // Path: convert from pixel coordinates to mm
        QJsonArray pa;
        auto toMM = [this](double px) { return px / pxPerMM; };
        auto r2 = [](double v) { return qRound(v * 100) / 100.0; };

        const QPainterPath &pathPx = obj.pathCanvas;
        for (int e = 0; e < pathPx.elementCount(); ++e) {
            QPainterPath::Element el = pathPx.elementAt(e);
            QJsonObject cmd;
            switch (el.type) {
            case QPainterPath::MoveToElement:
                cmd["cmd"] = "M";
                cmd["x"] = r2(toMM(el.x));
                cmd["y"] = r2(toMM(el.y));
                break;
            case QPainterPath::LineToElement:
                cmd["cmd"] = "L";
                cmd["x"] = r2(toMM(el.x));
                cmd["y"] = r2(toMM(el.y));
                break;
            case QPainterPath::CurveToElement:
            {
                // Cubic Bezier: need next two elements
                QPainterPath::Element ctrl2 = pathPx.elementAt(e+1);
                QPainterPath::Element end   = pathPx.elementAt(e+2);
                cmd["cmd"] = "C";
                cmd["x1"] = r2(toMM(el.x));
                cmd["y1"] = r2(toMM(el.y));
                cmd["x2"] = r2(toMM(ctrl2.x));
                cmd["y2"] = r2(toMM(ctrl2.y));
                cmd["x"]  = r2(toMM(end.x));
                cmd["y"]  = r2(toMM(end.y));
                e += 2;   // skip the two curve data elements
                break;
            }
            case QPainterPath::CurveToDataElement:
                // Should not appear alone – ignore
                continue;
            }
            pa.append(cmd);
        }
        // Always add a closing "Z" command (your reference does this)
        QJsonObject closeCmd;
        closeCmd["cmd"] = "Z";
        pa.append(closeCmd);
        jo["path"] = pa;

        arr.append(jo);
    }
    root["objects"] = arr;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Save Failed", "Cannot write file: " + path);
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    dirtyFlag = false;
    undoStack.setClean();
    updateStatusBar();
}
void MainWindow::loadJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        lblFile->setText("Failed to open: " + path);
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isNull()) { lblFile->setText("Invalid JSON"); return; }

    QJsonObject root = doc.object();
    pxPerMM = root["pixelsPerMM"].toDouble(10.0);
    calibrated = root["calibrated"].toBool(false);
    filePath = path;
    objects.clear();

    QJsonArray arr = root["objects"].toArray();
    for (auto v : arr) {
        QJsonObject obj = v.toObject();
        ShapeObject so;
        so.id = obj["id"].toInt();
        so.shape = obj["shape"].toString();
        if (obj.contains("dimensions")) {
            QJsonObject d = obj["dimensions"].toObject();
            so.widthMM    = d["width_mm"]    .toDouble();
            so.heightMM   = d["height_mm"]   .toDouble();
            so.diagonalMM = d["diagonal_mm"] .toDouble();
            so.perimMM    = d["perimeter_mm"].toDouble();
            so.areaMM2    = d["area_mm2"]    .toDouble();
            so.aspectRatio= d["aspect_ratio"].toDouble();
            so.angleDeg   = d["angle_deg"]   .toDouble();
        } else {
            so.widthMM    = obj["width_mm"] .toDouble();
            so.heightMM   = obj["height_mm"].toDouble();
            so.perimMM    = obj["perim_mm"] .toDouble();
            so.areaMM2    = obj["area_mm2"] .toDouble();
            so.angleDeg   = obj["angle_deg"].toDouble();
            so.diagonalMM = std::sqrt(so.widthMM*so.widthMM + so.heightMM*so.heightMM);
            so.aspectRatio= so.heightMM>0 ? so.widthMM/so.heightMM : 0;
        }
        QString centreKey = obj.contains("center_on_sheet") ? "center_on_sheet" : "center";
        QJsonObject cen = obj[centreKey].toObject();
        so.centerXmm = cen["x_mm"].toDouble();
        so.centerYmm = cen["y_mm"].toDouble();
        QJsonArray pa = obj["path"].toArray();
        QPainterPath path;
        for (auto cv : pa) {
            QJsonObject cmd = cv.toObject();
            QString t = cmd["cmd"].toString();
            if (t == "M")
                path.moveTo(cmd["x"].toDouble()*pxPerMM, cmd["y"].toDouble()*pxPerMM);
            else if (t == "L")
                path.lineTo(cmd["x"].toDouble()*pxPerMM, cmd["y"].toDouble()*pxPerMM);
            else if (t == "C")
                path.cubicTo(cmd["x1"].toDouble()*pxPerMM, cmd["y1"].toDouble()*pxPerMM,
                             cmd["x2"].toDouble()*pxPerMM, cmd["y2"].toDouble()*pxPerMM,
                             cmd["x"].toDouble()*pxPerMM,  cmd["y"].toDouble()*pxPerMM);
            else if (t == "Z")
                path.closeSubpath();
        }
        so.pathCanvas = path;
        objects.push_back(so);
    }

    canvas->setObjects(objects, pxPerMM, calibrated);
    populateSidebar();
    updateStatusBar();
    showObjectDetails(-1);
    undoStack.clear();
    dirtyFlag = false;
}

void MainWindow::populateSidebar()
{
    objList->clear();
    for (int i = 0; i < (int)objects.size(); ++i) {
        const auto &o = objects[i];
        QColor col = CanvasWidget::COLORS[i % CanvasWidget::COLORS.size()];
        auto *item = new QListWidgetItem;
        item->setText(QString("  #%1  %2\n  %3 × %4 mm")
                          .arg(o.id).arg(o.shape).arg(o.widthMM,0,'f',1).arg(o.heightMM,0,'f',1));
        QPixmap px(12,12);
        px.fill(Qt::transparent);
        QPainter pp(&px);
        pp.setRenderHint(QPainter::Antialiasing);
        pp.setBrush(col); pp.setPen(Qt::NoPen);
        pp.drawEllipse(px.rect());
        item->setIcon(QIcon(px));
        item->setSizeHint(QSize(-1,52));
        objList->addItem(item);
    }
}

void MainWindow::updateStatusBar()
{
    QFileInfo fi(filePath);
    lblFile->setText(fi.fileName().isEmpty() ? "No file" : fi.fileName() + (dirtyFlag ? " *" : ""));
    lblCount->setText(QString("Objects: %1").arg(objects.size()));
    if (calibrated) {
        lblCalib->setText("📐 Calibrated");
        lblCalib->setStyleSheet("color:#1a6e3a;font-weight:600;font-size:11px;");
    } else {
        lblCalib->setText("⚠ Uncalibrated");
        lblCalib->setStyleSheet("color:#eb8c00;font-size:11px;");
    }
}

void MainWindow::showObjectDetails(int idx)
{
    if (idx < 0 || idx >= (int)objects.size()) {
        dShape->setText("–");
        for (auto *l : {dWidth,dHeight,dDiag,dArea,dPerim,dAspect,dAngle,dCenter})
            l->setText("–");
        return;
    }
    const auto &o = objects[idx];
    QColor col = CanvasWidget::COLORS[idx % CanvasWidget::COLORS.size()];
    dShape->setText(o.shape);
    dShape->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(col.name()));
    dWidth->setText(QString("%1 mm").arg(o.widthMM,0,'f',2));
    dHeight->setText(QString("%1 mm").arg(o.heightMM,0,'f',2));
    dDiag->setText(QString("%1 mm").arg(o.diagonalMM,0,'f',2));
    dArea->setText(QString("%1 mm²").arg(o.areaMM2,0,'f',1));
    dPerim->setText(QString("%1 mm").arg(o.perimMM,0,'f',1));
    dAspect->setText(QString("%1 : 1").arg(o.aspectRatio,0,'f',2));
    dAngle->setText(QString("%1°").arg(o.angleDeg,0,'f',1));
    dCenter->setText(QString("(%1, %2) mm").arg(o.centerXmm,0,'f',1).arg(o.centerYmm,0,'f',1));
}

void MainWindow::markDirty(bool dirty)
{
    dirtyFlag = dirty;
    updateStatusBar();
}

void MainWindow::deleteSelected()
{
    int idx = canvas->getSelectedIndex();
    if (idx < 0 || idx >= (int)objects.size()) return;
    undoStack.push(new DeleteShapeCommand(&objects, idx, objects[idx]));
    canvas->setObjects(objects, pxPerMM, calibrated);
    populateSidebar();
    updateStatusBar();
    int newSel = (idx < (int)objects.size()) ? idx : (int)objects.size()-1;
    canvas->setSelectedIndex(newSel);
    showObjectDetails(newSel);
    if (newSel >= 0) objList->setCurrentRow(newSel);
    else objList->clearSelection();
    markDirty();
}

void MainWindow::duplicateSelected()
{
    int idx = canvas->getSelectedIndex();
    if (idx < 0) return;
    ShapeObject copy = objects[idx];
    copy.id = getNextId();
    copy.centerXmm += 10.0;
    copy.centerYmm += 10.0;
    QTransform trans;
    trans.translate(10.0 * pxPerMM, 10.0 * pxPerMM);
    copy.pathCanvas = trans.map(copy.pathCanvas);
    int newIdx = (int)objects.size();
    undoStack.push(new AddShapeCommand(&objects, copy, newIdx));
    objects.push_back(copy);
    canvas->setObjects(objects, pxPerMM, calibrated);
    populateSidebar();
    canvas->setSelectedIndex(newIdx);
    objList->setCurrentRow(newIdx);
    showObjectDetails(newIdx);
    markDirty();
}

void MainWindow::zoomToSelected()
{
    int idx = canvas->getSelectedIndex();
    if (idx >= 0 && idx < (int)objects.size()) {
        canvas->zoomToFit(objects[idx].pathCanvas.boundingRect());
    }
}

void MainWindow::onObjectSelected(int row)
{
    canvas->setSelectedIndex(row);
    showObjectDetails(row);
}

void MainWindow::onCanvasClicked(int index)
{
    canvas->setSelectedIndex(index);
    showObjectDetails(index);
    if (index >= 0 && index < objList->count())
        objList->setCurrentRow(index);
    else
        objList->clearSelection();
}

void MainWindow::onObjectMoved(int idx)
{
    if (idx >= 0 && idx < (int)objects.size()) {
        showObjectDetails(idx);
        markDirty();
    }
}

void MainWindow::updateUndoRedoButtons()
{
    btnUndo->setEnabled(undoStack.canUndo());
    btnRedo->setEnabled(undoStack.canRedo());
}

void MainWindow::showCanvasContextMenu(const QPoint &pos)
{
    QMenu menu;
    int idx = canvas->getSelectedIndex();
    if (idx >= 0) {
        menu.addAction("Delete", this, &MainWindow::deleteSelected);
        menu.addAction("Duplicate", this, &MainWindow::duplicateSelected);
        menu.addAction("Zoom to Selected", this, &MainWindow::zoomToSelected);
        menu.addSeparator();
    }
    menu.addAction("Fit View", canvas, &CanvasWidget::fitToView);
    menu.exec(canvas->mapToGlobal(pos));
}

void MainWindow::showListContextMenu(const QPoint &pos)
{
    QModelIndex idx = objList->indexAt(pos);
    if (!idx.isValid()) return;
    int row = idx.row();
    if (row >= 0 && row < (int)objects.size()) {
        QMenu menu;
        menu.addAction("Delete", this, &MainWindow::deleteSelected);
        menu.addAction("Duplicate", this, &MainWindow::duplicateSelected);
        menu.addAction("Zoom to Selected", this, &MainWindow::zoomToSelected);
        menu.exec(objList->mapToGlobal(pos));
    }
}
/* ══════════════════════════════════════════════════════════════════════════
   Undo command for pack operation (saves all old positions at once)
   ══════════════════════════════════════════════════════════════════════════ */
class PackShapesCommand : public QUndoCommand
{
public:
    struct ObjState {
        int index;
        double oldCX, oldCY, newCX, newCY;
        QPainterPath oldPath, newPath;
    };

    PackShapesCommand(std::vector<ShapeObject> *objs, const std::vector<ObjState> &states)
        : objects(objs), states(states) { setText("Pack Shapes"); }

    void undo() override {
        for (auto &s : states) {
            (*objects)[s.index].centerXmm = s.oldCX;
            (*objects)[s.index].centerYmm = s.oldCY;
            (*objects)[s.index].pathCanvas = s.oldPath;
        }
    }
    void redo() override {
        for (auto &s : states) {
            (*objects)[s.index].centerXmm = s.newCX;
            (*objects)[s.index].centerYmm = s.newCY;
            (*objects)[s.index].pathCanvas = s.newPath;
        }
    }
private:
    std::vector<ShapeObject> *objects;
    std::vector<ObjState> states;
};

/* ── Pack / arrange shapes into minimal space ───────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════
   2D Bin-Packing: Maximal Rectangles (Best Short Side Fit)
   No overlaps, fills gaps greedily, uses all available space
   ══════════════════════════════════════════════════════════════════════════ */
void MainWindow::packShapes()
{
    if (objects.empty()) return;

    const double MARGIN = 8.0;   // mm from page edge
    const double GAP    = 5.0;   // mm between shapes
    const double PAGE_W = 210.0;
    const double PAGE_H = 297.0;

    // Available packing area
    const double areaX = MARGIN;
    const double areaY = MARGIN;
    const double areaW = PAGE_W - MARGIN * 2;
    const double areaH = PAGE_H - MARGIN * 2;

    // A free rectangle on the page
    struct FreeRect {
        double x, y, w, h;
    };

    // Start with one big free rectangle = entire usable page
    std::vector<FreeRect> freeRects = {{ areaX, areaY, areaW, areaH }};

    // Sort shapes: largest area first (better packing)
    std::vector<int> order(objects.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return (objects[a].widthMM * objects[a].heightMM) >
               (objects[b].widthMM * objects[b].heightMM);
    });

    // Placed positions: index -> (newCX, newCY)
    std::vector<std::pair<int, QPointF>> placements;

    // Track placed bounding boxes to split free rects
    struct PlacedRect { double x, y, w, h; };
    std::vector<PlacedRect> placed;

    for (int idx : order) {
        const ShapeObject &obj = objects[idx];
        double sw = obj.widthMM  + GAP;
        double sh = obj.heightMM + GAP;

        // Find best free rect using BSSF (Best Short Side Fit)
        int    bestFree  = -1;
        double bestScore = 1e18;

        for (int i = 0; i < (int)freeRects.size(); ++i) {
            const FreeRect &fr = freeRects[i];
            if (fr.w >= sw && fr.h >= sh) {
                // Short side fit score: minimize the smaller leftover dimension
                double score = std::min(fr.w - sw, fr.h - sh);
                if (score < bestScore) {
                    bestScore = score;
                    bestFree  = i;
                }
            }
        }

        if (bestFree < 0) {
            // No space found — place below everything as fallback
            double maxY = areaY;
            for (auto &p : placed) maxY = std::max(maxY, p.y + p.h);
            placements.push_back({ idx, QPointF(areaX + obj.widthMM * 0.5,
                                               maxY + obj.heightMM * 0.5) });
            placed.push_back({ areaX, maxY, obj.widthMM + GAP, obj.heightMM + GAP });
            continue;
        }

        FreeRect &fr = freeRects[bestFree];
        double px = fr.x;  // top-left of placed shape (with gap baked in later)
        double py = fr.y;

        // Record placement (center position)
        placements.push_back({ idx, QPointF(px + obj.widthMM * 0.5,
                                           py + obj.heightMM * 0.5) });
        PlacedRect pr = { px, py, obj.widthMM + GAP, obj.heightMM + GAP };
        placed.push_back(pr);

        // Split all free rects that overlap with this placed rect
        std::vector<FreeRect> newFreeRects;
        for (auto &r : freeRects) {
            // Does placed rect intersect this free rect?
            if (pr.x >= r.x + r.w || pr.x + pr.w <= r.x ||
                pr.y >= r.y + r.h || pr.y + pr.h <= r.y) {
                newFreeRects.push_back(r);  // no overlap, keep as-is
                continue;
            }
            // Split into up to 4 sub-rectangles around the placed shape
            // Left slice
            if (pr.x > r.x)
                newFreeRects.push_back({ r.x, r.y, pr.x - r.x, r.h });
            // Right slice
            if (pr.x + pr.w < r.x + r.w)
                newFreeRects.push_back({ pr.x + pr.w, r.y,
                                        r.x + r.w - (pr.x + pr.w), r.h });
            // Top slice
            if (pr.y > r.y)
                newFreeRects.push_back({ r.x, r.y, r.w, pr.y - r.y });
            // Bottom slice
            if (pr.y + pr.h < r.y + r.h)
                newFreeRects.push_back({ r.x, pr.y + pr.h,
                                        r.w, r.y + r.h - (pr.y + pr.h) });
        }

        // Remove free rects that are fully contained inside another (deduplicate)
        std::vector<FreeRect> pruned;
        for (int i = 0; i < (int)newFreeRects.size(); ++i) {
            bool contained = false;
            for (int j = 0; j < (int)newFreeRects.size(); ++j) {
                if (i == j) continue;
                const FreeRect &a = newFreeRects[i];
                const FreeRect &b = newFreeRects[j];
                if (b.x <= a.x && b.y <= a.y &&
                    b.x + b.w >= a.x + a.w &&
                    b.y + b.h >= a.y + a.h) {
                    contained = true;
                    break;
                }
            }
            if (!contained) pruned.push_back(newFreeRects[i]);
        }
        freeRects = pruned;
    }

    // Now apply all placements as one undoable command
    std::vector<PackShapesCommand::ObjState> states;
    for (auto &[idx, newCenter] : placements) {
        ShapeObject &obj = objects[idx];

        PackShapesCommand::ObjState state;
        state.index   = idx;
        state.oldCX   = obj.centerXmm;
        state.oldCY   = obj.centerYmm;
        state.oldPath = obj.pathCanvas;

        double dxMM = newCenter.x() - obj.centerXmm;
        double dyMM = newCenter.y() - obj.centerYmm;

        obj.centerXmm = newCenter.x();
        obj.centerYmm = newCenter.y();
        QTransform trans;
        trans.translate(dxMM * pxPerMM, dyMM * pxPerMM);
        obj.pathCanvas = trans.map(state.oldPath);

        state.newCX   = obj.centerXmm;
        state.newCY   = obj.centerYmm;
        state.newPath = obj.pathCanvas;
        states.push_back(state);
    }

    undoStack.push(new PackShapesCommand(&objects, states));
    canvas->setObjects(objects, pxPerMM, calibrated);
    canvas->fitToView();
    populateSidebar();
    showObjectDetails(canvas->getSelectedIndex());
    markDirty();
}
int MainWindow::getNextId() const
{
    int maxId = 0;
    for (const auto &obj : objects)
        if (obj.id > maxId) maxId = obj.id;
    return maxId + 1;
}
