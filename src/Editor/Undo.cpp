#include "Undo.h"

#include <QtCore/qglobal.h>

namespace Med {
namespace Editor {

class UndoOps::Op {
public:
  enum class Type { INSERTION, DELETION };

  Op(OpType type, Buffer* buffer) : type_(type), start_(buffer), end_(buffer) {}

  const OpType type_;
  Buffer::SafePoint start_;
  Buffer::SafePoint end_;
  std::vector<QString> lines_;
};

UndoOps::UndoOps(Buffer* buffer, UndoOps* otherOps, bool clearOtherOps) : buffer_(buffer), otherOps_(otherOps), clearOtherOps_(clearOtherOps) {}
UndoOps::~UndoOps() {}

void UndoOps::clear() { ops_.clear(); }

UndoOps::Op* UndoOps::newOp(OpType opType) {
  if (clearOtherOps_) otherOps_->clear();
  ops_.push_back(std::make_unique<Op>(opType, buffer_));
  return ops_.back().get();
}

void UndoOps::recordInsertion(const Buffer::Point& start, const Buffer::Point& end) {
  if (!ops_.empty()) {
    Op& currentOp = *ops_.back();
    if (currentOp.type_ == OpType::INSERTION) {
      if (currentOp.start_.samePositionAs(start) ||currentOp.end_.samePositionAs(end)) {
        return;
      }
      if (currentOp.end_.samePositionAs(start)) {
        currentOp.end_.moveTo(end);
        return;
      }
      if (currentOp.start_.samePositionAs(end)) {
        currentOp.start_.moveTo(start);
        return;
      }
    }
  }
  Op* op = newOp(OpType::INSERTION);
  op->start_.moveTo(start);
  op->end_.moveTo(end);
}

void UndoOps::recordPartialLineDeletion(const Buffer::Point& start, const Buffer::Point& end, QStringRef content) {
  auto handling = deletionHandling(start, end);
  switch (handling.first) {
    case DeletionHandling::PREPEND:
      // TODO: remove toString() after QString::prepend() supports QStringRef
      handling.second->lines_.front().prepend(content.toString());
      return;
    case DeletionHandling::APPEND:
      handling.second->lines_.back().append(content);
      return;
    case DeletionHandling::NEW:
      handling.second->lines_.push_back(content.toString());
      return;
  }
}

void UndoOps::recordMultilineDeletion(const Buffer::Point& start, const Buffer::Point& end, QStringRef firstLineDeleted, std::vector<QString> linesDeleted, QStringRef lastLineDeleted) {
  auto handling = deletionHandling(start, end);
  switch (handling.first) {
    case DeletionHandling::PREPEND:
      // TODO: remove toString() after QString::prepend() supports QStringRef
      handling.second->lines_.front().prepend(lastLineDeleted.toString());
      std::move(linesDeleted.begin(), linesDeleted.end(), std::inserter(handling.second->lines_, handling.second->lines_.begin()));
      handling.second->lines_.insert(handling.second->lines_.begin(),
                                    firstLineDeleted.toString());
      return;
    case DeletionHandling::APPEND:
      handling.second->lines_.back().append(firstLineDeleted);
      std::move(linesDeleted.begin(), linesDeleted.end(), std::back_inserter(handling.second->lines_));
      handling.second->lines_.push_back(lastLineDeleted.toString());
      return;
    case DeletionHandling::NEW:
      handling.second->lines_.push_back(firstLineDeleted.toString());
      std::move(linesDeleted.begin(), linesDeleted.end(), std::back_inserter(handling.second->lines_));
      handling.second->lines_.push_back(lastLineDeleted.toString());
      return;
  }
}

std::pair<UndoOps::DeletionHandling, UndoOps::Op*> UndoOps::deletionHandling(const Buffer::Point& start, const Buffer::Point& end) {
  if (!ops_.empty()) {
    Op* currentOp = ops_.back().get();
    if (currentOp->type_ == OpType::DELETION) {
      if (currentOp->start_.samePositionAs(start)) return { DeletionHandling::APPEND, currentOp };
      if (currentOp->start_.samePositionAs(end)) return { DeletionHandling::PREPEND, currentOp };
    }
  }
  Op* op = newOp(OpType::DELETION);
  op->start_.moveTo(start);
  return { DeletionHandling::NEW, op };
}

bool UndoOps::revertLast(Buffer::Point* insertionPoint) {
  if (ops_.empty()) return false;
  std::unique_ptr<Op> op = std::move(ops_.back());
  ops_.pop_back();
  switch (op->type_) {
    case OpType::INSERTION: {
      const bool ok = op->start_.deleteTo(&op->end_, otherOps_);
      if (ok && insertionPoint) insertionPoint->moveTo(op->start_);
      return ok;
    }
    case OpType::DELETION: {
      const bool ok = op->lines_.size() >= 2
      ? op->start_.insertBefore(&op->lines_.front(), op->lines_.begin() + 1, op->lines_.end() - 1, &op->lines_.back(), otherOps_)
      : op->start_.insertBefore(&op->lines_.front(), otherOps_);
      if (ok && insertionPoint) insertionPoint->moveTo(op->start_);
      return ok;
    }
  }
}

Undo::Undo(Buffer* buffer) : opsToUndo_(buffer, &opsToRedo_, true), opsToRedo_(buffer, &opsToUndo_, false) {}
Undo::~Undo() {}

}  // namespace Editor
}  // namespace Med

