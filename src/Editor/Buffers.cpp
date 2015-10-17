#include "Buffers.h"

namespace Med {
namespace Editor {

Buffers::Buffers() {}
Buffers::~Buffers() {}

Buffer* Buffers::New() {
  buffers_.push_back(Buffer::New());
  return buffers_.back().get();
}

Buffer* Buffers::OpenFile(const std::string& filePath) {
  buffers_.push_back(Buffer::Open(filePath));
  return buffers_.back().get();
}

}  // namespace Editor
}  // namespace Med

