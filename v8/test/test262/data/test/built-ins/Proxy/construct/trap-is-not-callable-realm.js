// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-proxy-object-internal-methods-and-internal-slots-construct-argumentslist-newtarget
es6id: 9.5.14
description: >
  Throws if trap is not callable (honoring the Realm of the current execution
  context)
---*/

var OProxy = $.createRealm().global.Proxy;
var p = new OProxy(function() {}, {
  construct: {}
});

assert.throws(TypeError, function() {
  new p();
});
