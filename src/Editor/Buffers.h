#ifndef MED_EDITOR_BUFFERS_H
#define MED_EDITOR_BUFFERS_H

#include <list>
#include <string>
#include <QObject>

#include "Buffer.h"

namespace Med {
namespace Editor {

class Buffers {
public:
  Buffers();
  virtual ~Buffers();
  
  Buffer* Open(const std::string& filePath);
  
private:
  std::list<std::unique_ptr<Buffer>> buffers_;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_BUFFERS_H

