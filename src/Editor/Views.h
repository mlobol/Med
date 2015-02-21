#ifndef MED_EDITOR_VIEWS_H
#define MED_EDITOR_VIEWS_H

#include <list>
#include <string>
#include <QObject>

#include "Buffer.h"
#include "View.h"

namespace Med {
namespace Editor {

class Views {
public:
  Views();
  virtual ~Views();
  
  View* newView(Buffer* buffer);
  
private:
  std::list<std::unique_ptr<View>> views_;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_VIEWS_H
