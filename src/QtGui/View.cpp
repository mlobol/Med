#include "View.h"

#include <QtGui/QPainter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QScrollBar>

namespace Med {
namespace QtGui {

class View::Lines : public QWidget {
public:
  Lines(QWidget* parent, View* view) : QWidget(parent), view_(view) {
    // Not sure if this is needed. Seems like it should be but also to work without it.
    setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored));
    setTextFont({});
  }
  
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(this);
    QPoint textPos(0, textFontMetrics->leading() + textFontMetrics->height());
    for (Editor::Buffer::Line line : view_->view_->buffer()->iterateFromLineNumber(view_->view_->pageTopLineNumber())) {
      painter.drawText(textPos, *line.content);
      textPos.ry() += textFontMetrics->lineSpacing();
    }
    QWidget::paintEvent(event);
  }
  
  void setTextFont(const QFont& font) {
    textFont.reset(new QFont(font));
    textFontMetrics.reset(new QFontMetrics(*textFont));
  }
  
  int linesPerPage() {
    return height() / textFontMetrics->lineSpacing();
  }
  
  std::unique_ptr<QFont> textFont;
  std::unique_ptr<QFontMetrics> textFontMetrics;
  
  View* view_;
};
  
class View::ScrollArea : public QAbstractScrollArea {
public:
  ScrollArea(QWidget* parent, View* view): QAbstractScrollArea(parent), view_(view) {
    QObject::connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] (int value) {
      view_->view_->setPageTopLineNumber(value);
      if (view_->lines_) view_->lines_->update();
    });
  }

  void paintEvent(QPaintEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->paintEvent(event);
  }
  
  void resizeEvent(QResizeEvent* event) override {
    verticalScrollBar()->setPageStep(view_->lines_->linesPerPage());
    linesPerPageOrLineCountUpdated();
  }
  
  void linesPerPageOrLineCountUpdated() {
    verticalScrollBar()->setRange(1, std::max(0, view_->view_->buffer()->lineCount() - view_->lines_->linesPerPage()));
  }
  
  View* view_;
};

View::View(Editor::View* view) : view_(view) {
  scrollArea_ = new ScrollArea(this, this);
  lines_ = new Lines(scrollArea_, this);
  scrollArea_->setViewport(lines_);
  QVBoxLayout* layout = new QVBoxLayout();
  layout->addWidget(scrollArea_);
  setLayout(layout);
  scrollArea_->resizeEvent(nullptr);
}

View::~View() {}

}  // namespace QtGui
}  // namespace Med

#include "View.moc"
