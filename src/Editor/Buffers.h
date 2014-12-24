#ifndef MED_EDITOR_BUFFERS_H
#define MED_EDITOR_BUFFERS_H

#include <list>
#include <string>
#include <QObject>

#include "Buffer.h"

namespace med {
namespace editor {

class Buffers : public QObject {
  Q_OBJECT
public:
  Buffers();
  virtual ~Buffers();
  
  void Open(const std::string& filePath);
  
signals:
  void newBuffer(Buffer* buffer);
  
private:
  std::list<std::unique_ptr<Buffer>> openBuffers;
};

}  // namespace editor
}  // namespace med

#endif // MED_EDITOR_BUFFERS_H

