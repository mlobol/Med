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
    auto node = std::make_unique<Lines::Tree::Node>();
    node->value = stream->readLine();
    if (node->value.isNull()) break;
    int lineNumber = lines_->tree.extreme(Util::DRBTreeDefs::Side::RIGHT, {})->key + 1;
    lines_->tree.attach(node.release(), lineNumber, {});
  }
  name_ = name;
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
  auto node = new Lines::Tree::Node();
  Util::DRBTreeDefs::OperationOptions options;
  options.repeats = true;
  buffer_->lines_->tree.attach(node, lineNumber_ + 1, options);
  node->setDelta(1);
  if (currentLine) {
    node->value = currentLine->right(currentLine->size() - columnNumber_);
    currentLine->truncate(columnNumber_);
  }
  ++lineNumber_;
  columnNumber_ = 0;
  invalidateCache();
  ++buffer_->contentVersion_;
  return true;
}

}  // namespace Editor
}  // namespace Med
