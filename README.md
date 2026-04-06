# 🧾 Path Viewer – A4 Object Digitizer & Layout Editor

A powerful desktop application built with **Qt (C++)** for visualizing, analyzing, and editing object layouts extracted from an A4 sheet.
It transforms detected shapes into an interactive, measurable, and editable workspace — similar to a lightweight CAD/layout tool.

---

## 🚀 Features

### 📐 Accurate Measurement System

* Real-world dimensions (mm-based)
* Area, perimeter, diagonal, aspect ratio
* Pixel-to-mm calibrated mapping

### 🖱 Interactive Canvas

* Smooth zoom & pan
* Grid system (10mm / 50mm)
* Ruler guides on A4 sheet
* Clean visual rendering with anti-aliasing

### 🎯 Object Interaction

* Select objects from canvas or sidebar
* Highlight + bounding visualization
* Detailed measurement panel

### ✏️ Editing Capabilities

* 🔥 Drag & move objects
* 🗑 Delete unwanted objects
* 📏 Snap-to-grid alignment
* Real-time updates

### 📊 Smart Visualization

* Colored object differentiation
* Dimension tags with leader lines
* Clean UI inspired by modern design systems

### 📂 JSON-Based Workflow

* Load digitized object data
* Supports structured formats (v2 & v3)
* Reconstructs shapes using vector paths

---

## 🖼️ Preview

> Add screenshots here (recommended)

* Canvas view with grid
* Object measurement panel
* Interactive dragging

---

## 🏗️ Tech Stack

* **Language:** C++17
* **Framework:** Qt (Widgets, GUI, JSON)
* **Graphics:** QPainter / QPainterPath
* **Architecture:** Modular UI + Canvas Rendering Engine

---

## 📁 Project Structure

```
├── mainwindow.cpp        # UI & application logic
├── canvaswidget.cpp      # Rendering + interaction engine
├── mainwindow.h
├── ...
```

---

## ⚙️ How It Works

1. Load JSON file containing detected objects
2. Convert paths → QPainterPath
3. Map coordinates using px/mm calibration
4. Render objects on A4 canvas
5. Enable interaction (select, move, delete)

---

## 🧠 Use Cases

* 📦 Packaging layout planning
* 🏭 Manufacturing measurements
* 📐 Shape analysis & visualization
* 🧾 Digitized document processing
* 🎓 Computer Vision output visualization

---

## 🛠️ Build Instructions

### Requirements

* Qt 6.x (MinGW / MSVC)
* C++17 enabled

### Steps

```bash
# Open project in Qt Creator
# Select Release or Debug
# Build & Run
```

---

## 📤 Deployment

For Windows:

```bash
windeployqt PathViewer.exe
```

This bundles required Qt DLLs automatically.

---

## 🔥 Future Improvements

* ✨ Resize handles (like Figma)
* 🔄 Undo / Redo system
* 🧠 Multi-object selection
* 📐 Alignment tools (left, center, distribute)
* 📤 Export edited layout
* 🤖 Integration with live detection system

---

## 👨‍💻 Author

**Mohan Chilivery**
Software Engineer | Full Stack Developer | AIML Student

---

## ⭐ Show Your Support

If you like this project:

* ⭐ Star the repo
* 🍴 Fork it
* 💬 Share feedback

---

## 📜 License

This project is open-source and available under the MIT License.
