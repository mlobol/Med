#include "Buffer.h"

#include <memory>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace Med {
namespace Editor {

class Buffer::Point::LineIteratorImpl : public LineIterator::Impl {
public:
  LineIteratorImpl(const Point& from) : line_(from.bufferLine_) {}

  const QString* get() const override { return &line_->value.content; }

  bool advance() override {
    line_ = line_->adjacent(Util::DRBTreeDefs::Side::RIGHT);
    return line_;
  }

  Tree::Node* line_;
};

Buffer::Buffer() {}
Buffer::~Buffer() {}

void Buffer::InitFromStream(QTextStream* stream, const QString& name) {
  while (true) {
    QString line = stream->readLine();
    if (line.isNull()) break;
    insertLine(lineCount() + 1)->node->value.content = line;
  }
  name_ = name;
}

Buffer::Tree::Iterator Buffer::line(int lineNumber) {
  return tree_.get(lineNumber, {});
}

Buffer::Tree::Iterator Buffer::insertLine(int lineNumber) {
  auto node = new Tree::Node();
  Util::DRBTreeDefs::OperationOptions options;
  options.repeats = true;
  Tree::Iterator line = tree_.attach(node, lineNumber, options);
  node->setDelta(1);
  return line;
}

std::unique_ptr<Buffer> Buffer::Open(const std::string& filePath) {
  QFile file(QString::fromStdString(filePath));
  if (!file.open(QFile::ReadOnly)) {
    throw IOException("Failed to open file " + filePath + ".");
  }
  std::unique_ptr<Buffer> buffer(new Buffer());
  QFileInfo fileInfo(file);
  QTextStream stream(&file);
  buffer->InitFromStream(&stream, fileInfo.baseName());
  return buffer;
}

Buffer::Point::Point(bool safe, Buffer* buffer) : safe_(safe), buffer_(buffer) {}
Buffer::Point::~Point() { setLine({}); }

void Buffer::Point::sortPair(Point* left, Point* right, Point** first, Point** second) {
  const bool leftIsFirst = left->bufferLine_ != right->bufferLine_ ? left->lineNumber() < right->lineNumber() : left->columnNumber() < right->columnNumber();
  *first = leftIsFirst ? left : right;
  *second = leftIsFirst ? right : left;
}

void Buffer::Point::setLine(Tree::Node* newLine) {
  if (safe_ && bufferLine_) {
    std::vector<SafePoint*>& points = bufferLine_->value.points;
    if (indexInLinePoints_ < points.size() - 1) {
      SafePoint*& pointsEntry = points[indexInLinePoints_];
      if (pointsEntry != this) throw new std::logic_error("Internal Error.");
      pointsEntry = points.back();
      pointsEntry->indexInLinePoints_ = indexInLinePoints_;
    }
    points.pop_back();
  }
  bufferLine_ = newLine;
  if (safe_ && bufferLine_) {
    std::vector<SafePoint*>& points = bufferLine_->value.points;
    indexInLinePoints_ = points.size();
    points.push_back(static_cast<SafePoint*>(this));
    // Make sure the column number is within limits.
    setColumnNumber(columnNumber_);
  }
}

bool Buffer::Point::setColumnNumber(int columnNumber) {
  if (!bufferLine_) return false;
  columnNumber_ = qBound(0, columnNumber, lineContent().size());
  return true;
}

void Buffer::Point::setLineNumber(int lineNumber) {
  setLine(buffer_->line(qBound(1, lineNumber, buffer_->lineCount() + 1))->node);
}

void Buffer::Point::moveTo(const Point& point) {
  setLine(point.bufferLine_);
  setColumnNumber(point.columnNumber_);
}

bool Buffer::Point::moveToLineStart() {
  columnNumber_ = 0;
  return true;
}

bool Buffer::Point::moveToLineEnd() {
  if (!bufferLine_) return false;
  columnNumber_ = lineContent().size();
  return true;
}

bool Buffer::Point::moveUp() {
  if (!bufferLine_) return false;
  Tree::Node* newLine = bufferLine_->adjacent(Util::DRBTreeDefs::Side::LEFT);
  if (!newLine) return false;
  setLine(newLine);
  return true;
}

bool Buffer::Point::moveDown() {
  if (!bufferLine_) return false;
  Tree::Node* newLine = bufferLine_->adjacent(Util::DRBTreeDefs::Side::RIGHT);
  if (!newLine) return false;
  setLine(newLine);
  return true;
}

bool Buffer::Point::moveLeft() {
  if (columnNumber_ == 0) return moveUp() && moveToLineEnd();
  --columnNumber_;
  return true;
}

bool Buffer::Point::moveRight() {
  if (!bufferLine_) return false;
  if (columnNumber_ >= lineContent().size()) return moveDown() && moveToLineStart();
  ++columnNumber_;
  return true;
}

bool Buffer::Point::insertBefore(const QString& text) {
  if (!bufferLine_) return false;
  const int columnNumber = columnNumber_;
  Q_ASSERT(columnNumber_ <= lineContent().size());
  line()->content.insert(columnNumber, text);
  for (Point* point : line()->points) {
    if (point->columnNumber_ >= columnNumber) point->columnNumber_ += text.size();
  }
  return true;
}

bool Buffer::Point::insertLineBreakBefore() {
  if (!bufferLine_) return false;
  Tree::Node* newLine = buffer_->insertLine(lineNumber() + 1)->node;
  const int columnNumber = columnNumber_;
  Q_ASSERT(columnNumber <= lineContent().size());
  newLine->value.content = lineContent().right(lineContent().size() - columnNumber);
  line()->content.truncate(columnNumber);
  std::vector<SafePoint*>& points = line()->points;
  for (int point_index = 0; point_index < points.size();) {
    Point* point = points[point_index];
    if (point->columnNumber_ >= columnNumber) {
      // Removes this point from the points vector.
      point->columnNumber_ -= columnNumber;
      point->setLine(newLine);
      continue;
    }
    ++point_index;
  }
  return true;
}

bool Buffer::Point::deleteCharBefore() {
  if (!isValid()) return false;
  TempPoint other(*this);
  other.moveLeft();
  return deleteTo(&other);
}

bool Buffer::Point::deleteCharAfter() {
  if (!isValid()) return false;
  TempPoint other(*this);
  other.moveRight();
  return deleteTo(&other);
}

bool Buffer::Point::deleteTo(Point* other) {
  if (!isValid() || !other->isValid()) return false;
  Point* from = nullptr;
  Point* to = nullptr;
  sortPair(this, other, &from, &to);
  if (from->bufferLine_ == to->bufferLine_) {
    from->line()->content.remove(from->columnNumber(), to->columnNumber() - from->columnNumber());
    for (SafePoint* point : from->bufferLine_->value.points) {
      if (point->columnNumber() > to->columnNumber()) {
        point->columnNumber_ -= to->columnNumber() - from->columnNumber();
      } else if (point->columnNumber() > from->columnNumber()) {
        point->columnNumber_ = from->columnNumber();
      }
    }
  } else {
    QString& fromContent = from->bufferLine_->value.content;
    fromContent.truncate(from->columnNumber());
    fromContent.append(to->bufferLine_->value.content.mid(to->columnNumber()));
    for (SafePoint* point : from->bufferLine_->value.points) {
      if (point->columnNumber() > from->columnNumber()) {
        point->columnNumber_ = from->columnNumber();
      }
    }
    while (true) {
      Tree::Node* bufferLine = from->bufferLine_->adjacent(Util::DRBTreeDefs::Side::RIGHT);
      const bool isLast = bufferLine == to->bufferLine_;
      while (!bufferLine->value.points.empty()) {
        SafePoint* point = bufferLine->value.points.back();
        if (isLast && point->columnNumber() > to->columnNumber()) {
          point->columnNumber_ += from->columnNumber() - to->columnNumber();
        } else {
          point->columnNumber_ = from->columnNumber();
        }
        point->setLine(from->bufferLine_);
      }
      bufferLine->detach();
      delete bufferLine;
      from->bufferLine_->setDelta(1);
      if (isLast) break;
    }
  }
  return true;
}

Buffer::Point::LineIterator Buffer::Point::LinesForwardsIterable::begin() {
  return {std::make_unique<LineIteratorImpl>(*from_)};
}

}  // namespace Editor
}  // namespace Med
