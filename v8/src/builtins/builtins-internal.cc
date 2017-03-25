// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/counters.h"
#include "src/interface-descriptors.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

BUILTIN(Illegal) {
  UNREACHABLE();
  return isolate->heap()->undefined_value();  // Make compiler happy.
}

BUILTIN(EmptyFunction) { return isolate->heap()->undefined_value(); }

BUILTIN(UnsupportedThrower) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                 NewError(MessageTemplate::kUnsupported));
}

// -----------------------------------------------------------------------------
// Throwers for restricted function properties and strict arguments object
// properties

BUILTIN(RestrictedFunctionPropertiesThrower) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kRestrictedFunctionProperties));
}

BUILTIN(RestrictedStrictArgumentsPropertiesThrower) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kStrictPoisonPill));
}

// -----------------------------------------------------------------------------
// Interrupt and stack checks.

void Builtins::Generate_InterruptCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kInterrupt);
}

void Builtins::Generate_StackCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kStackGuard);
}

// -----------------------------------------------------------------------------
// TurboFan support builtins.

void Builtins::Generate_CopyFastSmiOrObjectElements(
    compiler::CodeAssemblerState* state) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CopyFastSmiOrObjectElementsDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* object = assembler.Parameter(Descriptor::kObject);

  // Load the {object}s elements.
  Node* source = assembler.LoadObjectField(object, JSObject::kElementsOffset);

  CodeStubAssembler::ParameterMode mode = assembler.OptimalParameterMode();
  Node* length = assembler.TaggedToParameter(
      assembler.LoadFixedArrayBaseLength(source), mode);

  // Check if we can allocate in new space.
  ElementsKind kind = FAST_ELEMENTS;
  int max_elements = FixedArrayBase::GetMaxLengthForNewSpaceAllocation(kind);
  Label if_newspace(&assembler), if_oldspace(&assembler);
  assembler.Branch(
      assembler.UintPtrOrSmiLessThan(
          length, assembler.IntPtrOrSmiConstant(max_elements, mode), mode),
      &if_newspace, &if_oldspace);

  assembler.Bind(&if_newspace);
  {
    Node* target = assembler.AllocateFixedArray(kind, length, mode);
    assembler.CopyFixedArrayElements(kind, source, target, length,
                                     SKIP_WRITE_BARRIER, mode);
    assembler.StoreObjectField(object, JSObject::kElementsOffset, target);
    assembler.Return(target);
  }

  assembler.Bind(&if_oldspace);
  {
    Node* target = assembler.AllocateFixedArray(kind, length, mode,
                                                CodeStubAssembler::kPretenured);
    assembler.CopyFixedArrayElements(kind, source, target, length,
                                     UPDATE_WRITE_BARRIER, mode);
    assembler.StoreObjectField(object, JSObject::kElementsOffset, target);
    assembler.Return(target);
  }
}

void Builtins::Generate_GrowFastDoubleElements(
    compiler::CodeAssemblerState* state) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef GrowArrayElementsDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* object = assembler.Parameter(Descriptor::kObject);
  Node* key = assembler.Parameter(Descriptor::kKey);
  Node* context = assembler.Parameter(Descriptor::kContext);

  Label runtime(&assembler, CodeStubAssembler::Label::kDeferred);
  Node* elements = assembler.LoadElements(object);
  elements = assembler.TryGrowElementsCapacity(
      object, elements, FAST_DOUBLE_ELEMENTS, key, &runtime);
  assembler.Return(elements);

  assembler.Bind(&runtime);
  assembler.TailCallRuntime(Runtime::kGrowArrayElements, context, object, key);
}

void Builtins::Generate_GrowFastSmiOrObjectElements(
    compiler::CodeAssemblerState* state) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef GrowArrayElementsDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* object = assembler.Parameter(Descriptor::kObject);
  Node* key = assembler.Parameter(Descriptor::kKey);
  Node* context = assembler.Parameter(Descriptor::kContext);

  Label runtime(&assembler, CodeStubAssembler::Label::kDeferred);
  Node* elements = assembler.LoadElements(object);
  elements = assembler.TryGrowElementsCapacity(object, elements, FAST_ELEMENTS,
                                               key, &runtime);
  assembler.Return(elements);

  assembler.Bind(&runtime);
  assembler.TailCallRuntime(Runtime::kGrowArrayElements, context, object, key);
}

namespace {

void Generate_NewArgumentsElements(CodeStubAssembler* assembler,
                                   compiler::Node* frame,
                                   compiler::Node* length) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  typedef compiler::Node Node;

  // Check if we can allocate in new space.
  ElementsKind kind = FAST_ELEMENTS;
  int max_elements = FixedArray::GetMaxLengthForNewSpaceAllocation(kind);
  Label if_newspace(assembler), if_oldspace(assembler, Label::kDeferred);
  assembler->Branch(assembler->IntPtrLessThan(
                        length, assembler->IntPtrConstant(max_elements)),
                    &if_newspace, &if_oldspace);

  assembler->Bind(&if_newspace);
  {
    // Prefer EmptyFixedArray in case of non-positive {length} (the {length}
    // can be negative here for rest parameters).
    Label if_empty(assembler), if_notempty(assembler);
    assembler->Branch(
        assembler->IntPtrLessThanOrEqual(length, assembler->IntPtrConstant(0)),
        &if_empty, &if_notempty);

    assembler->Bind(&if_empty);
    assembler->Return(assembler->EmptyFixedArrayConstant());

    assembler->Bind(&if_notempty);
    {
      // Allocate a FixedArray in new space.
      Node* result = assembler->AllocateFixedArray(kind, length);

      // Compute the effective {offset} into the {frame}.
      Node* offset = assembler->IntPtrAdd(length, assembler->IntPtrConstant(1));

      // Copy the parameters from {frame} (starting at {offset}) to {result}.
      Variable var_index(assembler, MachineType::PointerRepresentation());
      Label loop(assembler, &var_index), done_loop(assembler);
      var_index.Bind(assembler->IntPtrConstant(0));
      assembler->Goto(&loop);
      assembler->Bind(&loop);
      {
        // Load the current {index}.
        Node* index = var_index.value();

        // Check if we are done.
        assembler->GotoIf(assembler->WordEqual(index, length), &done_loop);

        // Load the parameter at the given {index}.
        Node* value = assembler->Load(
            MachineType::AnyTagged(), frame,
            assembler->WordShl(assembler->IntPtrSub(offset, index),
                               assembler->IntPtrConstant(kPointerSizeLog2)));

        // Store the {value} into the {result}.
        assembler->StoreFixedArrayElement(result, index, value,
                                          SKIP_WRITE_BARRIER);

        // Continue with next {index}.
        var_index.Bind(
            assembler->IntPtrAdd(index, assembler->IntPtrConstant(1)));
        assembler->Goto(&loop);
      }

      assembler->Bind(&done_loop);
      assembler->Return(result);
    }
  }

  assembler->Bind(&if_oldspace);
  {
    // Allocate in old space (or large object space).
    assembler->TailCallRuntime(
        Runtime::kNewArgumentsElements, assembler->NoContextConstant(),
        assembler->BitcastWordToTagged(frame), assembler->SmiFromWord(length));
  }
}

}  // namespace

void Builtins::Generate_NewUnmappedArgumentsElements(
    compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef NewArgumentsElementsDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* frame = assembler.Parameter(Descriptor::kFrame);
  Node* length = assembler.Parameter(Descriptor::kLength);
  Generate_NewArgumentsElements(&assembler, frame, assembler.SmiToWord(length));
}

void Builtins::Generate_ReturnReceiver(compiler::CodeAssemblerState* state) {
  CodeStubAssembler assembler(state);
  assembler.Return(assembler.Parameter(0));
}

}  // namespace internal
}  // namespace v8
