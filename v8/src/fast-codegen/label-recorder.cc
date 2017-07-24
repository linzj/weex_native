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
  for (; iterator.done(); iterator.Advance()) {
    switch (iterator.current_bytecode()) {
      case interpreter::Bytecode::kJump:
        RecordJump(iterator);
        break;
      case interpreter::Bytecode::kJumpConstant:
        RecordJumpConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpIfTrue:
        RecordJumpIfTrue(iterator);
        break;
      case interpreter::Bytecode::kJumpIfTrueConstant:
        RecordJumpIfTrueConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpIfFalse:
        RecordJumpIfFalse(iterator);
        break;
      case interpreter::Bytecode::kJumpIfFalseConstant:
        RecordJumpIfFalseConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpIfToBooleanTrue:
        RecordJumpIfToBooleanTrue(iterator);
        break;
      case interpreter::Bytecode::kJumpIfToBooleanTrueConstant:
        RecordJumpIfToBooleanTrueConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpIfToBooleanFalse:
        RecordJumpIfToBooleanFalse(iterator);
        break;
      case interpreter::Bytecode::kJumpIfToBooleanFalseConstant:
        RecordJumpIfToBooleanFalseConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpIfNotHole:
        RecordJumpIfNotHole(iterator);
        break;
      case interpreter::Bytecode::kJumpIfNotHoleConstant:
        RecordJumpIfNotHoleConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpIfJSReceiver:
        RecordJumpIfJSReceiver(iterator);
        break;
      case interpreter::Bytecode::kJumpIfJSReceiverConstant:
        RecordJumpIfJSReceiverConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpIfNull:
        RecordJumpIfNull(iterator);
        break;
      case interpreter::Bytecode::kJumpIfNullConstant:
        RecordJumpIfNullConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpIfUndefined:
        RecordJumpIfUndefined(iterator);
        break;
      case interpreter::Bytecode::kJumpIfUndefinedConstant:
        RecordJumpIfUndefinedConstant(iterator);
        break;
      case interpreter::Bytecode::kJumpLoop:
        RecordJumpLoop(iterator);
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

void LabelRecorder::RecordJump(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpIfTrue(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpIfTrueConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpIfFalse(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpIfFalseConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpIfToBooleanTrue(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpIfToBooleanTrueConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpIfToBooleanFalse(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpIfToBooleanFalseConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpIfNotHole(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpIfNotHoleConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpIfJSReceiver(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpIfJSReceiverConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpIfNull(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpIfNullConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpIfUndefined(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordJumpIfUndefinedConstant(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromConstant(iterator, 0);
}

void LabelRecorder::RecordJumpLoop(
    const interpreter::BytecodeArrayIterator& iterator) {
  RecordFromImm(iterator, 0);
}

void LabelRecorder::RecordFromImm(
    const interpreter::BytecodeArrayIterator& iterator, int index) {
  int offset = iterator.GetUnsignedImmediateOperand(index);
  Record(iterator.current_offset() + offset);
}

void LabelRecorder::RecordFromConstant(
    const interpreter::BytecodeArrayIterator& iterator, int index) {
  Handle<Object> offset = iterator.GetConstantForIndexOperand(index);
  Handle<Smi> offset_smi = Handle<Smi>::cast(offset);
  Record(iterator.current_offset() + offset_smi->value());
}

void LabelRecorder::Record(int offset) {
  DCHECK_GE(offset, 0);
  auto it = indexer_.find(offset);
  if (it != indexer_.end()) return;
  indexer_.insert(std::make_pair(offset, std::unique_ptr<Label>(new Label)));
}
}
}
