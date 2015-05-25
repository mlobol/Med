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

  static std::unique_ptr<Buffer> Open(const std::string& filePath);

  ~Buffer();

  const QString& name() { return name_; }

  int lineCount() const { return tree_.totalDelta(); }

private:
  friend class BufferTest;
  struct Line {
    std::vector<Point*> points;
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
  struct UnsafeLine {
    int lineNumber;
    const QString* lineContent;
  };

  typedef Util::IteratorHelper<UnsafeLine> Iterator;

  class LinesForwardsIterable {
  public:
    Iterator begin();
    Iterator end() { return {nullptr}; }

  private:
    friend Point;

    LinesForwardsIterable(Point* from) : from_(from) {}

    Point* from_;
  };

  Point(Buffer* buffer);
  ~Point();

  bool isValid() const { return line_.isValid(); }
  const QString& lineContent() const { return *modifiableLineContent(); }

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

  LinesForwardsIterable linesForwards() { return LinesForwardsIterable(this); }

private:
  class IteratorImpl;

  QString* modifiableLineContent() const { return &line_->node->value.content; }

  void setLine(Tree::Iterator newLine);
  void detachFromLine();
  void attachToLine();

  Buffer* const buffer_ = nullptr;
  Tree::Iterator line_;
  int columnNumber_ = 0;
  int indexInLinePoints_ = -1;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_BUFFER_H
