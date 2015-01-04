#include "Buffer.h"

#include <QtGui/QPainter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QAbstractScrollArea>

namespace Med {
namespace QtGui {

class Buffer::Lines : public QWidget {
public:
  Lines(QWidget* parent, Buffer* buffer) : QWidget(parent), buffer(buffer) {
    // Not sure if this is needed. Seems like it should be but also to work without it.
    setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored));
  }
  
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(this);
    painter.drawText(rect(), Qt::AlignCenter, "Test\nString");
    QWidget::paintEvent(event);
  }
  
  Buffer* buffer;
};
  
class Buffer::ScrollArea : public QAbstractScrollArea {
public:
  ScrollArea(QWidget* parent, Buffer* buffer): QAbstractScrollArea(parent), buffer(buffer) {}

  void paintEvent(QPaintEvent* event) override {
    // Don't know why I have to call this explicitly.
    buffer->lines->paintEvent(event);
  }
  
  Buffer* buffer;
};

Buffer::Buffer(Editor::Buffer* buffer) : buffer(buffer) {
  scrollArea = new ScrollArea(this, this);
  lines = new Lines(scrollArea, this);
  scrollArea->setViewport(lines);
  QVBoxLayout* layout = new QVBoxLayout();
  layout->addWidget(scrollArea);
  setLayout(layout);
}

Buffer::~Buffer() {}

}  // namespace QtGui
}  // namespace Med

#include "Buffer.moc"
