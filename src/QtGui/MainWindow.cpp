#include "MainWindow.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

namespace Med {
namespace QtGui {

MainWindow::MainWindow() : tabWidget(this) {
  setCentralWidget(&tabWidget);
  
  const auto addNewAction = [this](const char* text, QKeySequence shortcut, QMenu* menu, std::function<void()> callOnTrigger) {
    QAction* action = new QAction(this);
    action->setText(text);
    action->setShortcut(shortcut);
    QObject::connect(action, &QAction::triggered, this, callOnTrigger);
    addAction(action);
    if (menu != nullptr) menu->addAction(action);
    return action;
  };
  QMenu* fileMenu = menuBar()->addMenu("File");
  addNewAction("Quit", QKeySequence::Quit, fileMenu, [this]() { close(); });
  addNewAction("Open", QKeySequence::Open, fileMenu, [this]() {
    Open(QFileDialog::getOpenFileName().toStdString());
  });
}

MainWindow::~MainWindow() {}

void MainWindow::Open(const std::string& path) {
  Editor::View* view = views_.newView(buffers_.Open(path));
  viewWidgets.push_back(new View(view));
  tabWidget.addTab(viewWidgets.back(), view->buffer()->name());
}

}  // namespace QtGui
}  // namespace Med


#include "MainWindow.moc"
