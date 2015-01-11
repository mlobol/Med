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
    buffers.Open(QFileDialog::getOpenFileName().toStdString());
  });
  
  QObject::connect(&buffers, &Editor::Buffers::newBuffer, this, &MainWindow::handleNewBuffer);
}

MainWindow::~MainWindow() {}

void MainWindow::handleNewBuffer(Editor::Buffer* buffer) {
  bufferWidgets.push_back(new Buffer(buffer));
  tabWidget.addTab(bufferWidgets.back(), "");
}

}  // namespace QtGui
}  // namespace Med


#include "MainWindow.moc"
