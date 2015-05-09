#include "Buffer.h"

#include <memory>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include "Util/DRBTree.h"

namespace Med {
namespace Editor {

class Buffer::Lines {
public:
  typedef Util::DRBTree<int, int, QString> Tree;
  
  class LineIterator : public Iterator::Impl {
  public:
    LineIterator(Tree::Iterator treeIterator) : treeIterator_(treeIterator) {}
    
    Line get() const override {
      return {treeIterator_->key, &treeIterator_->node->value};
    }
    
    bool advance() override {
      ++treeIterator_;
      return !treeIterator_.finished();
    }
    
    Tree::Iterator treeIterator_;
  };
  
  Tree tree;
};

Buffer::Buffer() : lines_(new Lines()) {}
Buffer::~Buffer() {}

void Buffer::InitFromStream(QTextStream* stream, const QString& name) {
  while (true) {
    QString line = stream->readLine();
    if (line.isNull()) break;
    *insertLine(lineCount()) = line;
  }
  name_ = name;
}

QString* Buffer::insertLine(int lineNumber) {
  auto node = new Lines::Tree::Node();
  Util::DRBTreeDefs::OperationOptions options;
  options.repeats = true;
  lines_->tree.attach(node, lineNumber, options);
  node->setDelta(1);
  return &node->value;
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
  std::unique_ptr<Lines::LineIterator> lineIterator;
  Lines::Tree::Iterator treeIterator = buffer_->lines_->tree.get(lineNumber_, {});
  if (!treeIterator.finished()) {
    lineIterator.reset(new Lines::LineIterator(treeIterator));
  }
  return {std::move(lineIterator)};
}

int Buffer::lineCount() { return lines_->tree.totalDelta(); }

Buffer::IterableFromLineNumber Buffer::iterateFromLineNumber(int lineNumber) {
  return {this, lineNumber};
}

QString* Buffer::Point::line() {
  if (!buffer_) return nullptr;
  if (buffer_->contentVersion() == contentVersion_) return cachedLine_;
  cachedLine_ = nullptr;
  for (Line line : buffer_->iterateFromLineNumber(lineNumber_)) {
    cachedLine_ = line.content;
    break;
  }
  return cachedLine_;
}

bool Buffer::Point::setColumnNumber(int columnNumber) {
  QString* currentLine = line();
  if (!currentLine) return false;
  columnNumber_ = qBound(0, columnNumber, currentLine->size());
  return true;
}

void Buffer::Point::setLineNumber(int lineNumber) {
  lineNumber_ = qBound(0, lineNumber, buffer_->lineCount());
  invalidateCache();
}

bool Buffer::Point::moveLeft() {
  if (columnNumber_ > 0) --columnNumber_;
  else if (lineNumber_ > 0) {
    --lineNumber_;
    invalidateCache();
    QString* currentLine = line();
    if (currentLine) columnNumber_ = currentLine->size();
  } else return false;
  return true;
}

bool Buffer::Point::moveRight() {
  QString* currentLine = line();
  if (!currentLine) return false;
  if (columnNumber_ < currentLine->size()) ++columnNumber_;
  else if (lineNumber_ < buffer_->lineCount()) {
    ++lineNumber_;
    columnNumber_ = 0;
    invalidateCache();
  } else return false;
  return true;
}

bool Buffer::Point::insertBefore(const QString& text) {
  QString* lineContent = line();
  if (!lineContent) return false;
  lineContent->insert(columnNumber_, text);
  columnNumber_ += text.size();
  ++buffer_->contentVersion_;
  return true;
}

bool Buffer::Point::insertLineBreakBefore() {
  QString* currentLine = line();
  QString* newLine = buffer_->insertLine(lineNumber_ + 1);
  if (currentLine) {
    *newLine = currentLine->right(currentLine->size() - columnNumber_);
    currentLine->truncate(columnNumber_);
  }
  ++lineNumber_;
  columnNumber_ = 0;
  invalidateCache();
  ++buffer_->contentVersion_;
  return true;
}

bool Buffer::Point::deleteCharBefore() {
  // TODO: should join lines
  if (columnNumber_ == 0) return false;
  QString* currentLine = line();
  if (!currentLine) return false;
  --columnNumber_;
  currentLine->remove(columnNumber_, 1);
  ++buffer_->contentVersion_;
  return true;
}

}  // namespace Editor
}  // namespace Med
