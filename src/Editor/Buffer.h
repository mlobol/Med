#ifndef MED_EDITOR_BUFFER_H
#define MED_EDITOR_BUFFER_H

#include <list>
#include <memory>
#include <string>

namespace med {
namespace editor {
  
class IOException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

class Buffer {
public:
  virtual ~Buffer();
  static std::unique_ptr<Buffer> Open(const std::string& filePath);
  
private:
  class Lines;
  
  Buffer();
  
  std::unique_ptr<Lines> lines;
};

}  // namespace editor
}  // namespace med

#endif // MED_EDITOR_BUFFER_H

