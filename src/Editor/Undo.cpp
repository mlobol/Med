#include "Undo.h"

#include "Buffer.h"

#include <QtCore/qglobal.h>

namespace Med {
namespace Editor {

class Undo::Op {
public:
  enum class Type { INSERTION, DELETION };

  Op(OpType type, Buffer* buffer) : type_(type), start_(buffer), end_(buffer) {}

  const OpType type_;
  SafePoint start_;
  SafePoint end_;
  std::vector<QString> lines_;
};

Undo::Op* Undo::currentOp(RecordMode mode) {
  auto& ops = mode == RecordMode::UNDO ? opsToRedo_ : opsToUndo_;
  return ops.empty() ? nullptr : ops.back().get();
}

Undo::Op& Undo::newOp(RecordMode mode, OpType opType) {
  if (mode == RecordMode::NORMAL) opsToRedo_.clear();
  auto& ops = mode == RecordMode::UNDO ? opsToRedo_ : opsToUndo_;
  ops.push_back(std::make_unique<Op>(opType, buffer_));
  return *ops.back();
}

std::pair<Undo::DeletionHandling, Undo::Op&> Undo::deletionHandling(RecordMode mode, const Point& start, const Point& end) {
  if (Op* op = currentOp(mode)) {
    if (op->type_ == OpType::DELETION) {
      if (op->start_.samePositionAs(start)) return { DeletionHandling::APPEND, *op };
      if (op->start_.samePositionAs(end)) return { DeletionHandling::PREPEND, *op };
    }
  }
  Op& op = newOp(mode, OpType::DELETION);
  op.start_.moveTo(start);
  return { DeletionHandling::NEW, op };
}

void Undo::recordInsertion(RecordMode mode, const Point& start, const Point& end) {
  if (Op* op = currentOp(mode)) {
    if (op->type_ == OpType::INSERTION) {
      if (op->start_.samePositionAs(start) || op->end_.samePositionAs(end))         return;
      if (op->end_.samePositionAs(start)) {
        op->end_.moveTo(end);
        return;
      }
      if (op->start_.samePositionAs(end)) {
        op->start_.moveTo(start);
        return;
      }
    }
  }
  Op& op = newOp(mode, OpType::INSERTION);
  op.start_.moveTo(start);
  op.end_.moveTo(end);
}

void Undo::recordPartialLineDeletion(RecordMode mode, const Point& start, const Point& end, QStringRef content) {
  auto handling = deletionHandling(mode, start, end);
  switch (handling.first) {
    case DeletionHandling::PREPEND:
      // TODO: remove toString() after QString::prepend() supports QStringRef
      handling.second.lines_.front().prepend(content.toString());
      return;
    case DeletionHandling::APPEND:
      handling.second.lines_.back().append(content);
      return;
    case DeletionHandling::NEW:
      handling.second.lines_.push_back(content.toString());
      return;
  }
}

void Undo::recordMultilineDeletion(RecordMode mode, const Point& start, const Point& end, QStringRef firstLineDeleted, std::vector<QString> linesDeleted, QStringRef lastLineDeleted) {
  auto handling = deletionHandling(mode, start, end);
  switch (handling.first) {
    case DeletionHandling::PREPEND:
      // TODO: remove toString() after QString::prepend() supports QStringRef
      handling.second.lines_.front().prepend(lastLineDeleted.toString());
      std::move(linesDeleted.begin(), linesDeleted.end(), std::inserter(handling.second.lines_, handling.second.lines_.begin()));
      handling.second.lines_.insert(handling.second.lines_.begin(),
                                    firstLineDeleted.toString());
      return;
    case DeletionHandling::APPEND:
      handling.second.lines_.back().append(firstLineDeleted);
      std::move(linesDeleted.begin(), linesDeleted.end(), std::back_inserter(handling.second.lines_));
      handling.second.lines_.push_back(lastLineDeleted.toString());
      return;
    case DeletionHandling::NEW:
      handling.second.lines_.push_back(firstLineDeleted.toString());
      std::move(linesDeleted.begin(), linesDeleted.end(), std::back_inserter(handling.second.lines_));
      handling.second.lines_.push_back(lastLineDeleted.toString());
      return;
  }
}

bool Undo::revertLast(RecordMode mode, Point* insertionPoint) {
  auto& ops = mode == RecordMode::UNDO ? opsToUndo_ : opsToRedo_;
  if (ops.empty()) return false;
  std::unique_ptr<Op> op = std::move(ops.back());
  ops.pop_back();
  Recorder recorder = {this, mode};
  switch (op->type_) {
    case OpType::INSERTION: {
      const bool ok = op->start_.deleteTo(&op->end_, recorder);
      if (ok && insertionPoint) insertionPoint->moveTo(op->start_);
      return ok;
    }
    case OpType::DELETION: {
      const bool ok = op->lines_.size() >= 2
      ? op->start_.insertBefore(&op->lines_.front(), op->lines_.begin() + 1, op->lines_.end() - 1, &op->lines_.back(), recorder)
      : op->start_.insertBefore(&op->lines_.front(), recorder);
      if (ok && insertionPoint) insertionPoint->moveTo(op->start_);
      return ok;
    }
  }
}

Undo::Undo(Buffer* buffer) : buffer_(buffer) {}
Undo::~Undo() {}

}  // namespace Editor
}  // namespace Med

