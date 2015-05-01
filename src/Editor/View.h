#ifndef MED_EDITOR_VIEW_H
#define MED_EDITOR_VIEW_H

#include <list>
#include <memory>
#include <string>

#include <QtCore/QString>
#include <QtCore/QTextStream>

#include "Buffer.h"

namespace Med {
namespace Editor {
  
class View {
public:
  View(Buffer* buffer);
  
  Buffer* buffer() { return buffer_; }
  int pageTopLineNumber() { return pageTopLineNumber_; }
  void setPageTopLineNumber(int pageTopLineNumber) { pageTopLineNumber_ = pageTopLineNumber; }
  
  Buffer::CachedPoint* insertionPoint() { return &insertionPoint_; }
  
  bool insert(const QString& text);
  
private:
  Buffer* buffer_;
  int pageTopLineNumber_ = 1;
  Buffer::CachedPoint insertionPoint_;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_VIEW_H
