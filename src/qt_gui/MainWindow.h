#ifndef MED_QT_GUI_MAINWINDOW_H
#define MED_QT_GUI_MAINWINDOW_H

#include <QtWidgets/QMainWindow>

#include "editor/buffers.h"

namespace med {
namespace qt_gui {

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow();
  virtual ~MainWindow();
  
protected:
  editor::Buffers buffers;
};

}  // namespace qt_gui
}  // namespace med

#endif // MED_QT_GUI_MAINWINDOW_H

