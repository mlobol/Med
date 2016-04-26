#include "Buffer.h"

#include "Undo.h"

#include <memory>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStringBuilder>

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

std::unique_ptr<Buffer> Buffer::New() {
  return std::unique_ptr<Buffer>(new Buffer());
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

template<typename PointType>
void Buffer::Point::sortPair(typename std::remove_reference<PointType>::type* left, typename std::remove_reference<PointType>::type* right, PointType** first, PointType** second) {
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
  if (bufferLine_) {
    if (safe_) {
      std::vector<SafePoint*>& points = bufferLine_->value.points;
      indexInLinePoints_ = points.size();
      points.push_back(static_cast<SafePoint*>(this));
    }
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

bool Buffer::Point::contentTo(const Point& other, QString* output) const {
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

bool Buffer::Point::insertBefore(QStringRef text, UndoOps* opsToUndo) {
  if (!bufferLine_) return false;
  const int columnNumber = columnNumber_;
  Q_ASSERT(columnNumber_ <= lineContent().size());
  // Qt 5.5 and earlier don't provide QString::insert(int, QStringRef). Can be changed after Qt 5.6.
  TempPoint start(*this);
  line()->content.insert(columnNumber, text.constData(), text.size());
  for (Point* point : line()->points) {
    if (point->columnNumber_ >= columnNumber) point->columnNumber_ += text.size();
  }
  if (opsToUndo) opsToUndo->recordInsertion(start, *this);
  return true;
}

bool Buffer::Point::insertBefore(QStringRef currentLineText, LinesToInsertIterator beginLinesToInsert, LinesToInsertIterator endLinesToInsert, QStringRef newLineText, UndoOps* opsToUndo) {
  if (!bufferLine_) return false;
  TempPoint start(*this);
  const int columnNumber = columnNumber_;
  Q_ASSERT(columnNumber <= lineContent().size());
  int newLineNumber = lineNumber();
  for (LinesToInsertIterator textToInsert = beginLinesToInsert; textToInsert != endLinesToInsert; ++textToInsert) {
    Tree::Node* newLine = buffer_->insertLine(++newLineNumber)->node;
    newLine->value.content = std::move(*textToInsert);
  }
  Tree::Node* newLine = buffer_->insertLine(++newLineNumber)->node;
  newLine->value.content = newLineText % lineContent().rightRef(lineContent().size() - columnNumber);
  line()->content = lineContent().leftRef(columnNumber) % currentLineText;
  // Saving reference as the loop below might move the point to a new line.
  std::vector<SafePoint*>& points = line()->points;
  const int insertLength = newLineText.size();
  for (int point_index = 0; point_index < points.size();) {
    Point* point = points[point_index];
    if (point->columnNumber_ >= columnNumber) {
      point->columnNumber_ += insertLength - columnNumber;
      // Removes this point from the points vector.
      point->setLine(newLine);
      continue;
    }
    ++point_index;
  }
  if (opsToUndo) opsToUndo->recordInsertion(start, *this);
  return true;
}

bool Buffer::Point::insertBefore(const std::vector<QStringRef>& lines, UndoOps* opsToUndo) {
  if (!bufferLine_) return false;
  if (lines.empty()) return true;
  if (lines.size() == 1) return insertBefore(lines.front(), opsToUndo);
  std::vector<QString> fullLines;
  fullLines.reserve(lines.size() - 2);
  for (auto fullLine = lines.begin() + 1; fullLine != lines.end(); ++fullLine) {
    fullLines.push_back(fullLine->toString());
  }
  return insertBefore(lines.front(), fullLines.begin(), fullLines.end(), lines.back(), opsToUndo);
}

bool Buffer::Point::deleteCharBefore(UndoOps* opsToUndo) {
  if (!isValid()) return false;
  TempPoint other(*this);
  other.moveLeft();
  return deleteTo(&other, opsToUndo);
}

bool Buffer::Point::deleteCharAfter(UndoOps* opsToUndo) {
  if (!isValid()) return false;
  TempPoint other(*this);
  other.moveRight();
  return deleteTo(&other, opsToUndo);
}

bool Buffer::Point::deleteTo(Point* other, UndoOps* opsToUndo) {
  if (!isValid() || !other->isValid()) return false;
  Point* from = nullptr;
  Point* to = nullptr;
  sortPair(this, other, &from, &to);
  if (from->sameLineAs(*to)) {
    const int start = from->columnNumber();
    const int end = to->columnNumber();
    if (opsToUndo) opsToUndo->recordPartialLineDeletion(*from, *to, from->line()->content.midRef(start, end - start));
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
    QStringRef firstLineDeleted = opsToUndo ? fromContent.midRef(from->columnNumber()) : QStringRef();
    std::vector<QString> linesDeleted;
    while (true) {
      Tree::Node* bufferLine = from->bufferLine_->adjacent(Util::DRBTreeDefs::Side::RIGHT);
      QString& content = bufferLine->value.content;
      const bool isLast = bufferLine == to->bufferLine_;
      if (isLast) {
        if (opsToUndo) opsToUndo->recordMultilineDeletion(*from, *to, firstLineDeleted, std::move(linesDeleted), content.leftRef(to->columnNumber()));
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
      if (opsToUndo) linesDeleted.push_back(std::move(bufferLine->value.content));
      delete bufferLine;
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
