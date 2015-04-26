#include "View.h"

#include <QtCore/QEvent>
#include <QtGui/QPainter>
#include <QtGui/QTextLine>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

namespace Med {
namespace QtGui {

class View::Lines : public QWidget {
public:
  Lines(QWidget* parent, View* view) : QWidget(parent), view_(view) {
    // Not sure if this is needed. Seems like it should be but also to work without it.
    setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored));
    setFocusPolicy(Qt::StrongFocus);
    setTextFont({});
  }
  
  bool event(QEvent* event) override {
    if (event->type() == QEvent::KeyPress) {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      if (keyEvent->key() == Qt::Key_Tab) {
        keyPressEvent(keyEvent);
        return true;
      }
    }
    return QWidget::event(event);
  }
  
  void resetPage() {
    const int leading = textFontMetrics_->leading();
    QPointF linePos(0, 0);
    page_.clear();
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::NoWrap);
    for (Editor::Buffer::Line line : view_->view_->buffer()->iterateFromLineNumber(view_->view_->pageTopLineNumber())) {
      page_.emplace_back(new QTextLayout(*line.content, *textFont_));
      QTextLayout* layout = page_.back().get();
      layout->setTextOption(textOption);
      layout->setCacheEnabled(true);
      layout->beginLayout();
      QTextLine layoutLine = layout->createLine();
      layoutLine.setLineWidth(line.content->size());
      linePos.ry() += leading;
      layoutLine.setPosition(linePos);
      layout->endLayout();
      linePos.ry() += layoutLine.height();
      if (linePos.y() >= height()) break;
    }
  }
  
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(this);
    Editor::Buffer::Point insertionPoint = view_->view_->insertionPoint();
    int lineNumber = view_->view_->pageTopLineNumber() - 1;
    for (auto& layout : page_) {
      ++lineNumber;
      layout->draw(&painter, {0, 0});
      if (lineNumber == insertionPoint.lineNumber) {
        layout->drawCursor(&painter, {0, 0}, insertionPoint.columnNumber);
      }
    }
    QWidget::paintEvent(event);
  }
  
  void keyPressEvent(QKeyEvent* event) override {
  }
  
  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      int lineNumber = view_->view_->pageTopLineNumber() - 1;
      for (auto& layout : page_) {
        ++lineNumber;
        if (layout->boundingRect().contains(event->pos())) {
          int columnNumber = layout->lineAt(0).xToCursor(event->x());
          view_->view_->setInsertionPoint({lineNumber, columnNumber});
          update();
          break;
        }
      }
    }
  }
  
  void setTextFont(const QFont& font) {
    textFont_.reset(new QFont(font));
    textFontMetrics_.reset(new QFontMetrics(*textFont_));
  }
  
  int linesPerPage() {
    return height() / textFontMetrics_->lineSpacing();
  }
  
  std::unique_ptr<QFont> textFont_;
  std::unique_ptr<QFontMetrics> textFontMetrics_;
  std::vector<std::unique_ptr<QTextLayout>> page_;
  
  View* view_;
};
  
class View::ScrollArea : public QAbstractScrollArea {
public:
  ScrollArea(QWidget* parent, View* view): QAbstractScrollArea(parent), view_(view) {
    QObject::connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] (int value) {
      view_->view_->setPageTopLineNumber(value);
      if (view_->lines_) view_->lines_->resetPage();
    });
  }

  void paintEvent(QPaintEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->paintEvent(event);
  }
  
  void mousePressEvent(QMouseEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->mousePressEvent(event);
  }
  
  void resizeEvent(QResizeEvent* event) override {
    verticalScrollBar()->setPageStep(view_->lines_->linesPerPage());
    linesPerPageOrLineCountUpdated();
  }
  
  void linesPerPageOrLineCountUpdated() {
    verticalScrollBar()->setRange(1, std::max(0, view_->view_->buffer()->lineCount() - view_->lines_->linesPerPage()));
    if (view_->lines_) view_->lines_->resetPage();
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
