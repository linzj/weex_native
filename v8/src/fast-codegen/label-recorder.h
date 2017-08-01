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

  void Record(const interpreter::BytecodeArrayIterator& iterator);
  void Record(int offset);

  Handle<BytecodeArray> bytecode_array_;
  using IndexerType = std::unordered_map<int, std::unique_ptr<Label> >;
  IndexerType indexer_;
};
}
}
#endif  // LABEL_RECORDER_H_
