#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QUndoStack>
#include <vector>
#include "canvaswidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *e) override;

private slots:
    void openFile();
    void saveFile();
    void saveFileAs();
    void deleteSelected();
    void duplicateSelected();
    void zoomToSelected();
    void onObjectSelected(int row);
    void onCanvasClicked(int index);
    void onObjectMoved(int idx);

    void packShapes();
    void updateUndoRedoButtons();
    void showCanvasContextMenu(const QPoint &pos);
    void showListContextMenu(const QPoint &pos);
    void nudgeSelected(int dx_mm, int dy_mm);


private:
    QPushButton *btnPack;
    // In private slots:
    void buildUI();
    void applyTheme();
    void loadJson(const QString &path);
    void saveToFile(const QString &path);
    void populateSidebar();
    void updateStatusBar();
    void showObjectDetails(int idx);
    void markDirty(bool dirty = true);
    void createUndoCommandsForMove(int idx, const ShapeObject &oldObj, const ShapeObject &newObj);
    int getNextId() const;

    // UI elements
    QPushButton *btnOpen, *btnSave, *btnSaveAs, *btnDelete, *btnDuplicate;
    QPushButton *btnFit, *btnZoomIn, *btnZoomOut, *btnUndo, *btnRedo;
    QPushButton *chkDims, *chkGrid;
    QLabel *lblFile, *lblCalib, *lblCount;
    QListWidget *objList;
    CanvasWidget *canvas;
    QLabel *dShape, *dWidth, *dHeight, *dDiag, *dArea, *dPerim, *dAspect, *dAngle, *dCenter;

    // Data
    std::vector<ShapeObject> objects;
    double pxPerMM;
    bool calibrated;
    QString filePath;
    bool dirtyFlag;

    QUndoStack undoStack;
};

#endif // MAINWINDOW_H
