#ifndef MED_EDITOR_BUFFER_H
#define MED_EDITOR_BUFFER_H

#include <list>
#include <memory>
#include <string>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QTextStream>

#include "Undo.h"
#include "Util/DRBTree.h"
#include "Util/IteratorHelper.h"

namespace Med {
namespace Editor {

class IOException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

class SafePoint;

class Buffer {
public:
  static std::unique_ptr<Buffer> create();
  static std::unique_ptr<Buffer> open(const std::string& filePath);

  ~Buffer();

  const QString& name() { return name_; }
  const std::string& filePath() { return filePath_; }
  bool save();
  bool modified() { return modified_; }

  int lineCount() const {
    // If there are no lines, totalDelta() returns 0; otherwise, it returns first line number + line count. The first line number is 1, so we subtract that.
    return qMax(0, tree_.totalDelta() - 1);
  }

private:
  friend class BufferTest;
  friend class Point;

  struct Line {
    std::vector<SafePoint*> points;
    QString content;
  };
  typedef Util::DRBTree<int, int, Line> Tree;

  Buffer();

  void initFromStream(QTextStream* stream, const QString& name);

  Tree::Iterator line(int lineNumber);
  // TODO: better implementation for lastLine(), using extreme().
  Tree::Iterator lastLine() { return line(lineCount()); }

  Tree::Iterator insertLine(int lineNumber);
  // TODO: better implementation for insertLast().
  Tree::Iterator insertLast() { return insertLine(lineCount() + 1); }

  Tree tree_;
  QString name_;
  std::string filePath_;
  bool modified_ = false;
};

class Point {
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

  void reset() { setLine(nullptr); }
  void moveTo(const Point& point);
  bool setColumnNumber(int columnNumber);
  void setLineNumber(int lineNumber);

  bool sameLineAs(const Point& point) const { return bufferLine_ == point.bufferLine_; }
  int columnNumber() const { return columnNumber_; }
  bool samePositionAs(const Point& point) const {
    return sameLineAs(point) && columnNumber() == point.columnNumber();
  }
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

  const QString& lineContent() const { return line()->content; }
  bool contentTo(const Point& other, QString* output) const;

  // Inserts the text in the current line; no line breaks inserted.
  bool insertBefore(QStringRef text, Undo::Recorder recorder);
  // Inserts currentLineText, then a line break, then whole lines with text from [beginLinesToInsert, endLinesToInsert) (with a line break after each), then newLineText.
  using LinesToInsertIterator = std::vector<QString>::iterator;
  bool insertBefore(QStringRef currentLineText, LinesToInsertIterator beginLinesToInsert, LinesToInsertIterator endLinesToInsert, QStringRef newLineText, Undo::Recorder recorder);
  // Inserts the given lines with line breaks between them. No line break is inserted before the first line or after the last one.
  bool insertBefore(const std::vector<QStringRef>& lines, Undo::Recorder recorder);
  bool insertLineBreakBefore(Undo::Recorder recorder) { return insertBefore({}, {}, {}, {}, recorder); }

  bool deleteCharBefore(Undo::Recorder recorder);
  bool deleteCharAfter(Undo::Recorder recorder);
  bool deleteTo(Point* point, Undo::Recorder recorder);

  template<typename PointType>
  static void sortPair(typename std::remove_reference<PointType>::type* left, typename std::remove_reference<PointType>::type* right, PointType** first, PointType** second);

  LinesForwardsIterable linesForwards() { return LinesForwardsIterable(this); }

private:
  class LineIteratorImpl;

  friend class SafePoint;
  friend class TempPoint;

  Point(bool safe, Buffer* buffer);

  Point(const Point&) = delete;
  Point& operator=(const Point&) = delete;

  Buffer::Line* line() const { return &bufferLine_->value; }

  void setLine(Buffer::Tree::Node* newLine);
  void detachFromLine();
  void attachToLine();

  const bool safe_;
  Buffer* const buffer_ = nullptr;
  Buffer::Tree::Node* bufferLine_ = nullptr;
  int columnNumber_ = 0;
  int indexInLinePoints_ = -1;
};

class SafePoint : public Point {
public:
  SafePoint(Buffer* buffer) : Point(true, buffer) {}
};

class TempPoint : public Point {
public:
  TempPoint(const Point& other) : Point(false, other.buffer()) { moveTo(other); }
  TempPoint(Buffer* buffer, int line) : Point(false, buffer) {
    setLineNumber(line);
  }
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_BUFFER_H
