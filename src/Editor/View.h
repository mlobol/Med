#ifndef MED_EDITOR_VIEW_H
#define MED_EDITOR_VIEW_H

#include "Buffer.h"
#include "Undo.h"

namespace Med {
namespace Editor {

class View {
public:
  SafePoint insertionPoint_;
  SafePoint selectionPoint_;
  SafePoint pageTop_;
  Undo undo_;

  View(Buffer* buffer);

  Buffer* buffer() { return buffer_; }

private:
  Buffer* buffer_;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_VIEW_H
