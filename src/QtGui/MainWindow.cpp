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
  const auto addNewActionWithView = [this, &addNewAction](const char* text, QKeySequence shortcut, QMenu* menu, std::function<void(View* currentView)> callOnTrigger) {
    return addNewAction(text, shortcut, menu, [this, callOnTrigger]() {
      View* currentView = qobject_cast<View*>(tabWidget.currentWidget());
      if (currentView) callOnTrigger(currentView);
    });
  };
  QMenu* fileMenu = menuBar()->addMenu("File");
  addNewAction("New", QKeySequence::New, fileMenu, [this]() {
    OpenBuffer(buffers_.create());
  });
  addNewAction("Open...", QKeySequence::Open, fileMenu, [this]() {
    OpenFile(QFileDialog::getOpenFileName().toStdString());
  });
  addNewActionWithView("Save", QKeySequence::Save, fileMenu, [this](View* currentView) {
    currentView->save();
  });
  addNewAction("Close", QKeySequence::Close, fileMenu, [this]() {
    delete tabWidget.currentWidget();
  });
  addNewAction("Quit", QKeySequence::Quit, fileMenu, [this]() { close(); });
  QMenu* editMenu = menuBar()->addMenu("Edit");
  addNewActionWithView("Undo", QKeySequence::Undo, editMenu, [this](View* currentView) {
    currentView->undo();
  });
  addNewActionWithView("Redo", QKeySequence::Redo, editMenu, [this](View* currentView) {
    currentView->redo();
  });
  addNewActionWithView("Copy", QKeySequence::Copy, editMenu, [this](View* currentView) {
    currentView->copyToClipboard();
  });
  addNewActionWithView("Paste", QKeySequence::Paste, editMenu, [this](View* currentView) {
    currentView->pasteFromClipboard();
  });
}

MainWindow::~MainWindow() {}

void MainWindow::OpenBuffer(Editor::Buffer* buffer) {
  Editor::View* view = views_.newView(buffer);
  viewWidgets.push_back(new View(view, &tabWidget));
  tabWidget.setCurrentIndex(tabWidget.addTab(viewWidgets.back(), ""));
  viewWidgets.back()->updateLabel();
}

void MainWindow::OpenFile(const std::string& path) {
  OpenBuffer(buffers_.openFile(path));
}

}  // namespace QtGui
}  // namespace Med


#include "MainWindow.moc"
