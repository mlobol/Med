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
  class Point;

  struct Line {
    int lineNumber;
    QString* content;
  };

  typedef Util::IteratorHelper<Line> Iterator;

  class IterableFromLineNumber {
  public:
    IterableFromLineNumber(Buffer* buffer, int lineNumber) : buffer_(buffer), lineNumber_(lineNumber) {}
    
    Iterator begin();
    Iterator end() { return {}; }

  private:
    Buffer* buffer_;
    int lineNumber_;
  };
  
  static std::unique_ptr<Buffer> Open(const std::string& filePath);
  
  ~Buffer();
  
  const QString& name() { return name_; }
  
  int lineCount();
  IterableFromLineNumber iterateFromLineNumber(int lineNumber);
  
  int64_t contentVersion() const { return contentVersion_; }
  
private:
  friend class BufferTest;
  
  class Lines;
  
  Buffer();
  
  void InitFromStream(QTextStream* stream, const QString& name);
  
  std::unique_ptr<Lines> lines_;
  int64_t contentVersion_ = 0;
  QString name_;
};

class Buffer::Point {
public:
  Point(Buffer* buffer) : buffer_(buffer) {}
  
  void setColumnNumber(int columnNumber) { columnNumber_ = columnNumber; }
  void setLineNumber(int lineNumber) {
    lineNumber_ = lineNumber;
    invalidateCache();
  }

  int columnNumber() const { return columnNumber_; }
  int lineNumber() const { return lineNumber_; }
  QString* line();

  bool insertBefore(const QString& text);
  bool insertLineBreakBefore();
 
private:
  void invalidateCache() {
    cachedLine_ = nullptr;
    contentVersion_ = -1;
  }
  
  Buffer* const buffer_ = nullptr;
  int columnNumber_ = 0;
  int lineNumber_ = 0;
  QString* cachedLine_ = nullptr;
  int64_t contentVersion_ = -1;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_BUFFER_H
