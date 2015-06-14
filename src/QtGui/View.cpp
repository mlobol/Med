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
      update(cursorBounds_);
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
    int top = 0;
    cursorBounds_ = {};
    for (const QString* lineContent : view_->view_->pageTop_.linesForwards()) {
      page_.emplace_back(new QTextLayout(*lineContent, *textFont_));
      QTextLayout* layout = page_.back().get();
      top += leading;
      updateLayout(layout, top);
      if (lineContent == &insertionPoint().lineContent()) updateCursorBounds(layout);
      top = layout->boundingRect().bottom();
      if (top >= height()) break;
    }
  }

  void updateLayout(QTextLayout* layout, int top) {
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::NoWrap);
    layout->setTextOption(textOption);
    layout->setCacheEnabled(true);
    layout->beginLayout();
    QTextLine layoutLine = layout->createLine();
    layoutLine.setLineWidth(layout->text().size());
    layoutLine.setPosition(QPointF(0, top));
    layout->endLayout();
  }

  void paintEvent(QPaintEvent* event) override {
    QPainter painter(this);
    for (auto& layout : page_) layout->draw(&painter, {0, 0});
    if (cursorOn_ && hasFocus()) {
      QTextLayout* insertionPointLayout = layoutForLineNumber(insertionPoint().lineNumber());
      if (insertionPointLayout) {
        insertionPointLayout->drawCursor(&painter, {0, 0}, insertionPoint().columnNumber(), 2);
      }
    }
    QWidget::paintEvent(event);
  }

  Editor::Buffer::Point& insertionPoint() { return view_->view_->insertionPoint_; }

  QTextLayout* layoutForLineNumber(int lineNumber) {
    int layoutIndex = lineNumber - view_->view_->pageTop_.lineNumber();
    if (layoutIndex < 0 || layoutIndex >= page_.size()) return nullptr;
    return page_.at(layoutIndex).get();
  }

  void updateCursorBounds(QTextLayout* layoutForInsertionPoint) {
    cursorBounds_ = layoutForInsertionPoint->boundingRect().toAlignedRect();
    cursorBounds_.setLeft(layoutForInsertionPoint->lineAt(0).cursorToX(
      insertionPoint().columnNumber()) - 1);
    // Needs to be one larger than the width passed to drawCursor to account for rounding.
    cursorBounds_.setWidth(3);
  }

  void updateAfterVisibleChange(QRect bounds) {
    update(bounds);
    if (!hasFocus()) return;
    // When something changes in the view we start a new blink of the cursor so that it's obvious where it is.
    cursorOn_ = true;
    cursorBlinkingTimer_->start(500);
  }

  void updateAfterInsertionLineModified() {
    if (!insertionPoint().isValid()) return;
    QTextLayout* layout = layoutForLineNumber(insertionPoint().lineNumber());
    if (!layout) return;
    int oldHeight = layout->boundingRect().height();
    int top = layout->boundingRect().top();
    layout->setText(insertionPoint().lineContent());
    updateLayout(layout, top);
    if (oldHeight == layout->boundingRect().height()) {
      updateCursorBounds(layout);
      QRect bounds = layout->boundingRect().toAlignedRect();
      bounds.setLeft(0);
      bounds.setRight(width());
      updateAfterVisibleChange(bounds);
    } else {
      // Line height changed, fall back to resetting page.
      resetPage();
      updateAfterVisibleChange(rect());
    }
  }

  void updateAfterCursorMoved() {
    update(cursorBounds_);
    QTextLayout* layoutForInsertionPoint = layoutForLineNumber(insertionPoint().lineNumber());
    if (layoutForInsertionPoint) updateCursorBounds(layoutForInsertionPoint);
    updateAfterVisibleChange(cursorBounds_);
  }

  void updateAfterLineInsertedOrDeleted() {
    resetPage();
    updateAfterVisibleChange(rect());
  }

  void keyPressEvent(QKeyEvent* event) override {
    switch (event->key()) {
      // Moving the cursor.
      case Qt::Key_Left:
        if (insertionPoint().moveLeft()) updateAfterCursorMoved();
        return;
      case Qt::Key_Right:
        if (insertionPoint().moveRight()) updateAfterCursorMoved();
        return;
      case Qt::Key_Up:
        if (insertionPoint().moveUp()) updateAfterCursorMoved();
        return;
      case Qt::Key_Down:
        if (insertionPoint().moveDown()) updateAfterCursorMoved();
        return;
      case Qt::Key_Home:
        if (insertionPoint().moveToLineStart()) updateAfterCursorMoved();
        return;
      case Qt::Key_End:
        if (insertionPoint().moveToLineEnd()) updateAfterCursorMoved();
        return;
      // Content changes that may insert or delete lines.
      case Qt::Key_Return:
        if (insertionPoint().insertLineBreakBefore()) updateAfterLineInsertedOrDeleted();
        return;
      case Qt::Key_Backspace:
        // TODO: optimize case where no lines inserted or deleted
        if (insertionPoint().deleteCharBefore()) updateAfterLineInsertedOrDeleted();
        return;
      case Qt::Key_Delete:
        // TODO: optimize case where no lines inserted or deleted
        if (insertionPoint().deleteCharAfter()) updateAfterLineInsertedOrDeleted();
        return;
      // Content changes that may not insert or delete lines.
      default:
        QString text = event->text();
        if (!text.isEmpty()) {
          if (insertionPoint().insertBefore(text)) updateAfterInsertionLineModified();
          return;
        }
    }
    QWidget::keyPressEvent(event);
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      int lineNumber = view_->view_->pageTop_.lineNumber() - 1;
      QTextLayout* layoutForClick = nullptr;
      for (auto& layout : page_) {
        ++lineNumber;
        layoutForClick = layout.get();
        if (layout->boundingRect().bottom() >= event->y()) break;
      }
      if (!layoutForClick) return;
      insertionPoint().setLineNumber(lineNumber);
      insertionPoint().setColumnNumber(layoutForClick->lineAt(0).xToCursor(event->x()));
      updateAfterCursorMoved();
    }
  }

  void focusInEvent(QFocusEvent* event) override {
    updateAfterVisibleChange(cursorBounds_);
  }

  void focusOutEvent(QFocusEvent* event) override {
    cursorBlinkingTimer_->stop();
    // The update will hide the cursor.
    update(cursorBounds_);
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
  QRect cursorBounds_;

  View* view_ = nullptr;
};

class View::ScrollArea : public QAbstractScrollArea {
public:
  ScrollArea(QWidget* parent, View* view): QAbstractScrollArea(parent), view_(view) {
    QObject::connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] (int value) {
      view_->view_->pageTop_.setLineNumber(value);
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
