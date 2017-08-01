#include "src/fast-codegen/label-recorder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/label.h"
#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
LabelRecorder::LabelRecorder(Handle<BytecodeArray> bytecode_array)
    : bytecode_array_(bytecode_array) {}

void LabelRecorder::Record() {
  interpreter::BytecodeArrayIterator iterator(bytecode_array_);
  for (; !iterator.done(); iterator.Advance()) {
    switch (iterator.current_bytecode()) {
      case interpreter::Bytecode::kJump:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfTrue:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfTrueConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfFalse:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfFalseConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfToBooleanTrue:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfToBooleanTrueConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfToBooleanFalse:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfToBooleanFalseConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfNotHole:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfNotHoleConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfJSReceiver:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfJSReceiverConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfNull:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfNullConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfUndefined:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpIfUndefinedConstant:
        Record(iterator);
        break;
      case interpreter::Bytecode::kJumpLoop:
        Record(iterator);
        break;
      default:
        break;
    }
  }
}

Label* LabelRecorder::GetLabel(int offset) {
  DCHECK_GE(offset, 0);
  auto it = indexer_.find(offset);
  if (it != indexer_.end()) return it->second.get();
  return nullptr;
}

void LabelRecorder::Record(const interpreter::BytecodeArrayIterator& iterator) {
  int offset = iterator.GetJumpTargetOffset();
  Record(offset);
}

void LabelRecorder::Record(int offset) {
  DCHECK_GE(offset, 0);
  auto it = indexer_.find(offset);
  if (it != indexer_.end()) return;
  indexer_.insert(std::make_pair(offset, std::unique_ptr<Label>(new Label)));
}
}
}
