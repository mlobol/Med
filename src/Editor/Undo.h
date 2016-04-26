#ifndef MED_EDITOR_UNDO_H
#define MED_EDITOR_UNDO_H

#include "Editor/Buffer.h"

namespace Med {
namespace Editor {

class UndoOps {
public:
  UndoOps(Buffer* buffer, UndoOps* otherOps, bool clearOtherOps);
  ~UndoOps();

  // Reverts the last recorded op.
  bool revertLast(Buffer::Point* insertionPoint);
  // Clears the recorded ops.
  void clear();
  // Called after insertion.
  void recordInsertion(const Buffer::Point& start, const Buffer::Point& end);
  // Called before points are moved and the line content is modified (so that the ref is still valid).
  void recordPartialLineDeletion(const Buffer::Point& start, const Buffer::Point& end, QStringRef content);
  // Called after all lines but the first have been extracted from the buffer, but before the first line is actually modified (so that the refs are still valid).
  void recordMultilineDeletion(const Buffer::Point& start, const Buffer::Point& end, QStringRef firstLineDeleted, std::vector<QString> linesDeleted, QStringRef lastLineDeleted);

private:
  class Op;
  enum class OpType { INSERTION, DELETION };

  Op* newOp(OpType opType);

  enum class DeletionHandling { PREPEND, APPEND, NEW };
  std::pair<DeletionHandling, Op*> deletionHandling(const Buffer::Point& start, const Buffer::Point& end);

  Buffer* const buffer_;
  std::vector<std::unique_ptr<Op>> ops_;
  UndoOps* const otherOps_;
  const bool clearOtherOps_;
};

class Undo {
public:
  Undo(Buffer* buffer);
  ~Undo();

  // Undoes the last operation. Returns false if there were no operations to undo, or the undoing failed for some other reason.
  bool undo(Buffer::Point* insertionPoint) { return opsToUndo_.revertLast(insertionPoint); }
  bool redo(Buffer::Point* insertionPoint) { return opsToRedo_.revertLast(insertionPoint); }

  UndoOps* opsToUndo() { return &opsToUndo_; }

private:
  UndoOps opsToUndo_;
  UndoOps opsToRedo_;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_UNDO_H
