#include "Buffer.h"

#include <memory>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace Med {
namespace Editor {
  
class Buffer::LineIterator : public Iterator::Impl {
public:
  LineIterator(Tree::Iterator treeIterator) : treeIterator_(treeIterator) {}
  
  Line get() const override {
    return {treeIterator_->key, &treeIterator_->node->value};
  }
  
  bool advance() override {
    ++treeIterator_;
    return treeIterator_.isValid();
  }
  
  Tree::Iterator treeIterator_;
};

Buffer::Buffer() {}
Buffer::~Buffer() {}

void Buffer::InitFromStream(QTextStream* stream, const QString& name) {
  while (true) {
    QString line = stream->readLine();
    if (line.isNull()) break;
    insertLine(lineCount())->node->value = line;
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

Buffer::Iterator Buffer::IterableFromLineNumber::begin() {
  std::unique_ptr<LineIterator> lineIterator;
  Tree::Iterator treeIterator = buffer_->tree_.get(lineNumber_, {});
  if (treeIterator.isValid()) {
    lineIterator.reset(new LineIterator(treeIterator));
  }
  return {std::move(lineIterator)};
}

bool Buffer::Point::setColumnNumber(int columnNumber) {
  if (!line_.isValid()) return false;
  columnNumber_ = qBound(0, columnNumber, lineContent()->size());
  return true;
}

void Buffer::Point::setLineNumber(int lineNumber) {
  line_ = buffer_->line(qBound(0, lineNumber, buffer_->lineCount()));
}

bool Buffer::Point::moveToLineStart() {
  columnNumber_ = 0;
  return true;
}

bool Buffer::Point::moveToLineEnd() {
  if (!line_.isValid()) return false;
  columnNumber_ = lineContent()->size();
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
  if (columnNumber_ >= lineContent()->size()) return moveDown() && moveToLineStart();
  ++columnNumber_;
  return true;
}

bool Buffer::Point::insertBefore(const QString& text) {
  if (!line_.isValid()) return false;
  lineContent()->insert(columnNumber_, text);
  columnNumber_ += text.size();
  return true;
}

bool Buffer::Point::insertLineBreakBefore() {
  if (!line_.isValid()) return false;
  Tree::Iterator newLine = buffer_->insertLine(lineNumber() + 1);
  newLine->node->value = lineContent()->right(lineContent()->size() - columnNumber_);
  lineContent()->truncate(columnNumber_);
  line_ = newLine;
  columnNumber_ = 0;
  return true;
}

bool Buffer::Point::deleteCharAfter() {
  if (!line_.isValid()) return false;
  if (columnNumber_ == lineContent()->size()) {
    // TODO: implement joining lines
    // if (!buffer_->joinLines(lineNumber_, 1)) return false;
    return false;
  } else {
    lineContent()->remove(columnNumber_, 1);
  }
  return true;
}

}  // namespace Editor
}  // namespace Med
