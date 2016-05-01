#include "View.h"

#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QClipboard>
#include <QtGui/QPainter>
#include <QtGui/QTextLine>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QApplication>
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
    setStyleSheet("background: white");
    cursorBlinkingTimer_ = new QTimer(this);
    QObject::connect(cursorBlinkingTimer_, &QTimer::timeout, this, [this] () {
      cursorOn_ = !cursorOn_;
      update(cursorBounds_);
    });
  }

  Editor::SafePoint& insertionPoint() { return view_->view_->insertionPoint_; }
  Editor::SafePoint& selectionPoint() { return view_->view_->selectionPoint_; }
  Editor::SafePoint& pageTop() { return view_->view_->pageTop_; }
  Editor::Undo* undo() { return &view_->view_->undo_; }
  Editor::Undo::Recorder recorder() { return undo()->recorder(); }

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
    page_.clear();
    if (!pageTop().isValid()) return;
    const int leading = textFontMetrics_->leading();
    QPointF linePos(0, 0);
    int top = 0;
    cursorBounds_ = {};
    for (const QString* lineContent : pageTop().linesForwards()) {
      page_.emplace_back();
      Line& line = page_.back();
      line.layout.reset(new QTextLayout(*lineContent, *textFont_));
      top += leading;
      updateLayout(line.layout.get(), top);
      if (lineContent == &insertionPoint().lineContent()) updateCursorBounds(line.layout.get());
      top = line.layout->boundingRect().bottom();
      if (top >= height()) break;
    }
    const int pageTopLineNumber = pageTop().lineNumber();
    updateSelection(pageTopLineNumber, pageTopLineNumber, pageTopLineNumber + page_.size());
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
    for (Line& line : page_) {
      if (line.selectionContinuesAfterEnd) {
        QRect selection = layoutBounds(line.layout.get());
        QTextLine textLine = line.layout->lineAt(0);
        selection.setLeft(textLine.cursorToX(textLine.width() - 1));
        painter.fillRect(selection, Qt::darkBlue);
      }
      line.layout->draw(&painter, {0, 0}, line.selections);
    }
    if (cursorOn_ && hasFocus() && insertionPoint().isValid()) {
      QTextLayout* insertionPointLayout = layoutForLineNumber(insertionPoint().lineNumber());
      if (insertionPointLayout) {
        insertionPointLayout->drawCursor(&painter, {0, 0}, insertionPoint().columnNumber(), 2);
      }
    }
    QWidget::paintEvent(event);
  }

  QTextLayout* layoutForLineNumber(int lineNumber) {
    int layoutIndex = lineNumber - pageTop().lineNumber();
    if (layoutIndex < 0 || layoutIndex >= page_.size()) return nullptr;
    return page_.at(layoutIndex).layout.get();
  }

  void updateCursorBounds(QTextLayout* layoutForInsertionPoint) {
    cursorBounds_ = layoutForInsertionPoint->boundingRect().toAlignedRect();
    cursorBounds_.setLeft(layoutForInsertionPoint->lineAt(0).cursorToX(
      insertionPoint().columnNumber()) - 1);
    // Needs to be two larger than the width passed to drawCursor to account for rounding.
    cursorBounds_.setWidth(4);
  }

  void updateAfterVisibleChange(QRect bounds) {
    update(bounds);
    if (!hasFocus()) return;
    // When something changes in the view we start a new blink of the cursor so that it's obvious where it is.
    cursorOn_ = true;
    cursorBlinkingTimer_->start(500);
  }

  QRect layoutBounds(QTextLayout* layout) {
    QRect bounds = layout->boundingRect().toAlignedRect();
    bounds.setLeft(0);
    bounds.setRight(width());
    return bounds;
  }

  void updateAfterInsertionLineModified() {
    if (!insertionPoint().isValid()) return;
    // TODO: delete everything between selectionPoint and insertionPoint
    QTextLayout* layout = layoutForLineNumber(insertionPoint().lineNumber());
    if (!layout) return;
    int oldHeight = layout->boundingRect().height();
    int top = layout->boundingRect().top();
    layout->setText(insertionPoint().lineContent());
    updateLayout(layout, top);
    if (oldHeight == layout->boundingRect().height()) {
      updateCursorBounds(layout);
      updateAfterVisibleChange(layoutBounds(layout));
    } else {
      // Line height changed, fall back to resetting page.
      resetPage();
      updateAfterVisibleChange(rect());
    }
  }

  void updateSelection(int pageTopLineNumber, int updateStartLineNumber, int updateEndLineNumber) {
    const bool hasSelection = insertionPoint().isValid() && selectionPoint().isValid();
    const int insertionPointLineNumber = hasSelection ? insertionPoint().lineNumber() : -1;
    const int selectionPointLineNumber = hasSelection ? selectionPoint().lineNumber() : -1;
    Editor::Point* selectionStart = nullptr;
    Editor::Point* selectionEnd = nullptr;
    if (hasSelection) Editor::Point::sortPair(&insertionPoint(), &selectionPoint(), &selectionStart, &selectionEnd);
    const int selectionStartLineNumber = hasSelection ? selectionStart->lineNumber() : -1;
    const int selectionEndLineNumber = hasSelection ? selectionEnd->lineNumber() : -1;
    for (int lineNumber = std::max(updateStartLineNumber, pageTopLineNumber);
          lineNumber <= std::min<int>(updateEndLineNumber, pageTopLineNumber + page_.size() - 1);
          ++lineNumber) {
      Line& line = page_[lineNumber - pageTopLineNumber];
      if (hasSelection && lineNumber >= selectionStartLineNumber && lineNumber <= selectionEndLineNumber) {
        const int start = lineNumber == selectionStartLineNumber ? selectionStart->columnNumber() : 0;
        const int end = lineNumber == selectionEndLineNumber ? selectionEnd->columnNumber() : line.layout->lineAt(0).textLength();
        QTextLayout::FormatRange range;
        range.format.setForeground(Qt::gray);
        range.format.setBackground(Qt::darkBlue);
        range.start = start;
        range.length = end - start;
        line.selections = {range};
        line.selectionContinuesAfterEnd = lineNumber < selectionEndLineNumber;
      } else {
        line.selections.clear();
        line.selectionContinuesAfterEnd = false;
      }
      update(layoutBounds(line.layout.get()));
    }
  }

  void handleCursorMove(bool extendSelection, std::function<bool()> move) {
    int selectionUpdateOneBoundLineNumber = -1;
    int selectionUpdateOtherBoundLineNumber = -1;
    if (insertionPoint().isValid()) {
      if (extendSelection) {
        if (!selectionPoint().isValid()) selectionPoint().moveTo(insertionPoint());
        selectionUpdateOneBoundLineNumber = insertionPoint().lineNumber();
      } else if (selectionPoint().isValid()) {
        selectionUpdateOneBoundLineNumber = insertionPoint().lineNumber();
        selectionUpdateOtherBoundLineNumber = selectionPoint().lineNumber();
        selectionPoint().reset();
      }
    }
    if (move()) {
      update(cursorBounds_);
      const int insertionPointLineNumber = insertionPoint().lineNumber();
      QTextLayout* layoutForInsertionPoint = layoutForLineNumber(insertionPointLineNumber);
      if (layoutForInsertionPoint) updateCursorBounds(layoutForInsertionPoint);
      updateAfterVisibleChange(cursorBounds_);
      if (selectionUpdateOneBoundLineNumber >= 0 && selectionUpdateOtherBoundLineNumber < 0) selectionUpdateOtherBoundLineNumber = insertionPointLineNumber;
    }
    if (selectionUpdateOtherBoundLineNumber >= 0) {
      const int selectionUpdateStartLineNumber = std::min(selectionUpdateOneBoundLineNumber, selectionUpdateOtherBoundLineNumber);
      const int selectionUpdateEndLineNumber = std::max(selectionUpdateOneBoundLineNumber, selectionUpdateOtherBoundLineNumber);
      updateSelection(pageTop().lineNumber(), selectionUpdateStartLineNumber, selectionUpdateEndLineNumber);
    }
  }

  void handleKeyCursorMove(QKeyEvent* event, std::function<bool()> move) {
    handleCursorMove(event->modifiers() & Qt::ShiftModifier, move);
  }

  void updateAfterLineInsertedOrDeleted() {
    resetPage();
    updateAfterVisibleChange(rect());
  }

  void handleKeyContentChange(bool canInsertOrDeleteLines, bool deleteSelection, std::function<bool()> change) {
    if (!insertionPoint().isValid()) return;
    if (selectionPoint().isValid()) {
      if (deleteSelection) {
        selectionPoint().deleteTo(&insertionPoint(), recorder());
        canInsertOrDeleteLines = true;
      }
      selectionPoint().reset();
    }
    if (change()) {
      if (canInsertOrDeleteLines) {
        updateAfterLineInsertedOrDeleted();
      } else {
        updateAfterInsertionLineModified();
      }
    }
  }

  void keyPressEvent(QKeyEvent* event) override {
    switch (event->key()) {
      // Moving the cursor.
      case Qt::Key_Left:
        handleKeyCursorMove(event, [this]() { return insertionPoint().moveLeft(); });
        return;
      case Qt::Key_Right:
        handleKeyCursorMove(event, [this]() { return insertionPoint().moveRight(); });
        return;
      case Qt::Key_Up:
        handleKeyCursorMove(event, [this]() { return insertionPoint().moveUp(); });
        return;
      case Qt::Key_Down:
        handleKeyCursorMove(event, [this]() { return insertionPoint().moveDown(); });
        return;
      case Qt::Key_Home:
        handleKeyCursorMove(event, [this]() { return insertionPoint().moveToLineStart(); });
        return;
      case Qt::Key_End:
        handleKeyCursorMove(event, [this]() { return insertionPoint().moveToLineEnd(); });
        return;
      // Content changes that may insert or delete lines.
      case Qt::Key_Return:
        handleKeyContentChange(true, true, [this]() { return insertionPoint().insertLineBreakBefore(recorder()); });
        return;
      case Qt::Key_Backspace:
        // TODO: optimize case where no lines inserted or deleted
        handleKeyContentChange(true, true, [this]() { return insertionPoint().deleteCharBefore(recorder()); });
        return;
      case Qt::Key_Delete:
        // TODO: optimize case where no lines inserted or deleted
        handleKeyContentChange(true, true, [this]() { return insertionPoint().deleteCharAfter(recorder()); });
        return;
      // Content changes that may not insert or delete lines.
      default:
        QString text = event->text();
        if (!text.isEmpty()) {
          handleKeyContentChange(false, true, [this, &text]() { return insertionPoint().insertBefore(&text, recorder()); });
          return;
        }
    }
    QWidget::keyPressEvent(event);
  }

  void handleMouseMoveWithButtonPressed(QMouseEvent* event) {
    handleCursorMove(mouseExtendingSelection_, [this, event]() {
      int lineNumber = pageTop().lineNumber() - 1;
      QTextLayout* layoutForClick = nullptr;
      for (Line& line : page_) {
        ++lineNumber;
        layoutForClick = line.layout.get();
        if (line.layout->boundingRect().bottom() >= event->y()) break;
      }
      if (!layoutForClick) return false;
      insertionPoint().setLineNumber(lineNumber);
      insertionPoint().setColumnNumber(layoutForClick->lineAt(0).xToCursor(event->x()));
      return true;
    });
    mouseExtendingSelection_ = true;
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    if (event->buttons() & Qt::LeftButton) {
      handleMouseMoveWithButtonPressed(event);
    }
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      handleMouseMoveWithButtonPressed(event);
    }
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    mouseExtendingSelection_ = false;
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

  void copyToClipboard() {
    // TODO: use custom MIME type so that data is only converted to text on demand when pasting.
    QString text;
    if (!insertionPoint().contentTo(selectionPoint(), &text)) return;
    QApplication::clipboard()->setText(text);
  }

  void pasteFromClipboard() {
    // TODO: implement more efficient paste when pasting from the same process.
    handleKeyContentChange(true, true, [this]() {
      return insertionPoint().insertBefore(QApplication::clipboard()->text().splitRef('\n').toStdVector(), recorder());
    });
  }

  struct Line {
    std::unique_ptr<QTextLayout> layout;
    QVector<QTextLayout::FormatRange> selections;
    bool selectionContinuesAfterEnd = false;
  };

  std::unique_ptr<QFont> textFont_;
  std::unique_ptr<QFontMetrics> textFontMetrics_;
  std::vector<Line> page_;

  bool cursorOn_ = false;
  QTimer* cursorBlinkingTimer_ = nullptr;
  QRect cursorBounds_;

  bool mouseExtendingSelection_ = false;

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

  void mouseMoveEvent(QMouseEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->mouseMoveEvent(event);
  }

  void mousePressEvent(QMouseEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->mousePressEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    // Don't know why I have to call this explicitly.
    view_->lines_->mouseReleaseEvent(event);
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

void View::copyToClipboard() { lines_->copyToClipboard(); }
void View::pasteFromClipboard() { lines_->pasteFromClipboard(); }
void View::undo() {
  lines_->handleKeyContentChange(true, false, [this]() { return view_->undo_.undo(&lines_->insertionPoint()); });
}
void View::redo() {
    lines_->handleKeyContentChange(true, false, [this]() { return view_->undo_.redo(&lines_->insertionPoint()); });
}
bool View::save() {
  return view_->buffer()->save();
}

}  // namespace QtGui
}  // namespace Med

#include "View.moc"
