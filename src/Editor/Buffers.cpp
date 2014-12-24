#include "Buffers.h"

namespace med {
namespace editor {
  
Buffers::Buffers() {}
Buffers::~Buffers() {}

void Buffers::Open(const std::string& filePath) {
  openBuffers.push_back(Buffer::Open(filePath));
  emit newBuffer(openBuffers.back().get());
}

}  // namespace editor
}  // namespace med

