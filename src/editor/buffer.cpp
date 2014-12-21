#include "buffer.h"
#include <qt5/QtCore/qstring.h>

#include <QtCore/qfile.h>

namespace med {
namespace editor {
  
Buffer::Buffer() {}
Buffer::~Buffer() {}

std::unique_ptr<Buffer> Buffer::Open(const std::string& file_path) {
  QFile file(QString::fromStdString(file_path));
  if (!file.open(QFile::ReadOnly)) {
    throw IOException("Failed to open file " + file_path + ".");
  }
  std::unique_ptr<Buffer> buffer(new Buffer());
  //buffer->
};

}  // namespace editor
}  // namespace med

