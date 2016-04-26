#include "View.h"

namespace Med {
namespace Editor {

View::View(Buffer* buffer) : buffer_(buffer), insertionPoint_(buffer), selectionPoint_(buffer), pageTop_(buffer), undo_(buffer) {
  pageTop_.setLineNumber(1);
}

}  // namespace Editor
}  // namespace Med