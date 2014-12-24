#include "Buffer.h"

#include <memory>
#include <QFile>
#include <QString>
#include <QTextStream>

#include "DRBTree.h"

namespace Med {
namespace Editor {

class Buffer::Lines {
public:
  typedef DRBTree<int, int, QString> Tree;
  
  Tree tree;
};
  
Buffer::Buffer() : lines(std::make_unique<Lines>()) {}
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

}  // namespace Editor
}  // namespace Med

