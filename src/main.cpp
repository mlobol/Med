#include <QtWidgets/QApplication>
#include "QtGui/MainWindow.h"

namespace Med {

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QtGui::MainWindow foo;
  foo.show();
  return app.exec();
}
  
}  // namespace Med

int main(int argc, char* argv[]) {
  Med::main(argc, argv);
}

