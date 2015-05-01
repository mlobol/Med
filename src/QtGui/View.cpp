#include "View.h"

#include <QtCore/QEvent>
#include <QtCore/QTimer>
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
    cursorBlinkingTimer_ = new QTimer(this);
    QObject::connect(cursorBlinkingTimer_, &QTimer::timeout, this, [this] () {
      cursorOn_ = !cursorOn_;
      update();
    });
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
    Editor::Buffer::Point insertionPoint = view_->view_->insertionPoint()->point();
    int lineNumber = view_->view_->pageTopLineNumber() - 1;
    for (auto& layout : page_) {
      ++lineNumber;
      layout->draw(&painter, {0, 0});
      if (cursorOn_ && lineNumber == insertionPoint.lineNumber && hasFocus()) {
        layout->drawCursor(&painter, {0, 0}, insertionPoint.columnNumber, 2);
      }
    }
    QWidget::paintEvent(event);
  }
  
  void keyPressEvent(QKeyEvent* event) override {
    QString text = event->text();
    if (!text.isEmpty()) {
      if (!view_->view_->insert(text)) return;
      // TODO: add helper method to get layout for a given line and update only that one.
      resetPage();
      handleVisibleUpdate();
    }
    QWidget::keyPressEvent(event);
  }
  
  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      int lineNumber = view_->view_->pageTopLineNumber() - 1;
      for (auto& layout : page_) {
        ++lineNumber;
        // TODO: this should only consider y coordinates so that clicking after the end of the line puts the cursor at the end of the line.
        // Also, clicking after the last line should put the cursor in the last line.
        if (layout->boundingRect().contains(event->pos())) {
          int columnNumber = layout->lineAt(0).xToCursor(event->x());
          view_->view_->insertionPoint()->setPoint({lineNumber, columnNumber});
          handleVisibleUpdate();
          break;
        }
      }
    }
  }
  
  void handleVisibleUpdate() {
    update();
    if (!hasFocus()) return;
    // When something changes in the view we start a new blink of the cursor so that it's obvious where it is.
    cursorOn_ = true;
    cursorBlinkingTimer_->start(500);
  }
  
  void focusInEvent(QFocusEvent* event) override { handleVisibleUpdate(); }
  
  void focusOutEvent(QFocusEvent* event) override {
    cursorBlinkingTimer_->stop();
    // The update will hide the cursor.
    update();
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

  bool cursorOn_ = false;
  QTimer* cursorBlinkingTimer_ = nullptr;
  
  View* view_ = nullptr;
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
  
  void keyPressEvent(QKeyEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->keyPressEvent(event);
  }
  
  void mousePressEvent(QMouseEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->mousePressEvent(event);
  }
  
  void focusInEvent(QFocusEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->focusInEvent(event);
  }
  
  void focusOutEvent(QFocusEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->focusOutEvent(event);
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
