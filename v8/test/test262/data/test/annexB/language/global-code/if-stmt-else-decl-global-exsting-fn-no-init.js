// This file was procedurally generated from the following sources:
// - src/annex-b-fns/global-exsting-fn-no-init.case
// - src/annex-b-fns/global/if-stmt-else-decl.template
/*---
description: Existing variable binding is not modified (IfStatement with a declaration in the second statement position in the global scope)
esid: sec-functiondeclarations-in-ifstatement-statement-clauses
es6id: B.3.4
flags: [generated, noStrict]
info: |
    The following rules for IfStatement augment those in 13.6:

    IfStatement[Yield, Return]:
        if ( Expression[In, ?Yield] ) FunctionDeclaration[?Yield] else Statement[?Yield, ?Return]
        if ( Expression[In, ?Yield] ) Statement[?Yield, ?Return] else FunctionDeclaration[?Yield]
        if ( Expression[In, ?Yield] ) FunctionDeclaration[?Yield] else FunctionDeclaration[?Yield]
        if ( Expression[In, ?Yield] ) FunctionDeclaration[?Yield]


    B.3.3.2 Changes to GlobalDeclarationInstantiation

    [...]
    1. Let fnDefinable be ? envRec.CanDeclareGlobalFunction(F).
    2. If fnDefinable is true, then
---*/
assert.sameValue(f(), 'outer declaration');

if (false) ; else function f() { return 'inner declaration'; }

function f() {
  return 'outer declaration';
}
