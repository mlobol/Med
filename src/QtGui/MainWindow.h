#ifndef MED_QTGUI_MAINWINDOW_H
#define MED_QTGUI_MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTabWidget>
#include <string>

#include "View.h"
#include "Editor/Buffers.h"
#include "Editor/Views.h"

namespace Med {
namespace QtGui {

class MainWindow : public QMainWindow {
public:
  MainWindow();
  virtual ~MainWindow();
  
  void Open(const std::string& path);
  
private:
  Editor::Buffers buffers_;
  Editor::Views views_;
  
  std::list<View*> viewWidgets;
  QTabWidget tabWidget;
};

}  // namespace QtGui
}  // namespace Med

#endif // MED_QTGUI_MAINWINDOW_H

