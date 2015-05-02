#include "View.h"

namespace Med {
namespace Editor {

View::View(Buffer* buffer) : buffer_(buffer), insertionPoint_(buffer) {}

}  // namespace Editor
}  // namespace Med