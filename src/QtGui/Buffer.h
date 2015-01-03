#ifndef MED_QTGUI_BUFFER_H
#define MED_QTGUI_BUFFER_H

#include <QtWidgets/QMainWindow>

#include "Editor/Buffer.h"

namespace Med {
namespace QtGui {

class Buffer : public QWidget {
public:
  Buffer(Editor::Buffer* buffer);
  virtual ~Buffer();
  
private:
  class ScrollArea;
  class Lines;
  friend ScrollArea;
  friend Lines;
  
  Editor::Buffer* buffer;
  ScrollArea* scrollArea;
  Lines* lines;
};

}  // namespace QtGui
}  // namespace Med

#endif // MED_QTGUI_BUFFER_H

