#include "Buffers.h"

namespace Med {
namespace Editor {
  
Buffers::Buffers() {}
Buffers::~Buffers() {}

void Buffers::Open(const std::string& filePath) {
  openBuffers.push_back(Buffer::Open(filePath));
  emit newBuffer(openBuffers.back().get());
}

}  // namespace Editor
}  // namespace Med

