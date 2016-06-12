#ifndef MED_QTGUI_VIEW_H
#define MED_QTGUI_VIEW_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QTabWidget>

#include "Editor/View.h"

namespace Med {
namespace QtGui {

class View : public QWidget {
  Q_OBJECT

public:
  View(Editor::View* view, QTabWidget* tabWidget);
  virtual ~View();

  void copyToClipboard();
  void pasteFromClipboard();
  void undo();
  void redo();
  bool save();

  void updateLabel();

private:
  class ScrollArea;
  class Lines;
  friend ScrollArea;
  friend Lines;

  Editor::View* const view_;
  QTabWidget* const tabWidget_;
  ScrollArea* scrollArea_;
  Lines* lines_;
};

}  // namespace QtGui
}  // namespace Med

#endif // MED_QTGUI_VIEW_H

