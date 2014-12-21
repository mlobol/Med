#ifndef MED_EDITOR_BUFFERS_H
#define MED_EDITOR_BUFFERS_H

#include <list>

#include "buffer.h"

namespace med {
namespace editor {

class Buffers {
public:
  Buffers();
  virtual ~Buffers();
  
private:
  std::list<Buffer> open_buffers;
};

}  // namespace editor
}  // namespace med

#endif // MED_EDITOR_BUFFERS_H

