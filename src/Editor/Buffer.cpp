#include "Buffer.h"

#include <memory>
#include <QtCore/QFile>

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
      if (treeIterator.finished()) return false;
      ++treeIterator;
      return true;
    }
    
    Tree::Iterator treeIterator;
  };
  
  Tree tree;
};

Buffer::Buffer() : lines(new Lines()) {}
Buffer::~Buffer() {}

void Buffer::InitFromStream(QTextStream* stream) {
  while (true) {
    auto node = std::make_unique<Lines::Tree::Node>();
    node->value = stream->readLine();
    if (node->value.isNull()) break;
    int lineNumber = lines->tree.extreme(Util::DRBTreeDefs::Side::RIGHT, {})->key + 1;
    lines->tree.attach(node.release(), lineNumber, {});
  }
}

std::unique_ptr<Buffer> Buffer::Open(const std::string& filePath) {
  QFile file(QString::fromStdString(filePath));
  if (!file.open(QFile::ReadOnly)) {
    throw IOException("Failed to open file " + filePath + ".");
  }
  std::unique_ptr<Buffer> buffer(new Buffer());
  QTextStream stream(&file);
  buffer->InitFromStream(&stream);
  return buffer;
}

Buffer::Iterator Buffer::IterableFromLineNumber::begin() {
  std::unique_ptr<Lines::LineIterator> lineIterator;
  Lines::Tree::Iterator treeIterator = buffer->lines->tree.get(lineNumber, {});
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
