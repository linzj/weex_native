// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check {} for automatic semicolon insertion
es5id: 7.9_A10_T6
description: Checking if execution of "{} \n * 1" fails
negative:
  phase: early
  type: SyntaxError
---*/

//CHECK#1
{}
 * 1
