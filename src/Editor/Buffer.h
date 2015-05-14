#ifndef MED_EDITOR_BUFFER_H
#define MED_EDITOR_BUFFER_H

#include <list>
#include <memory>
#include <string>

#include <QtCore/QString>
#include <QtCore/QTextStream>

#include "Util/DRBTree.h"
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
  
  int lineCount() const { return tree_.totalDelta(); }

  IterableFromLineNumber iterateFromLineNumber(int lineNumber) { return {this, lineNumber}; }
  
private:
  friend class BufferTest;
  class LineIterator;
  typedef Util::DRBTree<int, int, QString> Tree;

  Buffer();
  
  void InitFromStream(QTextStream* stream, const QString& name);

  Tree::Iterator line(int lineNumber);
  Tree::Iterator insertLine(int lineNumber);

  Tree tree_;
  QString name_;
};

class Buffer::Point {
public:
  Point(Buffer* buffer) : buffer_(buffer) {}
  
  bool setColumnNumber(int columnNumber);
  void setLineNumber(int lineNumber);

  int columnNumber() const { return columnNumber_; }
  int lineNumber() const { return line_->key; }

  bool moveToLineStart();
  bool moveToLineEnd();
  bool moveUp();
  bool moveDown();
  bool moveLeft();
  bool moveRight();
 
  bool insertBefore(const QString& text);
  bool insertLineBreakBefore();
  
  bool deleteCharBefore() { return moveLeft() && deleteCharAfter(); }
  bool deleteCharAfter();
  
private:
  QString* lineContent() const { return &line_->node->value; }
  
  Buffer* const buffer_ = nullptr;
  Tree::Iterator line_;
  int columnNumber_ = 0;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_BUFFER_H
