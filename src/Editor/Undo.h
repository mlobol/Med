#ifndef MED_EDITOR_UNDO_H
#define MED_EDITOR_UNDO_H

#include <memory>
#include <vector>

#include <QtCore/QString>

namespace Med {
namespace Editor {

class Buffer;
class Point;
class SafePoint;
class TempPoint;

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

  TempPoint deletionHandling(RecordMode mode, const Point& start, const Point& end);

  bool modified() const;
  void setUnmodified();

private:
  // Reverts the last recorded op.
  bool revertLast(RecordMode mode, Point* insertionPoint);
  // Clears the recorded ops.
  void clear();

  class Op;
  enum class OpType { INSERTION, DELETION };

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
