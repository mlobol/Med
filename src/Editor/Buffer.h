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
  class SafePoint;
  class TempPoint;

  static std::unique_ptr<Buffer> New();
  static std::unique_ptr<Buffer> Open(const std::string& filePath);

  ~Buffer();

  const QString& name() { return name_; }

  int lineCount() const { return tree_.totalDelta(); }

private:
  friend class BufferTest;
  struct Line {
    std::vector<SafePoint*> points;
    QString content;
  };
  typedef Util::DRBTree<int, int, Line> Tree;

  Buffer();

  void InitFromStream(QTextStream* stream, const QString& name);

  Tree::Iterator line(int lineNumber);
  Tree::Iterator insertLine(int lineNumber);

  Tree tree_;
  QString name_;
};

class Buffer::Point {
public:
  typedef Util::IteratorHelper<const QString*> LineIterator;

  class LinesForwardsIterable {
  public:
    LineIterator begin();
    LineIterator end() { return {nullptr}; }

  private:
    friend Point;

    LinesForwardsIterable(Point* from) : from_(from) {}

    Point* from_;
  };

  ~Point();

  Buffer* buffer() const { return buffer_; }
  bool isValid() const { return bufferLine_; }
  const QString& lineContent() const { return line()->content; }

  void reset() { setLine(nullptr); }
  void moveTo(const Point& point);
  bool setColumnNumber(int columnNumber);
  void setLineNumber(int lineNumber);

  int columnNumber() const { return columnNumber_; }
  int lineNumber() const {
    // TODO: cache line number and only compute it if the buffer changed since last time.
    return bufferLine_->key(Util::DRBTreeDefs::Side::LEFT);
  }

  bool moveToLineStart();
  bool moveToLineEnd();
  bool moveUp();
  bool moveDown();
  bool moveLeft();
  bool moveRight();

  bool insertBefore(const QString& text);
  bool insertLineBreakBefore();

  bool deleteCharBefore();
  bool deleteCharAfter();
  bool deleteTo(Point* point);

  static void sortPair(Point* left, Point* right, Point** first, Point** second);

  LinesForwardsIterable linesForwards() { return LinesForwardsIterable(this); }

private:
  class LineIteratorImpl;

  friend class SafePoint;
  friend class TempPoint;

  Point(bool safe, Buffer* buffer);

  Point(const Point&) = delete;
  Point& operator=(const Point&) = delete;

  Line* line() const { return &bufferLine_->value; }

  void setLine(Tree::Node* newLine);
  void detachFromLine();
  void attachToLine();

  const bool safe_;
  Buffer* const buffer_ = nullptr;
  Tree::Node* bufferLine_ = nullptr;
  int columnNumber_ = 0;
  int indexInLinePoints_ = -1;
};

class Buffer::SafePoint : public Point {
public:
  SafePoint(Buffer* buffer) : Point(true, buffer) {}
};

class Buffer::TempPoint : public Point {
public:
  TempPoint(Point& other) : Point(false, other.buffer()) { moveTo(other); }
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_BUFFER_H
