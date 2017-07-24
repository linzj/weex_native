#ifndef LABEL_RECORDER_H_
#define LABEL_RECORDER_H_
#include <memory>
#include <unordered_map>
#include "src/base/macros.h"
#include "src/handles.h"

namespace v8 {
namespace internal {
namespace interpreter {
class BytecodeArrayIterator;
}
class BytecodeArray;
class Label;

class LabelRecorder {
 public:
  LabelRecorder(Handle<BytecodeArray>);
  ~LabelRecorder() = default;
  void Record();
  Label* GetLabel(int offset);

 private:
  DISALLOW_COPY_AND_ASSIGN(LabelRecorder);  // NOLINT
  void RecordJump(const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpConstant(const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfTrue(const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfTrueConstant(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfFalse(const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfFalseConstant(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfToBooleanTrue(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfToBooleanTrueConstant(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfToBooleanFalse(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfToBooleanFalseConstant(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfNotHole(const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfNotHoleConstant(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfJSReceiver(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfJSReceiverConstant(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfNull(const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfNullConstant(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfUndefined(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpIfUndefinedConstant(
      const interpreter::BytecodeArrayIterator& iterator);
  void RecordJumpLoop(const interpreter::BytecodeArrayIterator& iterator);

  void RecordFromImm(const interpreter::BytecodeArrayIterator& iterator,
                     int index);
  void RecordFromConstant(const interpreter::BytecodeArrayIterator& iterator,
                          int index);
  void Record(int offset);

  Handle<BytecodeArray> bytecode_array_;
  using IndexerType = std::unordered_map<int, std::unique_ptr<Label> >;
  IndexerType indexer_;
};
}
}
#endif  // LABEL_RECORDER_H_
