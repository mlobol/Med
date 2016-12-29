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

Point::Point(bool safe, Buffer* buffer) : safe_(safe), buffer_(buffer) {}
Point::~Point() { setLine({}); }

template<typename PointType>
void Point::sortPair(typename std::remove_reference<PointType>::type* left, typename std::remove_reference<PointType>::type* right, PointType** first, PointType** second) {
  const bool leftIsFirst = left->bufferLine_ != right->bufferLine_ ? left->lineNumber() < right->lineNumber() : left->columnNumber() < right->columnNumber();
  *first = leftIsFirst ? left : right;
  *second = leftIsFirst ? right : left;
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
  buffer_->modified_ = true;
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
  for (TempPoint line(*from); line.isValid(); line.moveDown()) {
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
  return deleteTo(&other, recorder);
}

bool Point::deleteCharAfter(Undo::Recorder recorder) {
  if (!isValid()) return false;
  TempPoint other(*this);
  other.moveRight();
  return deleteTo(&other, recorder);
}

bool Point::deleteTo(Point* other, Undo::Recorder recorder) {
  if (!isValid() || !other->isValid()) return false;
  Point* from = nullptr;
  Point* to = nullptr;
  sortPair(this, other, &from, &to);
  if (from->sameLineAs(*to)) {
    const int start = from->columnNumber();
    const int end = to->columnNumber();
    if (recorder.undo) recorder.undo->recordPartialLineDeletion(recorder.mode, *from, *to, from->line()->content.midRef(start, end - start));
    for (SafePoint* point : from->bufferLine_->value.points) {
      if (point->columnNumber() > end) {
        point->columnNumber_ -= end - start;
      } else if (point->columnNumber() > start) {
        point->columnNumber_ = start;
      }
    }
    from->line()->content.remove(start, end - start);
  } else {
    QString& fromContent = from->bufferLine_->value.content;
    QStringRef firstLineDeleted = recorder.undo ? fromContent.midRef(from->columnNumber()) : QStringRef();
    std::vector<QString> linesDeleted;
    while (true) {
      Buffer::Tree::Node* bufferLine = from->bufferLine_->adjacent(Util::DRBTreeDefs::Side::RIGHT);
      QString& content = bufferLine->value.content;
      const bool isLast = bufferLine == to->bufferLine_;
      if (isLast) {
        if (recorder.undo) recorder.undo->recordMultilineDeletion(recorder.mode, *from, *to, firstLineDeleted, std::move(linesDeleted), content.leftRef(to->columnNumber()));
        fromContent.truncate(from->columnNumber());
        fromContent.append(content.mid(to->columnNumber()));
        for (SafePoint* point : from->bufferLine_->value.points) {
          if (point->columnNumber() > from->columnNumber()) {
            point->columnNumber_ = from->columnNumber();
          }
        }
      }
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
      from->bufferLine_->setDelta(1);
      if (recorder.undo) linesDeleted.push_back(std::move(bufferLine->value.content));
      delete bufferLine;
      if (isLast) break;
    }
  }
  buffer_->modified_ = true;
  return true;
}

Point::LineIterator Point::LinesForwardsIterable::begin() {
  return {std::make_unique<LineIteratorImpl>(*from_)};
}

}  // namespace Editor
}  // namespace Med
