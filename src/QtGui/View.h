#ifndef MED_QTGUI_VIEW_H
#define MED_QTGUI_VIEW_H

#include <QtWidgets/QWidget>

#include "Editor/View.h"

namespace Med {
namespace QtGui {

class View : public QWidget {
public:
  View(Editor::View* view);
  virtual ~View();
  
private:
  class ScrollArea;
  class Lines;
  friend ScrollArea;
  friend Lines;
  
  Editor::View* view_;
  ScrollArea* scrollArea_;
  Lines* lines_;
};

}  // namespace QtGui
}  // namespace Med

#endif // MED_QTGUI_VIEW_H
