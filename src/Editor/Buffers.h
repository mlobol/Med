#ifndef MED_EDITOR_BUFFERS_H
#define MED_EDITOR_BUFFERS_H

#include <list>
#include <string>
#include <QObject>

#include "Buffer.h"

namespace Med {
namespace Editor {

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

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_BUFFERS_H

