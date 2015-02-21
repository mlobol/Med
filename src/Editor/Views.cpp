#include "Views.h"

namespace Med {
namespace Editor {
  
Views::Views() {}
Views::~Views() {}

View* Views::newView(Buffer* buffer) {
  views_.emplace_back(new View(buffer));
  return views_.back().get();
}

}  // namespace Editor
}  // namespace Med

