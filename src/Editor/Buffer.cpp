#include "Buffer.h"

#include <memory>
#include <QFile>
#include <QString>
#include <QTextStream>

#include "Util/DRBTree.h"

namespace Med {
namespace Editor {

class Buffer::Lines {
public:
  typedef DRBTree<int, int, QString> Tree;
  
  class LineIterator : public Iterator::Impl {
  public:
    LineIterator(Tree::Iterator treeIterator) : treeIterator(treeIterator) {}
    
    Line get() const override {
      return {treeIterator->key, &treeIterator->node->value};
    }
    
    bool advance() override {
      if (treeIterator.finished()) return true;
      ++treeIterator;
      return false;
    }
    
    Tree::Iterator treeIterator;
  };
  
  Tree tree;
};

Buffer::Buffer() : lines(new Lines()) {}
Buffer::~Buffer() {}

std::unique_ptr<Buffer> Buffer::Open(const std::string& filePath) {
  QFile file(QString::fromStdString(filePath));
  if (!file.open(QFile::ReadOnly)) {
    throw IOException("Failed to open file " + filePath + ".");
  }
  std::unique_ptr<Buffer> buffer(new Buffer());
  QTextStream stream(&file);
  while (true) {
    auto node = std::make_unique<Lines::Tree::Node>();
    node->value = stream.readLine();
    if (node->value.isNull()) break;
    int line = buffer->lines->tree.extreme(DRBTreeDefs::Side::RIGHT, {})->key + 1;
    buffer->lines->tree.attach(node.get(), line, {});
  }
  return buffer;
}

Buffer::Iterator Buffer::IterableFromLineNumber::begin() {
  return {std::make_unique<Lines::LineIterator>(buffer->lines->tree.get(lineNumber, {}))};
}
  
Buffer::IterableFromLineNumber Buffer::iterateFromLineNumber(int lineNumber) {
  return {this, lineNumber};
}

}  // namespace Editor
}  // namespace Med

