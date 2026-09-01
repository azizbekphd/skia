/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

CanvasKit._extraInitializations = CanvasKit._extraInitializations || [];
CanvasKit._extraInitializations.push(function() {
  CanvasKit.SVGDOM.MakeFromString = function(svg, fontMgr, logger) {
    return CanvasKit.SVGDOM._MakeFromString(svg, fontMgr || null, logger || null);
  };
});
