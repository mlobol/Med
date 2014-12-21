#include "MainWindow.h"

#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QAction>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

namespace med {
namespace qt_gui {

class ScrollArea : public QAbstractScrollArea {
  using QAbstractScrollArea::QAbstractScrollArea;
};

MainWindow::MainWindow() {
  ScrollArea* scroll_area = new ScrollArea(this);
  setCentralWidget(scroll_area);

  const auto add_action = [this](const char* text, QKeySequence shortcut, QMenu* menu, std::function<void()> call_on_trigger) {
    QAction* action = new QAction(this);
    action->setText(text);
    action->setShortcut(shortcut);
    QObject::connect(action, &QAction::triggered, call_on_trigger);
    if (menu != nullptr) menu->addAction(action);
    return action;
  };
  QMenu* file_menu = menuBar()->addMenu("File");
  add_action("Quit", QKeySequence::Quit, file_menu, [this]() { close(); });
  add_action("Open", QKeySequence::Open, file_menu, [this]() {
    QString file_name = QFileDialog::getOpenFileName();
  });
}

MainWindow::~MainWindow() {}

}  // namespace qt_gui
}  // namaspace med


#include "MainWindow.moc"
