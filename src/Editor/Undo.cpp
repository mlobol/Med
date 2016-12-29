#include "Undo.h"

#include "Buffer.h"

#include <QtCore/qglobal.h>

namespace Med {
namespace Editor {

class Undo::Op {
public:
  enum class Type {
    // Text between start_ and end_ was inserted_. lines_ is ignored.
    INSERTION,
    // Text starting at start_ was deleted. end_ is ignored. Deleted text is present in lines_.
    DELETION,
  };

  Op(OpType type, Buffer* originalBuffer) : type_(type), originalStart_(SafePoint::Content(), originalBuffer), originalEnd_(SafePoint::Content(), originalBuffer) {}

  const OpType type_;
  SafePoint originalStart_;
  SafePoint originalEnd_;
  std::unique_ptr<Buffer> undoBuffer_;
};

Undo::Undo(Buffer* buffer) : buffer_(buffer) {}
Undo::~Undo() {}

bool Undo::modified() const { return !unmodified_; }
void Undo::setUnmodified() {
  unmodified_ = true;
  opMakesUnmodified_ = nullptr;
}

Undo::Op* Undo::currentOp(RecordMode mode) {
  auto& ops = mode == RecordMode::UNDO ? opsToRedo_ : opsToUndo_;
  return ops.empty() ? nullptr : ops.back().get();
}

Undo::Op& Undo::newOp(RecordMode mode, OpType opType) {
  if (mode == RecordMode::NORMAL) opsToRedo_.clear();
  auto& ops = mode == RecordMode::UNDO ? opsToRedo_ : opsToUndo_;
  ops.push_back(std::make_unique<Op>(opType, buffer_));
  if (unmodified_) {
    unmodified_ = false;
    opMakesUnmodified_ = ops.back().get();
  }
  return *ops.back();
}

TempPoint Undo::deletionHandling(RecordMode mode, const Point& start, const Point& end) {
  if (Op* op = currentOp(mode)) {
    if (op->type_ == OpType::DELETION) {
      if (op->originalStart_.samePositionAs(start)) {
        // The new deletion is just after the previous one; extend the end.
        return TempPoint(op->undoBuffer_.get(), Point::BufferEnd());
      }
      if (op->originalStart_.samePositionAs(end)) {
        // The new deletion is just before the previous one; extend the start.
        return TempPoint(op->undoBuffer_.get(), Point::BufferStart());
      }
    }
  }
  Op& op = newOp(mode, OpType::DELETION);
  op.originalStart_.moveTo(start);
  op.undoBuffer_ = Buffer::create();
  op.undoBuffer_->insertLast();
  return TempPoint(op.undoBuffer_.get(), Point::BufferEnd());
}

void Undo::recordInsertion(RecordMode mode, const Point& start, const Point& end) {
  if (Op* op = currentOp(mode)) {
    if (op->type_ == OpType::INSERTION) {
      if (op->originalStart_.samePositionAs(start) || op->originalEnd_.samePositionAs(end)) {
        // originalStart_ or originalEnd_ has already been updated so the current undo op already covers the new insertion.
        return;
      }
      if (op->originalEnd_.samePositionAs(start)) {
        // The new insertion is just after the previous one; extend the end.
        op->originalEnd_.moveTo(end);
        return;
      }
      if (op->originalStart_.samePositionAs(end)) {
        // The new insertion is just before the previous one; extend the start.
        op->originalStart_.moveTo(start);
        return;
      }
    }
  }
  Op& op = newOp(mode, OpType::INSERTION);
  op.originalStart_.moveTo(start);
  op.originalEnd_.moveTo(end);
}

bool Undo::revertLast(RecordMode mode, Point* insertionPoint) {
  auto& ops = mode == RecordMode::UNDO ? opsToUndo_ : opsToRedo_;
  if (ops.empty()) return false;
  std::unique_ptr<Op> op = std::move(ops.back());
  ops.pop_back();
  Recorder recorder = {this, mode};
  switch (op->type_) {
    case OpType::INSERTION: {
      const bool ok = op->originalStart_.deleteTo(op->originalEnd_, recorder);
      if (!ok) return false;
      break;
    }
    case OpType::DELETION: {
      TempPoint start(op->originalStart_);
      TempPoint(op->undoBuffer_.get(), Point::BufferStart()).moveContentBefore(TempPoint(op->undoBuffer_.get(), Point::BufferEnd()), op->originalStart_);
      recordInsertion(mode, start, op->originalStart_);
      break;
    }
  }
  if (insertionPoint) insertionPoint->moveTo(op->originalStart_);
  if (opMakesUnmodified_ == op.get()) {
    unmodified_ = true;
    opMakesUnmodified_ = nullptr;
  }
  return true;
}

}  // namespace Editor
}  // namespace Med

