#ifndef MED_EDITOR_UNDO_H
#define MED_EDITOR_UNDO_H

#include <memory>
#include <vector>

#include <QtCore/QString>

namespace Med {
namespace Editor {

class Point;
class Buffer;

class Undo {
public:
  Undo(Buffer* buffer);
  ~Undo();

  enum class RecordMode {
    NORMAL, UNDO, REDO
  };

  struct Recorder {
    Undo* undo = nullptr;
    Undo::RecordMode mode;
  };

  Recorder recorder() { return {this, RecordMode::NORMAL}; }

  // Undoes the last operation. Returns false if there were no operations to undo, or the undoing failed for some other reason. If undoing succeded and insertionPoint is not null, it's moved to the point where the operation happened.
  bool undo(Point* insertionPoint) { return revertLast(RecordMode::UNDO, insertionPoint); }
  bool redo(Point* insertionPoint) { return revertLast(RecordMode::REDO, insertionPoint); }

  // Called after insertion.
  void recordInsertion(RecordMode mode, const Point& start, const Point& end);
  // Called before points are moved and the line content is modified (so that the ref is still valid).
  void recordPartialLineDeletion(RecordMode mode, const Point& start, const Point& end, QStringRef content);
  // Called after all lines but the first have been extracted from the buffer, but before the first line is actually modified (so that the refs are still valid).
  void recordMultilineDeletion(RecordMode mode, const Point& start, const Point& end, QStringRef firstLineDeleted, std::vector<QString> linesDeleted, QStringRef lastLineDeleted);

  bool modified() const;
  void setUnmodified();

private:
  // Reverts the last recorded op.
  bool revertLast(RecordMode mode, Point* insertionPoint);
  // Clears the recorded ops.
  void clear();

  class Op;
  enum class OpType { INSERTION, DELETION };

  enum class DeletionHandling { PREPEND, APPEND, NEW };
  std::pair<DeletionHandling, Op&> deletionHandling(RecordMode mode, const Point& start, const Point& end);

  Op* currentOp(RecordMode mode);
  Op& newOp(RecordMode mode, OpType opType);

  Buffer* const buffer_;
  std::vector<std::unique_ptr<Op>> opsToUndo_;
  std::vector<std::unique_ptr<Op>> opsToRedo_;

  bool unmodified_ = false;
  // If non-null, reverting this op will put the buffer in unmodified state.
  Op* opMakesUnmodified_ = nullptr;
};

}  // namespace Editor
}  // namespace Med

#endif // MED_EDITOR_UNDO_H
