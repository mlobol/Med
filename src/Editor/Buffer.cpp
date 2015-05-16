#include "Buffer.h"

#include <memory>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace Med {
namespace Editor {

class Buffer::Point::IteratorImpl : public Iterator::Impl {
public:
  IteratorImpl(const Point& from) : point_(from) {}

  Point get() const override { return point_; }

  bool advance() override {
    ++point_.line_;
    return point_.line_.isValid();
  }

  Point point_;
};

Buffer::Buffer() {}
Buffer::~Buffer() {}

void Buffer::InitFromStream(QTextStream* stream, const QString& name) {
  while (true) {
    QString line = stream->readLine();
    if (line.isNull()) break;
    insertLine(lineCount() + 1)->node->value = line;
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

bool Buffer::Point::setColumnNumber(int columnNumber) {
  if (!line_.isValid()) return false;
  columnNumber_ = qBound(0, columnNumber, modifiableLineContent()->size());
  return true;
}

void Buffer::Point::setLineNumber(int lineNumber) {
  line_ = buffer_->line(qBound(1, lineNumber, buffer_->lineCount() + 1));
}

bool Buffer::Point::moveToLineStart() {
  columnNumber_ = 0;
  return true;
}

bool Buffer::Point::moveToLineEnd() {
  if (!line_.isValid()) return false;
  columnNumber_ = modifiableLineContent()->size();
  return true;
}

bool Buffer::Point::moveUp() {
  if (!line_.isValid()) return false;
  Tree::Iterator newLine = line_;
  --newLine;
  if (!newLine.isValid()) return false;
  line_ = newLine;
  return true;
}

bool Buffer::Point::moveDown() {
  if (!line_.isValid()) return false;
  Tree::Iterator newLine = line_;
  ++newLine;
  if (!newLine.isValid()) return false;
  line_ = newLine;
  return true;
}

bool Buffer::Point::moveLeft() {
  if (columnNumber_ == 0) return moveUp() && moveToLineEnd();
  --columnNumber_;
  return true;
}

bool Buffer::Point::moveRight() {
  if (!line_.isValid()) return false;
  if (columnNumber_ >= modifiableLineContent()->size()) return moveDown() && moveToLineStart();
  ++columnNumber_;
  return true;
}

bool Buffer::Point::insertBefore(const QString& text) {
  if (!line_.isValid()) return false;
  modifiableLineContent()->insert(columnNumber_, text);
  columnNumber_ += text.size();
  return true;
}

bool Buffer::Point::insertLineBreakBefore() {
  if (!line_.isValid()) return false;
  Tree::Iterator newLine = buffer_->insertLine(lineNumber() + 1);
  newLine->node->value = modifiableLineContent()->right(modifiableLineContent()->size() - columnNumber_);
  modifiableLineContent()->truncate(columnNumber_);
  line_ = newLine;
  columnNumber_ = 0;
  return true;
}

bool Buffer::Point::deleteCharAfter() {
  if (!line_.isValid()) return false;
  if (columnNumber_ == modifiableLineContent()->size()) {
    // TODO: implement joining lines
    // if (!buffer_->joinLines(lineNumber_, 1)) return false;
    return false;
  } else {
    modifiableLineContent()->remove(columnNumber_, 1);
  }
  return true;
}

Buffer::Point::Iterator Buffer::Point::LinesForwardsIterable::begin() {
  return {std::make_unique<IteratorImpl>(*from_)};
}

}  // namespace Editor
}  // namespace Med
