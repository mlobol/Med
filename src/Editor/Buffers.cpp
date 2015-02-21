#include "Buffers.h"

namespace Med {
namespace Editor {
  
Buffers::Buffers() {}
Buffers::~Buffers() {}

Buffer* Buffers::Open(const std::string& filePath) {
  buffers_.emplace_back(Buffer::Open(filePath));
  return buffers_.back().get();
}

}  // namespace Editor
}  // namespace Med

