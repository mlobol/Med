#include "Buffers.h"

namespace Med {
namespace Editor {

Buffers::Buffers() {}
Buffers::~Buffers() {}

Buffer* Buffers::create() {
  buffers_.push_back(Buffer::create());
  return buffers_.back().get();
}

Buffer* Buffers::openFile(const std::string& filePath) {
  buffers_.push_back(Buffer::open(filePath));
  return buffers_.back().get();
}

}  // namespace Editor
}  // namespace Med

