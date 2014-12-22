#include <QtWidgets/QApplication>
#include "qt_gui/MainWindow.h"

namespace med {

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  qt_gui::MainWindow foo;
  foo.show();
  return app.exec();
}
  
}  // namespace med

int main(int argc, char* argv[]) {
  med::main(argc, argv);
}
