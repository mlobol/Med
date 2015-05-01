#include "View.h"

namespace Med {
namespace Editor {

View::View(Buffer* buffer) : buffer_(buffer), insertionPoint_(buffer) {}

bool View::insert(const QString& text) {
  bool inserted = buffer_->insert(&insertionPoint_, text);
  if (!inserted) return false;
  insertionPoint_.setColumnNumber(insertionPoint_.point().columnNumber + text.size());
  return true;
}

}  // namespace Editor
}  // namespace Med