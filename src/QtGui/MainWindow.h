#ifndef MED_QT_GUI_MAINWINDOW_H
#define MED_QT_GUI_MAINWINDOW_H

#include <QtWidgets/QMainWindow>

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
};

}  // namespace QtGui
}  // namespace Med

#endif // MED_QT_GUI_MAINWINDOW_H

