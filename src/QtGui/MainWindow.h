#ifndef MED_QTGUI_MAINWINDOW_H
#define MED_QTGUI_MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTabWidget>

#include "Buffer.h"
#include "Editor/Buffers.h"

namespace Med {
namespace QtGui {

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow();
  virtual ~MainWindow();
  
private slots:
  void handleNewBuffer(Editor::Buffer* buffer);
  
private:
  Editor::Buffers buffers;
  
  std::list<Buffer> bufferWidgets;
  QTabWidget tabWidget;
};

}  // namespace QtGui
}  // namespace Med

#endif // MED_QTGUI_MAINWINDOW_H

