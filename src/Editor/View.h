#ifndef MED_EDITOR_VIEW_H
#define MED_EDITOR_VIEW_H

#include "Buffer.h"

namespace Med {
namespace Editor {

class View {
public:
  Buffer::Point insertionPoint_;
  Buffer::Point selectionPoint_;
  Buffer::Point pageTop_;

  View(Buffer* buffer);

  Buffer* buffer() { return buffer_; }

private:
  Buffer* buffer_;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_VIEW_H
