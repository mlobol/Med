#include "MainWindow.h"

#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QAction>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

namespace Med {
namespace QtGui {

class ScrollArea : public QAbstractScrollArea {
  using QAbstractScrollArea::QAbstractScrollArea;
};

MainWindow::MainWindow() {
  ScrollArea* scrollArea = new ScrollArea(this);
  setCentralWidget(scrollArea);

  const auto addAction = [this](const char* text, QKeySequence shortcut, QMenu* menu, std::function<void()> callOnTrigger) {
    QAction* action = new QAction(this);
    action->setText(text);
    action->setShortcut(shortcut);
    QObject::connect(action, &QAction::triggered, this, callOnTrigger);
    if (menu != nullptr) menu->addAction(action);
    return action;
  };
  QMenu* fileMenu = menuBar()->addMenu("File");
  addAction("Quit", QKeySequence::Quit, fileMenu, [this]() { close(); });
  addAction("Open", QKeySequence::Open, fileMenu, [this]() {
    buffers.Open(QFileDialog::getOpenFileName().toStdString());
  });
  
  connect(&buffers, &Editor::Buffers::newBuffer, this, &MainWindow::handleNewBuffer);
}

MainWindow::~MainWindow() {}

void MainWindow::handleNewBuffer(Editor::Buffer* buffer) {
  
}

}  // namespace QtGui
}  // namespace Med


#include "MainWindow.moc"
