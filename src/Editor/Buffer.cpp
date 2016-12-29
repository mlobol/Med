#include "Buffer.h"

#include <memory>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStringBuilder>

namespace Med {
namespace Editor {

class Point::LineIteratorImpl : public LineIterator::Impl {
public:
  LineIteratorImpl(const Point& from) : line_(from.bufferLine_) {}

  const QString* get() const override { return &line_->value.content; }

  bool advance() override {
    line_ = line_->adjacent(Util::DRBTreeDefs::Side::RIGHT);
    return line_;
  }

  Buffer::Tree::Node* line_;
};

Buffer::Buffer() {}
Buffer::~Buffer() {}

void Buffer::initFromStream(QTextStream* stream, const QString& name) {
  while (true) {
    QString line = stream->readLine();
    if (line.isNull()) break;
    insertLast()->node->value.content = std::move(line);
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

std::unique_ptr<Buffer> Buffer::create() {
  return std::unique_ptr<Buffer>(new Buffer());
}

std::unique_ptr<Buffer> Buffer::open(const std::string& filePath) {
  QFile file(QString::fromStdString(filePath));
  if (!file.open(QFile::ReadOnly)) {
    throw IOException("Failed to open file " + filePath + ".");
  }
  std::unique_ptr<Buffer> buffer(new Buffer());
  buffer->filePath_ = filePath;
  QFileInfo fileInfo(file);
  QTextStream stream(&file);
  buffer->initFromStream(&stream, fileInfo.fileName());
  return buffer;
}

bool Buffer::save() {
  if (filePath_.empty()) return false;
  QFile file(QString::fromStdString(filePath_));
  if (!file.open(QFile::Truncate | QFile::WriteOnly | QFile::Text)) {
    throw IOException("Failed to open file " + filePath_ + ".");
  }
  QTextStream stream(&file);
  TempPoint from(this, 0);
  for (const QString* lineContent : from.linesForwards()) {
    stream << *lineContent << '\n';
  }
  modified_ = false;
  return true;
}

Point::Point(Type type, Buffer* buffer) : type_(type), buffer_(buffer) {}
Point::~Point() {
  setLine({}); // Removes any references to the point from the buffer, which would become dangling after destruction.
}

void Point::setLine(Buffer::Tree::Node* newLine) {
  if (safe() && bufferLine_) {
    std::vector<SafePoint*>& points = line()->points;
    if (indexInLinePoints_ < points.size() - 1) {
      SafePoint*& pointsEntry = points[indexInLinePoints_];
      if (pointsEntry != this) throw new std::logic_error("Internal Error.");
      pointsEntry = points.back();
      pointsEntry->indexInLinePoints_ = indexInLinePoints_;
    }
    points.pop_back();
  }
  bufferLine_ = newLine;
  if (bufferLine_) {
    if (safe()) {
      std::vector<SafePoint*>& points = line()->points;
      indexInLinePoints_ = points.size();
      points.push_back(static_cast<SafePoint*>(this));
    }
    // Make sure the column number is within limits.
    setColumnNumber(columnNumber());
  }
}

void Point::setBufferAndLine(Buffer* buffer, Buffer::Tree::Node* newLine) {
  buffer_ = buffer;
  setLine(newLine);
}

bool Point::setColumnNumber(int columnNumber) {
  if (!bufferLine_) return false;
  columnNumber_ = qBound(0, columnNumber, lineContent().size());
  return true;
}

void Point::setLineNumber(int lineNumber) {
  setLine(buffer_->line(qBound(1, lineNumber, buffer_->lineCount() + 1))->node);
}

void Point::moveTo(const Point& point) {
  setLine(point.bufferLine_);
  setColumnNumber(point.columnNumber());
}

bool Point::moveToLineStart() {
  return setColumnNumber(0);
}

bool Point::moveToLineEnd() {
  if (!bufferLine_) return false;
  return setColumnNumber(lineContent().size());
}

bool Point::moveUp() {
  if (!bufferLine_) return false;
  Buffer::Tree::Node* newLine = bufferLine_->adjacent(Util::DRBTreeDefs::Side::LEFT);
  if (!newLine) return false;
  setLine(newLine);
  return true;
}

bool Point::moveDown() {
  if (!bufferLine_) return false;
  Buffer::Tree::Node* newLine = bufferLine_->adjacent(Util::DRBTreeDefs::Side::RIGHT);
  if (!newLine) return false;
  setLine(newLine);
  return true;
}

bool Point::moveLeft() {
  if (columnNumber() == 0) return moveUp() && moveToLineEnd();
  return setColumnNumber(columnNumber() - 1);
}

bool Point::moveRight() {
  if (!bufferLine_) return false;
  if (columnNumber() >= lineContent().size()) return moveDown() && moveToLineStart();
  return setColumnNumber(columnNumber() + 1);
}

bool Point::contentTo(const Point& other, QString* output) const {
  if (!isValid() || !other.isValid()) return false;
  const Point* from = nullptr;
  const Point* to = nullptr;
  sortPair(this, &other, &from, &to);
  for (TempPoint line(*from); line.isValid(); line.moveToStartOfNextLineOrMakeInvalid()) {
    const int start = line.sameLineAs(*from) ? from->columnNumber() : 0;
    const bool isLastLine = line.sameLineAs(*to);
    const int end = isLastLine ? to->columnNumber() : line.lineContent().size();
    output->append(line.lineContent().midRef(start, end - start));
    if (isLastLine) break;
    output->append('\n');
  }
  return true;
}

bool Point::insertBefore(QStringRef text, Undo::Recorder recorder) {
  if (!bufferLine_) return false;
  const int insertionColumnNumber = columnNumber();
  Q_ASSERT(columnNumber() <= lineContent().size());
  // Qt 5.5 and earlier don't provide QString::insert(int, QStringRef). Can be changed after Qt 5.6.
  TempPoint start(*this);
  line()->content.insert(insertionColumnNumber, text.constData(), text.size());
  for (Point* point : line()->points) {
    if (point->columnNumber() >= insertionColumnNumber) point->setColumnNumber(point->columnNumber() + text.size());
  }
  if (!safe()) setColumnNumber(insertionColumnNumber + text.size());
  if (recorder.undo) recorder.undo->recordInsertion(recorder.mode, start, *this);
  buffer_->modified_ = true;
  return true;
}

bool Point::insertBefore(QStringRef currentLineText, LinesToInsertIterator beginLinesToInsert, LinesToInsertIterator endLinesToInsert, QStringRef newLineText, Undo::Recorder recorder) {
  if (!bufferLine_) return false;
  TempPoint start(*this);
  const int insertionColumnNumber = columnNumber();
  Q_ASSERT(insertionColumnNumber <= lineContent().size());
  int newLineNumber = lineNumber();
  for (LinesToInsertIterator textToInsert = beginLinesToInsert; textToInsert != endLinesToInsert; ++textToInsert) {
    Buffer::Tree::Node* newLine = buffer_->insertLine(++newLineNumber)->node;
    newLine->value.content = std::move(*textToInsert);
  }
  Buffer::Tree::Node* newLine = buffer_->insertLine(++newLineNumber)->node;
  newLine->value.content = newLineText % lineContent().rightRef(lineContent().size() - insertionColumnNumber);
  line()->content = lineContent().leftRef(insertionColumnNumber) % currentLineText;
  // Saving reference as the loop below might move the point to a new line.
  std::vector<SafePoint*>& points = line()->points;
  const int insertionLength = newLineText.size();
  for (int pointIndex = 0; pointIndex < points.size();) {
    Point* point = points[pointIndex];
    if (point->columnNumber() >= insertionColumnNumber) {
      point->setColumnNumber(point->columnNumber() + insertionLength - insertionColumnNumber);
      // Removes this point from the points vector.
      point->setLine(newLine);
      continue;
    }
    ++pointIndex;
  }
  if (!safe()) {
    setColumnNumber(insertionLength);
    setLine(newLine);
  }
  if (recorder.undo) recorder.undo->recordInsertion(recorder.mode, start, *this);
  buffer_->modified_ = true;
  return true;
}

bool Point::insertBefore(const std::vector<QStringRef>& lines, Undo::Recorder recorder) {
  if (!bufferLine_) return false;
  if (lines.empty()) return true;
  if (lines.size() == 1) return insertBefore(lines.front(), recorder);
  std::vector<QString> fullLines;
  fullLines.reserve(lines.size() - 2);
  for (auto fullLine = lines.begin() + 1; fullLine != lines.end(); ++fullLine) {
    fullLines.push_back(fullLine->toString());
  }
  return insertBefore(lines.front(), fullLines.begin(), fullLines.end(), lines.back(), recorder);
}

bool Point::deleteCharBefore(Undo::Recorder recorder) {
  if (!isValid()) return false;
  TempPoint other(*this);
  other.moveLeft();
  return deleteTo(other, recorder);
}

bool Point::deleteCharAfter(Undo::Recorder recorder) {
  if (!isValid()) return false;
  TempPoint other(*this);
  other.moveRight();
  return deleteTo(other, recorder);
}

bool Point::deleteTo(const Point& other, Undo::Recorder recorder) {
  if (!isValid() || !other.isValid()) return false;
  if (recorder.undo) {
    const Point* from = nullptr;
    const Point* to = nullptr;
    sortPair(this, &other, &from, &to);
    moveContentBefore(other, recorder.undo->deletionHandling(recorder.mode, *from, *to));
  } else {
    moveContentBefore(other, TempPoint());
  }
  buffer_->modified_ = true;
  return true;
}

void Point::moveToStartOfNextLineOrMakeInvalid() {
  if (moveDown()) {
    moveToLineStart();
  } else {
    setLine({});
  }
}

Buffer::Tree::Node* Point::detachLineAndMoveToStartOfNextLineOrMakeInvalid() {
  Buffer::Tree::Node* bufferLine = bufferLine_;
  Buffer::Tree::Node* prevLine = bufferLine->adjacent(Util::DRBTreeDefs::Side::LEFT);
  moveToStartOfNextLineOrMakeInvalid();
  bufferLine->detach();
  if (prevLine) prevLine->setDelta(1);
  return bufferLine;
}

void Point::moveContentBefore(const Point& other, const Point& target) {
  const Point* from = nullptr;
  const Point* to = nullptr;
  sortPair(this, &other, &from, &to);
  TempPoint movingFrom(*from);
  TempPoint movingTarget(target);
  while (true) {
    // We iterate over the lines in order so that we can easily move the content to the destination.
    const bool isFirst = movingFrom.bufferLine_ == from->bufferLine_;
    const bool isLast = movingFrom.bufferLine_ == to->bufferLine_;
    if (isLast && to->columnNumber() == 0) {
      // Nothing to delete from last line.
      break;
    }
    if (isFirst || isLast) {
      // The first line stays in the source buffer, so can't move it completely. The last line is merged into the first line. In either case we need to adjust the line content and move the points.
      // We handle the last line here as it can also be the first, in case we're deleting from only one line.
      // To update the points we need the movingTarget.columnNumber() before the insertion.
      const int movingTargetColumnNumber = movingTarget.columnNumber();
      if (movingTarget.isValid()) {
        QStringRef movedContent =
            isLast
                ? movingFrom.lineContent().midRef(movingFrom.columnNumber(), to->columnNumber() - movingFrom.columnNumber())
                : movingFrom.lineContent().midRef(movingFrom.columnNumber());
        movingTarget.insertBefore(movedContent, {});
      }
      if (isFirst) {
        if (isLast) {
          from->line()->content.remove(from->columnNumber(), to->columnNumber() - from->columnNumber());
        } else {
          from->line()->content.truncate(from->columnNumber());
        }
      } else if (isLast) {
        QStringRef joinedContent = to->lineContent().midRef(to->columnNumber());
        from->line()->content.append(joinedContent);
      }
      const int toColumnNumber = to->columnNumber(); // Save as it might be updated by the loop.
      for (int pointIndex = 0; pointIndex < movingFrom.line()->points.size();) {
        SafePoint* point = movingFrom.line()->points[pointIndex];
        if (isLast && point->columnNumber() >= toColumnNumber) {
          // The point is after the deleted area.
          point->setColumnNumber(point->columnNumber() + from->columnNumber() - toColumnNumber);
          if (!isFirst) {
            point->setLine(from->bufferLine_);
            continue;
          }
        } else if (!isFirst || point->columnNumber() > from->columnNumber()) {
          // The point is in the deleted area.
          if (point->type_ == Type::CONTENT && movingTarget.isValid()) {
            // Content points are moved to the undo buffer.
            point->setColumnNumber(point->columnNumber() + movingTargetColumnNumber - movingFrom.columnNumber());
            point->setBufferAndLine(movingTarget.buffer_, movingTarget.bufferLine_);
            continue;
          } else {
            // Other points are moved to the point where the deleted area now collapsed.
            point->moveTo(*from);
            continue;
          }
        }
        // Otherwise the point is before the deleted area; nothing to do.
        ++pointIndex;
      }
      if (movingTarget.isValid() && !isLast) movingTarget.insertLineBreakBefore({});
      if (isFirst) {
        movingFrom.moveToStartOfNextLineOrMakeInvalid();
      } else {
        delete movingFrom.detachLineAndMoveToStartOfNextLineOrMakeInvalid();
      }
    } else {
      // Not the first or the last line, so we can move the whole line, together with content points, to the movingTarget.
      Buffer::Tree::Node* sourceLine = movingFrom.detachLineAndMoveToStartOfNextLineOrMakeInvalid();
      for (SafePoint* point : sourceLine->value.points) {
        if (movingTarget.isValid() && point->type_ == Type::CONTENT) {
          // TODO: points shouldn't have a pointer to the buffer; then this wouldn't be needed.
          point->buffer_ = movingTarget.buffer_;
        } else {
          point->moveTo(*from);
        }
      }
      if (movingTarget.isValid()) {
        Util::DRBTreeDefs::OperationOptions options;
        options.repeats = true;
        movingTarget.buffer_->tree_.attach(sourceLine, movingTarget.lineNumber(), options);
        sourceLine->setDelta(1);
        movingTarget.moveToStartOfNextLineOrMakeInvalid();
      } else {
        delete sourceLine;
      }
    }
    if (isLast) break;
  }
  if (!safe()) moveTo(movingFrom);
}

Point::LineIterator Point::LinesForwardsIterable::begin() {
  return {std::make_unique<LineIteratorImpl>(*from_)};
}

}  // namespace Editor
}  // namespace Med
