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
    LineIterator(Tree::Iterator treeIterator) : treeIterator(treeIterator) {}
    
    Line get() const override {
      return {treeIterator->key, &treeIterator->node->value};
    }
    
    bool advance() override {
      ++treeIterator;
      return !treeIterator.finished();
    }
    
    Tree::Iterator treeIterator;
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
  Lines::Tree::Iterator treeIterator = buffer->lines_->tree.get(lineNumber, {});
  if (!treeIterator.finished()) {
    lineIterator.reset(new Lines::LineIterator(treeIterator));
  }
  return {std::move(lineIterator)};
}

Buffer::IterableFromLineNumber Buffer::iterateFromLineNumber(int lineNumber) {
  return {this, lineNumber};
}

}  // namespace Editor
}  // namespace Med
