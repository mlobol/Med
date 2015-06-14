#include "Buffer.h"

#include <memory>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace Med {
namespace Editor {

class Buffer::Point::IteratorImpl : public Iterator::Impl {
public:
  IteratorImpl(const Point& from) : line_(from.bufferLine_) {}

  const QString* get() const override { return &line_->node->value.content; }

  bool advance() override {
    ++line_;
    return line_.isValid();
  }

  Tree::Iterator line_;
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

Buffer::Point::Point(Buffer* buffer) : buffer_(buffer) {}
Buffer::Point::~Point() { setLine({}); }

void Buffer::Point::setLine(Tree::Iterator newLine) {
  if (bufferLine_.isValid()) {
    std::vector<Point*>& points = bufferLine_->node->value.points;
    if (indexInLinePoints_ < points.size() - 1) {
      Point*& pointsEntry = points[indexInLinePoints_];
      if (pointsEntry != this) throw new std::logic_error("Internal Error.");
      pointsEntry = points.back();
      pointsEntry->indexInLinePoints_ = indexInLinePoints_;
    }
    points.pop_back();
  }
  bufferLine_ = newLine;
  if (bufferLine_.isValid()) {
    std::vector<Point*>& points = bufferLine_->node->value.points;
    indexInLinePoints_ = points.size();
    points.push_back(this);
    // Make sure the column number is within limits.
    setColumnNumber(columnNumber_);
  }
}

bool Buffer::Point::setColumnNumber(int columnNumber) {
  if (!bufferLine_.isValid()) return false;
  columnNumber_ = qBound(0, columnNumber, lineContent().size());
  return true;
}

void Buffer::Point::setLineNumber(int lineNumber) {
  setLine(buffer_->line(qBound(1, lineNumber, buffer_->lineCount() + 1)));
}

bool Buffer::Point::moveToLineStart() {
  columnNumber_ = 0;
  return true;
}

bool Buffer::Point::moveToLineEnd() {
  if (!bufferLine_.isValid()) return false;
  columnNumber_ = lineContent().size();
  return true;
}

bool Buffer::Point::moveUp() {
  if (!bufferLine_.isValid()) return false;
  Tree::Iterator newLine = bufferLine_;
  --newLine;
  if (!newLine.isValid()) return false;
  setLine(newLine);
  return true;
}

bool Buffer::Point::moveDown() {
  if (!bufferLine_.isValid()) return false;
  Tree::Iterator newLine = bufferLine_;
  ++newLine;
  if (!newLine.isValid()) return false;
  setLine(newLine);
  return true;
}

bool Buffer::Point::moveLeft() {
  if (columnNumber_ == 0) return moveUp() && moveToLineEnd();
  --columnNumber_;
  return true;
}

bool Buffer::Point::moveRight() {
  if (!bufferLine_.isValid()) return false;
  if (columnNumber_ >= lineContent().size()) return moveDown() && moveToLineStart();
  ++columnNumber_;
  return true;
}

bool Buffer::Point::insertBefore(const QString& text) {
  if (!bufferLine_.isValid()) return false;
  const int columnNumber = columnNumber_;
  Q_ASSERT(columnNumber_ <= lineContent().size());
  line()->content.insert(columnNumber, text);
  for (Point* point : line()->points) {
    if (point->columnNumber_ >= columnNumber) point->columnNumber_ += text.size();
  }
  return true;
}

bool Buffer::Point::insertLineBreakBefore() {
  if (!bufferLine_.isValid()) return false;
  Tree::Iterator newLine = buffer_->insertLine(lineNumber() + 1);
  const int columnNumber = columnNumber_;
  Q_ASSERT(columnNumber_ <= lineContent().size());
  newLine->node->value.content = lineContent().right(lineContent().size() - columnNumber);
  line()->content.truncate(columnNumber);
  std::vector<Point*>& points = line()->points;
  for (int point_index = 0; point_index < points.size();) {
    Point* point = points[point_index];
    if (point->columnNumber_ >= columnNumber) {
      // Removes this point from the points vector.
      point->setLine(newLine);
      point->columnNumber_ -= columnNumber;
      continue;
    }
    ++point_index;
  }
  return true;
}

bool Buffer::Point::deleteCharAfter() {
  if (!bufferLine_.isValid()) return false;
  Q_ASSERT(columnNumber_ <= lineContent().size());
  if (columnNumber_ == lineContent().size()) {
    Tree::Iterator nextBufferLine = bufferLine_;
    ++nextBufferLine;
    if (!nextBufferLine.isValid()) return false;
    Line& nextLine = nextBufferLine->node->value;
    line()->content.append(nextLine.content);
    while (!nextLine.points.empty()) {
      Point* point = nextLine.points.back();
      point->setLine(bufferLine_);
      point->columnNumber_ += columnNumber_;
    }
    nextBufferLine->node->detach();
    delete nextBufferLine->node;
    bufferLine_->node->setDelta(1);
  } else {
    line()->content.remove(columnNumber_, 1);
    for (Point* point : line()->points) {
      if (point->columnNumber_ > columnNumber_) --point->columnNumber_;
    }
  }
  return true;
}

Buffer::Point::Iterator Buffer::Point::LinesForwardsIterable::begin() {
  return {std::make_unique<IteratorImpl>(*from_)};
}

}  // namespace Editor
}  // namespace Med
