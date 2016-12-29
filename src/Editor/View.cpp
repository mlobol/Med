#include "View.h"

namespace Med {
namespace Editor {

View::View(Buffer* buffer) : buffer_(buffer), insertionPoint_(SafePoint::Interactive(), buffer), selectionPoint_(SafePoint::Interactive(), buffer), pageTop_(SafePoint::Interactive(), buffer), undo_(buffer) {
  pageTop_.setLineNumber(1);
  undo_.setUnmodified();
}

}  // namespace Editor
}  // namespace Med