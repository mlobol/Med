#ifndef MED_EDITOR_BUFFER_H
#define MED_EDITOR_BUFFER_H

#include <list>
#include <memory>
#include <string>

#include <QtCore/QString>
#include <QtCore/QTextStream>

#include "Util/IteratorHelper.h"

namespace Med {
namespace Editor {
  
class IOException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

class Buffer {
public:
  virtual ~Buffer();
  static std::unique_ptr<Buffer> Open(const std::string& filePath);
  
  struct Line {
    int lineNumber;
    QString* content;
  };
  
  typedef Util::IteratorHelper<Line> Iterator;
  
  class IterableFromLineNumber {
  public:
    IterableFromLineNumber(Buffer* buffer, int lineNumber) : buffer(buffer), lineNumber(lineNumber) {}
    
    Iterator begin();
    Iterator end() { return {}; }
    
    Buffer* buffer;
    int lineNumber;
  };
  
  IterableFromLineNumber iterateFromLineNumber(int lineNumber);
  
private:
  friend class BufferTest;
  
  class Lines;
  
  Buffer();
  
  void InitFromStream(QTextStream* stream);
  
  std::unique_ptr<Lines> lines;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_BUFFER_H

