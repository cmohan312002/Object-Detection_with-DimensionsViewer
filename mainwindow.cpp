#include "mainwindow.h"

#include <QPainter>
#include <QPainterPath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
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
#include <cmath>
#include <algorithm>

/* ══════════════════════════════════════════════════════════════════════════
   Colour palette
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
   CanvasWidget
   ══════════════════════════════════════════════════════════════════════════ */
CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(400, 400);
}

void CanvasWidget::setObjects(const std::vector<ShapeObject> &objs,
                              double pxMM, bool cal)
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

/* ── paint ─────────────────────────────────────────────────────────────── */
void CanvasWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    /* widget background */
    p.fillRect(rect(), C::BG);

    p.save();
    p.translate(panOffset.x(), panOffset.y());
    p.scale(zoom, zoom);

    /* ── A4 sheet shadow ── */
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(180,160,140,60));
    p.drawRect(QRectF(6, 6, canvasW, canvasH));

    /* ── A4 sheet ── */
    p.setBrush(C::A4_BG);
    p.setPen(QPen(C::BORDER, 1.0/zoom));
    p.drawRect(QRectF(0, 0, canvasW, canvasH));

    /* ── 10 mm grid ── */
    if (showGrid) {
        p.setPen(QPen(C::GRID, 0.5/zoom, Qt::DotLine));
        double step = 10.0 * pxPerMM;
        for (double x = step; x < canvasW; x += step)
            p.drawLine(QLineF(x, 0, x, canvasH));
        for (double y = step; y < canvasH; y += step)
            p.drawLine(QLineF(0, y, canvasW, y));
        /* bold 50 mm lines */
        p.setPen(QPen(QColor(210,200,190), 0.8/zoom));
        step = 50.0 * pxPerMM;
        for (double x = step; x < canvasW; x += step)
            p.drawLine(QLineF(x, 0, x, canvasH));
        for (double y = step; y < canvasH; y += step)
            p.drawLine(QLineF(0, y, canvasW, y));
    }

    /* ── Ruler tick marks along edges ── */
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

    /* ── Draw shapes ── */
    for (int i = 0; i < (int)objects.size(); ++i) {
        const auto &obj = objects[i];
        QColor col = COLORS[i % COLORS.size()];
        bool sel   = (i == selected);

        /* shape fill + outline */
        p.setPen(QPen(col, sel ? 3.0/zoom : 1.8/zoom));
        p.setBrush(sel
                       ? QColor(col.red(),col.green(),col.blue(), 55)
                       : QColor(col.red(),col.green(),col.blue(), 22));
        p.drawPath(obj.pathCanvas);

        /* selection ring */
        if (sel) {
            p.setPen(QPen(col, 4.0/zoom, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            QRectF br = obj.pathCanvas.boundingRect();
            br.adjust(-6/zoom, -6/zoom, 6/zoom, 6/zoom);
            p.drawRect(br);
        }

        /* dimension tag – only if enabled */
        if (showDims) drawDimensionTag(p, obj, i, sel);
    }

    p.restore();
}

/* ── dimension tag  (outside bounding box, connected with leader line) ── */
void CanvasWidget::drawDimensionTag(QPainter &p, const ShapeObject &obj,
                                    int colorIdx, bool selected)
{
    QColor col = COLORS[colorIdx % COLORS.size()];
    QRectF bb  = obj.pathCanvas.boundingRect();

    /* tag text – two lines */
    QString line1 = QString("%1").arg(obj.shape);
    QString line2 = QString("W %1  H %2 mm")
                        .arg(obj.widthMM, 0, 'f', 1)
                        .arg(obj.heightMM, 0, 'f', 1);
    QString line3 = QString("A %1 mm²").arg(obj.areaMM2, 0, 'f', 1);

    /* font – fixed logical size regardless of zoom */
    QFont f("Segoe UI", 8.5 / zoom);
    QFont fb("Segoe UI", 8.5 / zoom, QFont::Bold);
    QFontMetrics fm(f), fmb(fb);

    double pad  = 6.0  / zoom;
    double lh   = fmb.height();   // line height
    double tagW = std::max({(double)fmb.horizontalAdvance(line1),
                            (double)fm.horizontalAdvance(line2),
                            (double)fm.horizontalAdvance(line3)}) + pad*2;
    double tagH = lh * 3 + pad * 2 + lh * 0.4;   // 3 lines + padding

    /* place tag to the RIGHT of the bounding box; if too far right, go left */
    double gap   = 14.0 / zoom;
    double tagX  = bb.right() + gap;
    double tagY  = bb.center().y() - tagH * 0.5;
    if (tagX + tagW > canvasW - 10/zoom) {
        tagX = bb.left() - tagW - gap;
    }
    tagY = std::max(tagY, 4.0/zoom);
    tagY = std::min(tagY, canvasH - tagH - 4.0/zoom);

    QRectF tagRect(tagX, tagY, tagW, tagH);

    /* leader line from tag edge to shape bounding box centre-right/left */
    QPointF lineStart = (tagX > bb.right())
                            ? QPointF(tagRect.left(),  tagRect.center().y())
                            : QPointF(tagRect.right(), tagRect.center().y());
    QPointF lineEnd   = (tagX > bb.right())
                          ? QPointF(bb.right(),  bb.center().y())
                          : QPointF(bb.left(),   bb.center().y());

    /* draw leader line */
    p.setPen(QPen(col, 0.8/zoom, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawLine(lineStart, lineEnd);
    /* arrow dot at shape end */
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    double r = 3.0/zoom;
    p.drawEllipse(lineEnd, r, r);

    /* tag shadow */
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(160,140,120,30));
    p.drawRoundedRect(tagRect.adjusted(2/zoom,2/zoom,2/zoom,2/zoom), 5/zoom, 5/zoom);

    /* tag background */
    p.setBrush(QColor(255,255,255, selected ? 250 : 230));
    p.setPen(QPen(col, selected ? 1.5/zoom : 0.8/zoom));
    p.drawRoundedRect(tagRect, 5/zoom, 5/zoom);

    /* coloured left bar */
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    p.drawRoundedRect(QRectF(tagX, tagY, 4.0/zoom, tagH), 2/zoom, 2/zoom);

    /* index badge top-right of tag */
    double bR = 8.0/zoom;
    QPointF bc(tagRect.right() - bR - 1/zoom, tagRect.top() + bR + 1/zoom);
    p.setBrush(col);
    p.drawEllipse(bc, bR, bR);
    p.setPen(Qt::white);
    p.setFont(QFont("Segoe UI", 6.5/zoom, QFont::Bold));
    p.drawText(QRectF(bc.x()-bR, bc.y()-bR, bR*2, bR*2),
               Qt::AlignCenter, QString::number(obj.id));

    /* text */
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

/* ── interaction ────────────────────────────────────────────────────────── */
void CanvasWidget::wheelEvent(QWheelEvent *e)
{
    double factor = (e->angleDelta().y() > 0) ? 1.15 : 1.0/1.15;
    QPointF mp    = e->position();
    /* zoom toward cursor */
    panOffset = mp - (mp - panOffset) * factor;
    zoom     *= factor;
    clampZoom();
    update();
}

void CanvasWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton ||
        (e->button() == Qt::LeftButton && e->modifiers() & Qt::AltModifier)) {
        panning   = true;
        lastMouse = e->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() == Qt::LeftButton) {
        /* hit-test shapes in reverse order (top-most first) */
        QTransform inv = viewTransform().inverted();
        QPointF pt = inv.map(QPointF(e->position()));
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
        lastMouse  = e->position();
        update();
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *e)
{
    Q_UNUSED(e)
    panning = false;
    setCursor(Qt::ArrowCursor);
}

/* ══════════════════════════════════════════════════════════════════════════
   MainWindow
   ══════════════════════════════════════════════════════════════════════════ */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Shape Viewer  •  A4 Digitizer");
    resize(1280, 820);
    buildUI();
    applyTheme();
}

MainWindow::~MainWindow() {}

/* ── UI construction ────────────────────────────────────────────────────── */
void MainWindow::buildUI()
{
    auto mkBtn = [&](const QString &t, bool accent=false) {
        auto *b = new QPushButton(t);
        b->setFixedHeight(32);
        b->setCursor(Qt::PointingHandCursor);
        if (accent) b->setObjectName("accent");
        return b;
    };
    auto mkLabel = [&](const QString &t, bool bold=false) {
        auto *l = new QLabel(t);
        if (bold) { QFont f = l->font(); f.setBold(true); l->setFont(f); }
        return l;
    };

    /* ── Toolbar ── */
    btnOpen    = mkBtn("📂  Open JSON", true);
    btnFit     = mkBtn("⊞  Fit View");
    btnZoomIn  = mkBtn("＋ Zoom");
    btnZoomOut = mkBtn("－ Zoom");
    chkDims    = mkBtn("📏  Dimensions ✓");
    chkGrid    = mkBtn("⊞  Grid");
    chkDims->setCheckable(true); chkDims->setChecked(true);
    chkGrid->setCheckable(true); chkGrid->setChecked(false);
    chkDims->setObjectName("toggle");
    chkGrid->setObjectName("toggle");

    lblFile  = mkLabel("No file loaded");
    lblCalib = mkLabel("–");
    lblCount = mkLabel("Objects: 0");

    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);
    toolbar->addWidget(btnOpen);
    toolbar->addWidget(btnFit);
    toolbar->addWidget(btnZoomIn);
    toolbar->addWidget(btnZoomOut);
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

    /* ── Canvas ── */
    canvas = new CanvasWidget;

    /* ── Left sidebar: object list ── */
    objList = new QListWidget;
    objList->setFixedWidth(200);
    objList->setObjectName("objList");

    /* ── Right panel: dimension details ── */
    auto mkRow = [&](const QString &label, QLabel *&val) -> QWidget* {
        auto *row = new QWidget;
        auto *hl  = new QHBoxLayout(row);
        hl->setContentsMargins(0,0,0,0);
        hl->setSpacing(6);
        auto *lbl = new QLabel(label);
        lbl->setObjectName("dimLabel");
        lbl->setFixedWidth(80);
        val = new QLabel("–");
        val->setObjectName("dimValue");
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
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
    detailLayout->setContentsMargins(12, 12, 12, 12);
    detailLayout->setSpacing(4);

    auto *hdrLabel = mkLabel("Object Details", true);
    hdrLabel->setObjectName("detailHeader");
    detailLayout->addWidget(hdrLabel);
    detailLayout->addSpacing(8);

    dShape  = new QLabel("–"); dShape->setObjectName("shapeLabel");
    detailLayout->addWidget(dShape);
    detailLayout->addSpacing(6);

    /* separator */
    auto mkSep = [&](const QString &t) {
        auto *s = new QLabel(t); s->setObjectName("sectionSep");
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

    /* ── Status bar ── */
    auto *statusBar = new QHBoxLayout;
    statusBar->addWidget(lblFile);
    statusBar->addStretch();

    auto *statusFrame = new QWidget;
    statusFrame->setLayout(statusBar);
    statusFrame->setFixedHeight(28);
    statusFrame->setObjectName("statusBar");

    /* ── Main splitter ── */
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(objList);
    splitter->addWidget(canvas);
    splitter->addWidget(detailScroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({200, 860, 220});

    /* ── Root layout ── */
    auto *root = new QWidget;
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0,0,0,0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(tbFrame);
    rootLayout->addWidget(splitter, 1);
    rootLayout->addWidget(statusFrame);
    setCentralWidget(root);

    /* ── Connections ── */
    connect(btnOpen,    &QPushButton::clicked, this, &MainWindow::openFile);
    connect(btnFit,     &QPushButton::clicked, canvas, &CanvasWidget::fitToView);
    connect(btnZoomIn,  &QPushButton::clicked, canvas, &CanvasWidget::zoomIn);
    connect(btnZoomOut, &QPushButton::clicked, canvas, &CanvasWidget::zoomOut);
    connect(chkDims, &QPushButton::toggled, canvas, &CanvasWidget::setShowDimensions);
    connect(chkGrid, &QPushButton::toggled, canvas, &CanvasWidget::setShowGrid);
    connect(objList, &QListWidget::currentRowChanged, this, &MainWindow::onObjectSelected);
    connect(canvas, &CanvasWidget::objectClicked, this, &MainWindow::onCanvasClicked);

    /* Try to load default path */
    loadJson("C:/Users/shrad/Documents/digitized_20260325_095714.json");
}

/* ── Theme ─────────────────────────────────────────────────────────────── */
void MainWindow::applyTheme()
{
    qApp->setStyleSheet(R"(
        QWidget {
            background:#f5f5f2; color:#1e1e1e;
            font-family:"Segoe UI","Helvetica Neue",sans-serif;
            font-size:12px;
        }
        QWidget#toolbar { background:#ffffff; border-bottom:1px solid #e0d8d0; }
        QWidget#statusBar { background:#ffffff; border-top:1px solid #e0d8d0;
                            padding:0 12px; }
        QPushButton {
            background:#fff; color:#1e1e1e;
            border:1px solid #d8cfc8; border-radius:6px;
            padding:0 14px; font-size:12px;
        }
        QPushButton:hover   { background:#fff3ec; border-color:#eb5f14; color:#eb5f14; }
        QPushButton:pressed { background:#ffe8d8; }
        QPushButton:disabled{ color:#b8b0a8; border-color:#e8e2dc; }
        QPushButton#accent  { background:#eb5f14; border-color:#eb5f14;
                              color:#fff; font-weight:600; }
        QPushButton#accent:hover { background:#d45210; }
        QPushButton#toggle  { background:#fff; border:1px solid #d0c8c0; }
        QPushButton#toggle:checked { background:#fff3ec; border-color:#eb5f14;
                                     color:#eb5f14; font-weight:600; }
        QListWidget#objList {
            background:#fff; border:none;
            border-right:1px solid #e0d8d0;
            font-size:12px;
        }
        QListWidget#objList::item {
            padding:8px 12px;
            border-bottom:1px solid #f0e8e0;
        }
        QListWidget#objList::item:selected {
            background:#fff3ec; color:#eb5f14; font-weight:600;
        }
        QListWidget#objList::item:hover { background:#faf5f0; }
        QScrollArea#detailScroll { background:#fff; border-left:1px solid #e0d8d0; }
        QLabel#detailHeader { font-size:13px; font-weight:700; color:#1e1e1e; }
        QLabel#shapeLabel   { font-size:15px; font-weight:700; color:#eb5f14; }
        QLabel#sectionSep   {
            font-size:9px; font-weight:700; color:#9e9088;
            letter-spacing:1px;
            border-bottom:1px solid #e8e0d8;
            padding-bottom:2px; margin-top:4px;
        }
        QLabel#dimLabel { color:#7e7068; font-size:11px; }
        QLabel#dimValue { color:#1e1e1e; font-size:12px; font-weight:600; }
        QSplitter::handle { background:#e0d8d0; width:1px; }
        QLabel { color:#6e6460; }
        QScrollBar:vertical { width:6px; background:#f5f5f2; }
        QScrollBar::handle:vertical { background:#d0c8c0; border-radius:3px; }
    )");
}

/* ── File loading ───────────────────────────────────────────────────────── */
void MainWindow::openFile()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open Digitized JSON",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON files (*.json)");
    if (!path.isEmpty()) loadJson(path);
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
    pxPerMM    = root["pixelsPerMM"].toDouble(10.0);
    calibrated = root["calibrated"].toBool(false);
    filePath   = path;

    objects.clear();
    QJsonArray arr = root["objects"].toArray();

    for (auto v : arr) {
        QJsonObject obj = v.toObject();
        ShapeObject so;
        so.id    = obj["id"].toInt();
        so.shape = obj["shape"].toString();

        /* ── read dimensions block (v3 format) or flat fields (v2) ── */
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
            /* backwards-compatible with v2 flat format */
            so.widthMM    = obj["width_mm"] .toDouble();
            so.heightMM   = obj["height_mm"].toDouble();
            so.perimMM    = obj["perim_mm"] .toDouble();
            so.areaMM2    = obj["area_mm2"] .toDouble();
            so.angleDeg   = obj["angle_deg"].toDouble();
            so.diagonalMM = std::sqrt(so.widthMM*so.widthMM + so.heightMM*so.heightMM);
            so.aspectRatio= so.heightMM > 0 ? so.widthMM/so.heightMM : 0;
        }

        /* centre */
        QString centreKey = obj.contains("center_on_sheet") ?
                                "center_on_sheet" : "center";
        QJsonObject cen = obj[centreKey].toObject();
        so.centerXmm = cen["x_mm"].toDouble();
        so.centerYmm = cen["y_mm"].toDouble();

        /* ── reconstruct QPainterPath in canvas pixels ── */
        QJsonArray pa = obj["path"].toArray();
        QPainterPath path;
        for (auto cv : pa) {
            QJsonObject cmd = cv.toObject();
            QString t = cmd["cmd"].toString();
            if (t == "M")
                path.moveTo(cmd["x"].toDouble()*pxPerMM,
                            cmd["y"].toDouble()*pxPerMM);
            else if (t == "L")
                path.lineTo(cmd["x"].toDouble()*pxPerMM,
                            cmd["y"].toDouble()*pxPerMM);
            else if (t == "C")
                path.cubicTo(cmd["x1"].toDouble()*pxPerMM,
                             cmd["y1"].toDouble()*pxPerMM,
                             cmd["x2"].toDouble()*pxPerMM,
                             cmd["y2"].toDouble()*pxPerMM,
                             cmd["x"].toDouble() *pxPerMM,
                             cmd["y"].toDouble() *pxPerMM);
            else if (t == "Z")
                path.closeSubpath();
        }
        so.pathCanvas = path;
        objects.push_back(so);
    }

    /* update canvas */
    canvas->setObjects(objects, pxPerMM, calibrated);

    /* update UI */
    populateSidebar();
    updateStatusBar();
    showObjectDetails(-1);
}

void MainWindow::populateSidebar()
{
    objList->clear();
    for (int i = 0; i < (int)objects.size(); ++i) {
        const auto &o = objects[i];
        QColor col = CanvasWidget::COLORS[i % CanvasWidget::COLORS.size()];

        /* coloured dot + name + size */
        auto *item = new QListWidgetItem;
        item->setText(QString("  #%1  %2\n  %3 × %4 mm")
                          .arg(o.id)
                          .arg(o.shape)
                          .arg(o.widthMM, 0, 'f', 1)
                          .arg(o.heightMM, 0, 'f', 1));

        /* colour the icon */
        QPixmap px(12, 12);
        px.fill(Qt::transparent);
        QPainter pp(&px);
        pp.setRenderHint(QPainter::Antialiasing);
        pp.setBrush(col); pp.setPen(Qt::NoPen);
        pp.drawEllipse(px.rect());
        item->setIcon(QIcon(px));
        item->setSizeHint(QSize(-1, 52));
        objList->addItem(item);
    }
}

void MainWindow::updateStatusBar()
{
    QFileInfo fi(filePath);
    lblFile->setText(fi.fileName().isEmpty() ? "No file" : fi.fileName());
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
    dShape->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;")
                              .arg(col.name()));
    dWidth ->setText(QString("%1 mm").arg(o.widthMM,  0,'f',2));
    dHeight->setText(QString("%1 mm").arg(o.heightMM, 0,'f',2));
    dDiag  ->setText(QString("%1 mm").arg(o.diagonalMM,0,'f',2));
    dArea  ->setText(QString("%1 mm²").arg(o.areaMM2, 0,'f',1));
    dPerim ->setText(QString("%1 mm").arg(o.perimMM,  0,'f',1));
    dAspect->setText(QString("%1 : 1").arg(o.aspectRatio,0,'f',2));
    dAngle ->setText(QString("%1°").arg(o.angleDeg,   0,'f',1));
    dCenter->setText(QString("(%1, %2) mm")
                         .arg(o.centerXmm,0,'f',1)
                         .arg(o.centerYmm,0,'f',1));
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
