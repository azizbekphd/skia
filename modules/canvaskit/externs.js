//
// This externs file prevents the Closure JS compiler from minifying away
// names of objects created by Emscripten.
// Basically, by defining empty objects and functions here, Closure will
// know not to rename them.  This is needed because of our pre-js files,
// that is, the JS we hand-write to bundle into the output. That JS will be
// hit by the closure compiler and thus needs to know about what functions
// have special names and should not be minified.
//
// Emscripten does not support automatically generating an externs file, so we
// do it by hand. The general process is to write some JS code, and then put any
// calls to CanvasKit or related things in here. Running ./compile.sh and then
// looking at the minified results or running the Release trybot should
// verify nothing was missed. Optionally, looking directly at the minified
// pathkit.js can be useful when developing locally.
//
// Docs:
//   https://github.com/cljsjs/packages/wiki/Creating-Externs
//   https://github.com/google/closure-compiler/wiki/Types-in-the-Closure-Type-System
//
// Example externs:
//   https://github.com/google/closure-compiler/tree/master/externs
//

var CanvasKit = {
  // public API (i.e. things we declare in the pre-js file or in the cpp bindings)
  Color: function() {},
  Color4f: function() {},
  ColorAsInt: function() {},
  LTRBRect: function() {},
  XYWHRect: function() {},
  LTRBiRect: function() {},
  XYWHiRect: function() {},
  RRectXY: function() {},
  /** @return {ImageData} */
  ImageData: function() {},

  GetWebGLContext: function() {},
  MakeCanvas: function() {},
  MakeCanvasSurface: function() {},
  MakeGrContext: function() {}, // deprecated
  MakeWebGLContext: function() {},
  /** @return {CanvasKit.AnimatedImage} */
  MakeAnimatedImageFromEncoded: function() {},
  /** @return {CanvasKit.Image} */
  MakeImage: function() {},
  /** @return {CanvasKit.Image} */
  MakeImageFromEncoded: function() {},
  MakeImageFromCanvasImageSource: function() {},
  MakeOnScreenGLSurface: function() {},
  MakeRenderTarget: function() {},
  MakePicture: function() {},
  MakeSWCanvasSurface: function() {},
  MakeManagedAnimation: function() {},
  MakeVertices: function() {},
  MakeSurface: function() {},
  MakeGPUDeviceContext: function() {},
  MakeGPUCanvasContext: function() {},
  MakeGPUCanvasSurface: function() {},
  MakeGPUTextureSurface: function() {},
  MakeRasterDirectSurface: function() {},
  MakeWebGLCanvasSurface: function() {},
  Malloc: function() {},
  MallocGlyphIDs: function() {},
  MakeLazyImageFromTextureSource: function() {},
  Free: function() {},
  computeTonalColors: function() {},
  deleteContext: function() {},
  getColorComponents: function() {},
  getDecodeCacheLimitBytes: function() {},
  getDecodeCacheUsageBytes: function() {},
  multiplyByAlpha: function() {},
  parseColorString: function() {},
  setDecodeCacheLimitBytes: function() {},
  getShadowLocalBounds: function() {},
  // Defined by emscripten.
  createContext: function() {},

  // Added by debugger when it extends canvaskit
  MinVersion: function() {},
  SkpFilePlayer: function() {},

  // private API (i.e. things declared in the bindings that we use
  // in the pre-js file)
  _MakeGrContext: function() {},
  _MakeImage: function() {},
  _MakeManagedAnimation: function() {},
  _MakeOnScreenGLSurface: function() {},
  _MakePicture: function() {},
  _MakeRenderTargetII: function() {},
  _MakeRenderTargetWH: function() {},
  _computeTonalColors: function() {},
  _decodeAnimatedImage: function() {},
  _decodeImage: function() {},
  _getShadowLocalBounds: function() {},
  _setTextureCleanup: function() {},

  // The testing object is meant to expose internal functions
  // for more fine-grained testing, e.g. parseColor
  _testing: {},

  // Objects and properties on CanvasKit

  Animation: {
    prototype: {
      render: function() {},
      size: function() {},
    },
    _render: function() {},
    _size: function() {},
  },

  Blender: {
    Mode: function() {},
  },

  GrDirectContext: {
    // public API (from webgl.js)
    prototype: {
      getResourceCacheLimitBytes: function () {},
      getResourceCacheUsageBytes: function () {},
      releaseResourcesAndAbandonContext: function () {},
      setResourceCacheLimitBytes: function () {},
    },

    // private API (from C++ bindings)
    _getResourceCacheLimitBytes: function() {},
    _getResourceCacheUsageBytes: function() {},
    _releaseResourcesAndAbandonContext: function() {},
    _setResourceCacheLimitBytes: function() {},
  },

  ManagedAnimation: {
    prototype: {
      render: function() {},
      seek: function() {},
      seekFrame: function() {},
      setColor: function() {},
      setColorSlot: function() {},
      getColorSlot: function() {},
      setScalarSlot: function() {},
      getScalarSlot: function() {},
      setVec2Slot: function() {},
      getVec2Slot: function() {},
      setTextSlot: function() {},
      getTextSlot: function() {},
      setImageSlot: function() {},
      setTransform: function() {},
      size: function() {},

      attachEditor:          function() {},
      enableEditor:          function() {},
      dispatchEditorKey:     function() {},
      dispatchEditorPointer: function() {},
      setEditorCursorWeight: function() {},
    },
    _render: function() {},
    _seek: function() {},
    _seekFrame: function() {},
    _setTransform: function() {},
    _getSlotInfo: function() {},
    _setColorSlot: function() {},
    _getColorSlot: function() {},
    _setVec2Slot: function() {},
    _getVec2Slot: function() {},
    _setTextSlot: function() {},
    _size: function() {},
  },

  Paragraph: {
    // public API (from C++ bindings)
    didExceedMaxLines: function() {},
    getAlphabeticBaseline: function() {},
    getGlyphPositionAtCoordinate: function() {},
    getHeight: function() {},
    getIdeographicBaseline: function() {},
    getLineMetrics: function() {},
    getLineMetricsAt: function() {},
    getLineNumberAt: function() {},
    getLongestLine: function() {},
    getMaxIntrinsicWidth: function() {},
    getMaxWidth: function() {},
    getMinIntrinsicWidth: function() {},
    getNumberOfLines: function() {},
    getWordBoundary: function() {},
    getShapedLines: function() {},
    layout: function() {},

    // private API
    /** @return {Float32Array} */
    _getClosestGlyphInfoAtCoordinate: function() {},
    _getGlyphInfoAt: function() {},
    _getRectsForRange: function() {},
    _getRectsForPlaceholders: function() {},
  },

  ParagraphBuilder: {
    Make: function() {},
    MakeFromFontProvider: function() {},
    MakeFromFontCollection: function() {},
    ShapeText: function() {},
    RequiresClientICU() {},

    addText: function() {},
    build: function() {},

    setWordsUtf8: function() {},
    setWordsUtf16: function() {},
    setGraphemeBreaksUtf8: function() {},
    setGraphemeBreaksUtf16: function() {},
    setLineBreaksUtf8: function() {},
    setLineBreaksUtf16: function() {},

    getText: function() {},
    pop: function() {},
    reset: function() {},

    prototype: {
      pushStyle: function() {},
      pushPaintStyle: function() {},
      addPlaceholder: function() {},
    },

    // private API
    _Make: function() {},
    _MakeFromFontProvider: function() {},
    _MakeFromFontCollection: function() {},
    _ShapeText: function() {},
    _pushStyle: function() {},
    _pushPaintStyle: function() {},
    _addPlaceholder: function() {},

    _setWordsUtf8: function() {},
    _setWordsUtf16: function() {},
    _setGraphemeBreaksUtf8: function() {},
    _setGraphemeBreaksUtf16: function() {},
    _setLineBreaksUtf8: function() {},
    _setLineBreaksUtf16: function() {},
  },

  Bidi: {
    Make: function() {},
    getBidiRegions: function () {},
    reorderVisual: function () {},
    // private API
    _getBidiRegions: function() {},
    _reorderVisual: function() {},
  },

  CodeUnits: {
    Make: function() {},
    compute: function() {},
    // private API
    _compute: function() {},
  },

  RuntimeEffect: {
    // public API (from JS bindings)
    Make: function() {},
    MakeForBlender: function() {},
    getUniform: function() {},
    getUniformCount: function() {},
    getUniformFloatCount: function() {},
    getUniformName: function() {},
    prototype: {
      makeShader: function() {},
      makeShaderWithChildren: function() {},
      makeBlender: function() {},
    },
    // private API (from C++ bindings)
    _Make: function() {},
    _MakeForBlender: function() {},
    _makeShader: function() {},
    _makeShaderWithChildren: function() {},
    _makeBlender: function() {},
  },

  ParagraphStyle: function() {},

  AnimatedImage: {
    // public API (from C++ bindings)
    decodeNextFrame: function() {},
    getFrameCount: function() {},
    getRepetitionCount: function() {},
    height: function() {},
    makeImageAtCurrentFrame: function() {},
    reset: function() {},
    width: function() {},
  },

  Canvas: {
    // public API (from C++ bindings)
    clipPath: function() {},
    getSaveCount: function() {},
    makeSurface: function() {},
    restore: function() {},
    restoreToCount: function() {},
    rotate: function() {},
    save: function() {},
    saveLayerPaint: function() {},
    scale: function() {},
    skew: function() {},
    translate: function() {},

    prototype: {
      clear: function() {},
      clipRRect: function() {},
      clipRect: function() {},
      concat: function() {},
      drawArc: function() {},
      drawAtlas: function() {},
      drawCircle: function() {},
      drawColor: function() {},
      drawColorComponents: function() {},
      drawColorInt: function() {},
      drawDRRect: function() {},
      drawGlyphs: function() {},
      drawImage: function() {},
      drawImageCubic: function() {},
      drawImageNine: function() {},
      drawImageOptions: function() {},
      drawImageRect: function() {},
      drawImageRectCubic: function() {},
      drawImageRectOptions: function() {},
      drawLine: function() {},
      drawOval: function() {},
      drawPaint: function() {},
      drawParagraph: function() {},
      drawPatch: function() {},
      drawPath: function() {},
      drawPicture: function() {},
      drawPoints: function() {},
      drawRRect:  function() {},
      drawRect4f: function() {},
      drawRect: function() {},
      drawShadow: function() {},
      drawText: function() {},
      drawTextBlob: function() {},
      drawVertices: function() {},
      getDeviceClipBounds: function() {},
      quickReject: function() {},
      getLocalToDevice: function() {},
      getTotalMatrix: function() {},
      readPixels: function() {},
      saveLayer: function() {},
      writePixels : function() {},
    },

    // private API
    _clear: function() {},
    _clipRRect: function() {},
    _clipRect: function() {},
    _concat: function() {},
    _drawArc: function() {},
    _drawAtlasCubic: function() {},
    _drawAtlasOptions: function() {},
    _drawCircle: function() {},
    _drawColor: function() {},
    _drawColorInt: function() {},
    _drawDRRect:  function() {},
    _drawGlyphs: function() {},
    _drawImage: function() {},
    _drawImageCubic: function() {},
    _drawImageNine: function() {},
    _drawImageOptions: function() {},
    _drawImageRect: function() {},
    _drawImageRectCubic: function() {},
    _drawImageRectOptions: function() {},
    _drawLine: function() {},
    _drawOval: function() {},
    _drawPaint: function() {},
    _drawParagraph: function() {},
    _drawPatch: function() {},
    _drawPath: function() {},
    _drawPicture: function() {},
    _drawPoints: function() {},
    _drawRRect:  function() {},
    _drawRect4f: function() {},
    _drawRect: function() {},
    _drawShadow: function() {},
    _drawSimpleText: function() {},
    _drawTextBlob: function() {},
    _drawVertices: function() {},
    _getDeviceClipBounds: function() {},
    _quickReject: function() {},
    _getLocalToDevice: function() {},
    _getTotalMatrix: function() {},
    _readPixels: function() {},
    _saveLayer: function() {},
    _writePixels: function() {},
    delete: function() {},
  },

  ColorFilter: {
    // public API (from C++ bindings and JS interface)
    MakeBlend: function() {},
    MakeCompose: function() {},
    MakeLerp: function() {},
    MakeLinearToSRGBGamma: function() {},
    MakeMatrix: function() {},
    MakeSRGBToLinearGamma: function() {},
    // private API (from C++ bindings)
    _MakeBlend: function() {},
    _makeMatrix: function() {},
  },

  ColorMatrix: {
    concat: function() {},
    identity: function() {},
    postTranslate: function() {},
    rotated: function() {},
    scaled: function() {},
  },

  ColorSpace: {
    Equals: function() {},
    SRGB: {},
    DISPLAY_P3: {},
    ADOBE_RGB: {},
    // private API (from C++ bindings)
    _MakeSRGB: function() {},
    _MakeDisplayP3: function() {},
    _MakeAdobeRGB: function() {},
  },

  ContourMeasureIter: {
    next: function() {},
  },

  ContourMeasure: {
    getSegment: function() {},
    isClosed: function() {},
    length: function() {},
    prototype: {
      getPosTan: function() {},
    },
    _getPosTan: function() {},
  },

  Font: {
    // public API (from C++ bindings)
    getMetrics: function() {},
    getScaleX: function() {},
    getSize: function() {},
    getSkewX: function() {},
    isEmbolden: function() {},
    getTypeface: function() {},
    setHinting: function() {},
    setLinearMetrics: function() {},
    setScaleX: function() {},
    setSize: function() {},
    setSkewX: function() {},
    setEmbolden: function() {},
    setSubpixel: function() {},
    setTypeface: function() {},

    prototype: {
      getGlyphBounds: function() {},
      getGlyphIDs: function() {},
      getGlyphWidths: function() {},
      getGlyphIntercepts: function() {},
    },

    // private API (from C++ bindings)
    _getGlyphIDs: function() {},
    _getGlyphIntercepts: function() {},
    _getGlyphWidthBounds: function() {},
  },

  FontMgr: {
    // public API (from C++ and JS bindings)
    FromData: function() {},
    countFamilies: function() {},
    getFamilyName: function() {},
    matchFamilyStyle: function() {},

    // private API
    _makeTypefaceFromData: function() {},
    _fromData: function() {},
  },

  TypefaceFontProvider: {
    // public API (from C++ and JS bindings)
    Make: function() {},
    registerFont: function() {},

    // private API
    _registerFont: function() {},
  },

  FontCollection: {
    // public API (from C++ and JS bindings)
    Make: function() {},
    setDefaultFontManager: function() {},
    enableFontFallback: function() {},
  },

  Image: {
    // public API (from C++ bindings)
    encodeToBytes: function() {},
    getColorSpace: function() {},
    getImageInfo: function() {},
    makeCopyWithDefaultMipmaps: function() {},
    height: function() {},
    width: function() {},

    prototype: {
      makeShaderCubic: function() {},
      makeShaderOptions: function() {},
    },
    // private API
    _encodeToBytes: function() {},
    _makeFromGenerator: function() {},
    _makeShaderCubic: function() {},
    _makeShaderOptions: function() {},
  },

  ImageFilter: {
    MakeBlend: function() {},
    MakeBlur: function() {},
    MakeColorFilter: function() {},
    MakeCompose: function() {},
    MakeDilate: function() {},
    MakeDisplacementMap: function() {},
    MakeDropShadow: function() {},
    MakeDropShadowOnly: function() {},
    MakeErode: function() {},
    MakeImage: function() {},
    MakeMatrixTransform: function() {},
    MakeOffset: function() {},

    prototype: {
      getOutputBounds: function() {},
    },

    // private API
    _getOutputBounds: function() {},
    _MakeDropShadow: function() {},
    _MakeDropShadowOnly: function() {},
    _MakeImageCubic: function() {},
    _MakeImageOptions: function() {},
    _MakeMatrixTransformCubic: function() {},
    _MakeMatrixTransformOptions: function() {},
  },

  // These are defined in interface.js
  M44: {
    identity: function() {},
    invert: function() {},
    mustInvert: function() {},
    multiply: function() {},
    rotatedUnitSinCos: function() {},
    rotated: function() {},
    scaled: function() {},
    translated: function() {},
    lookat: function() {},
    perspective: function() {},
    rc: function() {},
    transpose: function() {},
    setupCamera: function() {},
  },

  Matrix: {
    identity: function() {},
    invert: function() {},
    mapPoints: function() {},
    multiply: function() {},
    rotated: function() {},
    scaled: function() {},
    skewed: function() {},
    translated: function() {},
  },

  MaskFilter: {
    MakeBlur: function() {},
  },

  MipmapMode: {
    None: {},
    Nearest: {},
    Linear: {},
  },

  Paint: {
    // public API (from C++ bindings)
    /** @return {CanvasKit.Paint} */
    copy: function() {},
    getStrokeCap: function() {},
    getStrokeJoin: function() {},
    getStrokeMiter: function() {},
    getStrokeWidth: function() {},
    setAntiAlias: function() {},
    setBlendMode: function() {},
    setBlender: function() {},
    setColorInt: function() {},
    setDither: function() {},
    setImageFilter: function() {},
    setMaskFilter: function() {},
    setPathEffect: function() {},
    setShader: function() {},
    setStrokeCap: function() {},
    setStrokeJoin: function() {},
    setStrokeMiter: function() {},
    setStrokeWidth: function() {},
    setStyle: function() {},

    prototype: {
      getColor: function() {},
      setColor: function() {},
      setColorComponents: function() {},
      setColorInt: function() {},
    },

    // Private API
    delete: function() {},
    _getColor: function() {},
    _setColor: function() {},
  },

  PathEffect: {
    MakeCorner: function() {},
    MakeDash: function() {},
    MakeDiscrete: function() {},
    MakePath1D: function() {},
    MakeLine2D: function() {},
    MakePath2D: function() {},

    // Private C++ API
    _MakeDash: function() {},
    _MakeLine2D: function() {},
    _MakePath2D: function() {},
  },

  Path: {
    // public API (from C++ and JS bindings)
    CanInterpolate: function() {},
    MakeFromCmds: function() {},
    MakeFromPathInterpolation: function() {},
    MakeFromSVGString: function() {},
    MakeFromOp: function() {},
    MakeFromVerbsPointsWeights: function() {},
    contains: function() {},
    /** @return {CanvasKit.Path} */
    copy: function() {},
    countPoints: function() {},
    equals: function() {},
    getFillType: function() {},
    isEmpty: function() {},
    isVolatile: function() {},
    makeAsWinding: function() {},
    reset: function() {},
    rewind: function() {},
    setFillType: function() {},
    setIsVolatile: function() {},
    toCmds: function() {},
    toSVGString: function() {},

    prototype: {
      addArc: function() {},
      addCircle: function() {},
      addOval: function() {},
      addPath: function() {},
      addPoly: function() {},
      addRect: function() {},
      addRRect: function() {},
      addVerbsPointsWeights: function() {},
      arc: function() {},
      arcToOval: function() {},
      arcToRotated: function() {},
      arcToTangent: function() {},
      close: function() {},
      conicTo: function() {},
      computeTightBounds: function() {},
      cubicTo: function() {},
      dash: function() {},
      getBounds: function() {},
      getPoint: function() {},
      lineTo: function() {},
      moveTo: function() {},
      offset: function() {},
      op: function() {},
      quadTo: function() {},
      rArcTo: function() {},
      rConicTo: function() {},
      rCubicTo: function() {},
      rLineTo: function() {},
      rMoveTo: function() {},
      rQuadTo: function() {},
      simplify: function() {},
      stroke: function() {},
      transform: function() {},
      trim: function() {},
    },

    // private API
    _MakeFromCmds: function() {},
    _MakeFromVerbsPointsWeights: function() {},
    _addArc: function() {},
    _addCircle: function() {},
    _addOval: function() {},
    _addPath: function() {},
    _addPoly: function() {},
    _addRect: function() {},
    _addRRect: function() {},
    _addVerbsPointsWeights: function() {},
    _arcToOval: function() {},
    _arcToRotated: function() {},
    _arcToTangent: function() {},
    _close: function() {},
    _conicTo: function() {},
    _computeTightBounds: function() {},
    _cubicTo: function() {},
    _dash: function() {},
    _getBounds: function() {},
    _getPoint: function() {},
    _lineTo: function() {},
    _moveTo: function() {},
    _op: function() {},
    _quadTo: function() {},
    _rArcTo: function() {},
    _rConicTo: function() {},
    _rCubicTo: function() {},
    _rect: function() {},
    _rLineTo: function() {},
    _rMoveTo: function() {},
    _rQuadTo: function() {},
    _simplify: function() {},
    _stroke: function() {},
    _transform: function() {},
    _trim: function() {},
    delete: function() {},
    dump: function() {},
    dumpHex: function() {},
  },

  Picture: {
    serialize: function() {},
    approximateByteSize: function() {},
    prototype: {
      makeShader: function() {},
      cullRect: function () {},
    },
    _makeShader: function() {},
    _cullRect: function () {},
  },

  PictureRecorder: {
    finishRecordingAsPicture: function() {},
    prototype: {
      beginRecording: function() {},
    },
    _beginRecording: function() {},
  },

  Shader: {
    // Deprecated names
    Blend: function() {},
    Color: function() {},
    Lerp: function() {},
    // public API (from JS / C++ bindings)
    MakeBlend: function() {},
    MakeColor: function() {},
    MakeFractalNoise: function() {},
    MakeLinearGradient: function() {},
    MakeRadialGradient: function() {},
    MakeSweepGradient: function() {},
    MakeTurbulence: function() {},
    MakeTwoPointConicalGradient: function() {},

    // private API (from C++ bindings)
    _MakeColor: function() {},
    _MakeLinearGradient: function() {},
    _MakeRadialGradient: function() {},
    _MakeSweepGradient: function() {},
    _MakeTwoPointConicalGradient: function() {},
  },

  Surface: {
    // public API (from C++ bindings)
    imageInfo: function() {},

    sampleCnt: function() {},
    reportBackendTypeIsGPU: function() {},

    prototype: {
      getCanvas: function() {},
      makeImageFromTexture: function() {},
      makeImageFromTextureSource: function() {},
      /** @return {CanvasKit.Image} */
      makeImageSnapshot: function() {},
      makeSurface: function() {},
      updateTextureFromSource: function() {},
    },

    // private API
    _flush: function() {},
    _getCanvas: function() {},
    _makeImageFromTexture: function() {},
    _makeImageSnapshot: function() {},
    _makeSurface: function() {},
    _makeRasterDirect: function() {},
    _resetContext: function() {},
    delete: function() {},
  },

  TextBlob: {
    // public API (both C++ and JS bindings)
    MakeFromGlyphs: function() {},
    MakeFromRSXform: function() {},
    MakeFromRSXformGlyphs: function() {},
    MakeFromText: function() {},
    MakeOnPath: function() {},
    // private API (from C++ bindings)
    _MakeFromGlyphs: function() {},
    _MakeFromRSXform: function() {},
    _MakeFromRSXformGlyphs: function() {},
    _MakeFromText: function() {},
  },

  Typeface: {
    GetDefault: function() {},
    MakeTypefaceFromData: function() {},
    prototype: {
      getGlyphIDs: function() {},
      getFamilyName: function() {},
    },
    _MakeTypefaceFromData: function() {},
    _getGlyphIDs: function() {},
  },

  // These are defined in interface.js
  Vector: {
    add: function() {},
    sub: function() {},
    dot: function() {},
    cross: function() {},
    normalize: function() {},
    mulScalar: function() {},
    length: function() {},
    lengthSquared: function() {},
    dist: function() {},
  },

  Vertices: {
    // public API (from C++ bindings)
    uniqueID: function() {},

    prototype: {
      bounds: function() {},
    },
    // private API (from C++ bindings)

    _bounds: function() {},
  },

  _VerticesBuilder: {
    colors: function() {},
    detach: function() {},
    indices: function() {},
    positions: function() {},
    texCoords: function() {},
  },

  TextStyle: function() {},

  SkpDebugPlayer: {
    // public API (from C++ bindings)
    loadSkp: function() {},
  },

  // Constants and Enums
  gpu: {},
  skottie: {},

  TRANSPARENT: {},
  BLACK: {},
  WHITE: {},
  RED: {},
  GREEN: {},
  BLUE: {},
  YELLOW: {},
  CYAN: {},
  MAGENTA: {},

  MOVE_VERB: {},
  LINE_VERB: {},
  QUAD_VERB: {},
  CONIC_VERB: {},
  CUBIC_VERB: {},
  CLOSE_VERB: {},

  NoDecoration: {},
  UnderlineDecoration: {},
  OverlineDecoration: {},
  LineThroughDecoration: {},

  SaveLayerInitWithPrevious: {},
  SaveLayerF16ColorType: {},

  Affinity: {
    Upstream: {},
    Downstream: {},
  },

  AlphaType: {
    Opaque: {},
    Premul: {},
    Unpremul: {},
  },

  BlendMode: {
    Clear: {},
    Src: {},
    Dst: {},
    SrcOver: {},
    DstOver: {},
    SrcIn: {},
    DstIn: {},
    SrcOut: {},
    DstOut: {},
    SrcATop: {},
    DstATop: {},
    Xor: {},
    Plus: {},
    Modulate: {},
    Screen: {},
    Overlay: {},
    Darken: {},
    Lighten: {},
    ColorDodge: {},
    ColorBurn: {},
    HardLight: {},
    SoftLight: {},
    Difference: {},
    Exclusion: {},
    Multiply: {},
    Hue: {},
    Saturation: {},
    Color: {},
    Luminosity: {},
  },

  BlurStyle: {
    Normal: {},
    Solid: {},
    Outer: {},
    Inner: {},
  },

  ClipOp: {
    Difference: {},
    Intersect: {},
  },

  ColorType: {
    Alpha_8: {},
    RGB_565: {},
    ARGB_4444: {},
    RGBA_8888: {},
    RGB_888x: {},
    BGRA_8888: {},
    RGBA_1010102: {},
    RGB_101010x: {},
    Gray_8: {},
    RGBA_F16: {},
    RGBA_F32: {},
  },

  FillType: {
    Winding: {},
    EvenOdd: {},
  },

  FilterMode: {
    Linear: {},
    Nearest: {},
  },

  FontSlant: {
    Upright: {},
    Italic: {},
    Oblique: {},
  },

  FontHinting: {
    None: {},
    Slight: {},
    Normal: {},
    Full: {},
  },

  FontWeight: {
    Invisible: {},
    Thin: {},
    ExtraLight: {},
    Light: {},
    Normal: {},
    Medium: {},
    SemiBold: {},
    Bold: {},
    ExtraBold: {},
    Black: {},
    ExtraBlack: {},
  },

  FontWidth: {
    UltraCondensed: {},
    ExtraCondensed: {},
    Condensed: {},
    SemiCondensed: {},
    Normal: {},
    SemiExpanded: {},
    Expanded: {},
    ExtraExpanded: {},
    UltraExpanded: {},
  },

  GlyphRunFlags: {
    IsWhiteSpace: {},
  },

  ImageFormat: {
    PNG: {},
    JPEG: {},
  },

  PaintStyle: {
    Fill: {},
    Stroke: {},
  },

  PathOp: {
    Difference: {},
    Intersect: {},
    Union: {},
    XOR: {},
    ReverseDifference: {},
  },

  PointMode: {
    Points: {},
    Lines: {},
    Polygon: {},
  },

  RectHeightStyle: {
    Tight: {},
    Max: {},
    IncludeLineSpacingMiddle: {},
    IncludeLineSpacingTop: {},
    IncludeLineSpacingBottom: {},
    Strut: {},
  },

  RectWidthStyle: {
    Tight: {},
    Max: {},
  },

  StrokeCap: {
    Butt: {},
    Round: {},
    Square: {},
  },

  StrokeJoin: {
    Miter: {},
    Round: {},
    Bevel: {},
  },

  TextAlign: {
    Left: {},
    Right: {},
    Center: {},
    Justify: {},
    Start: {},
    End: {},
  },

  TextDirection: {
    LTR: {},
    RTL: {},
  },

  LineBreakType : {
    SoftLineBreak: {},
    HardLineBreak: {},
  },

  TextHeightBehavior: {
    All: {},
    DisableFirstAscent: {},
    DisableLastDescent: {},
    DisableAll: {},
  },

  DecorationStyle: {
    Solid: {},
    Double: {},
    Dotted: {},
    Dashed: {},
    Wavy: {},
  },

  PlaceholderAlignment: {
    Baseline: {},
    AboveBaseline: {},
    BelowBaseline: {},
    Top: {},
    Bottom: {},
    Middle: {},
  },

  TextBaseline: {
    Alphabetic: {},
    Ideographic: {},
  },

  TileMode: {
    Clamp: {},
    Repeat: {},
    Mirror: {},
    Decal: {},
  },

  VertexMode: {
    Triangles: {},
    TrianglesStrip: {},
    TriangleFan: {},
  },

  InputState: {
    Up: {},
    Down: {},
    Move: {},
    Right: {},
    Left: {},
  },

  ModifierKey: {
    None: {},
    Shift: {},
    Control: {},
    Option: {},
    Command: {},
    FirstPass: {},
  },

  CodeUnitFlags: {
    NoCodeUnitFlag: {},
    Whitespace: {},
    Space: {},
    Control: {},
    Ideographic: {},
  },

  // Things Enscriptem adds for us

  /**
   * @type {Float32Array}
   */
  HEAPF32: {},
  /**
   * @type {Float64Array}
   */
  HEAPF64: {},
  /**
   * @type {Uint8Array}
   */
  HEAPU8: {},
  /**
   * @type {Uint16Array}
   */
  HEAPU16: {},
  /**
   * @type {Uint32Array}
   */
  HEAPU32: {},
  /**
   * @type {Int8Array}
   */
  HEAP8: {},
  /**
   * @type {Int16Array}
   */
  HEAP16: {},
  /**
   * @type {Int32Array}
   */
  HEAP32: {},

  _malloc: function() {},
  _free: function() {},
  onRuntimeInitialized: function() {},
};

// Public API things that are newly declared in the JS should go here.
// It's not enough to declare them above, because closure can still erase them
// unless they go on the prototype.
CanvasKit.Paragraph.prototype.getRectsForRange = function() {};
CanvasKit.Paragraph.prototype.getRectsForPlaceholders = function() {};
CanvasKit.Paragraph.prototype.getClosestGlyphInfoAtCoordinate = function() {};
CanvasKit.Paragraph.prototype.getGlyphInfoAt = function() {};

CanvasKit.Surface.prototype.dispose = function() {};
CanvasKit.Surface.prototype.flush = function() {};
CanvasKit.Surface.prototype.requestAnimationFrame = function() {};
CanvasKit.Surface.prototype.drawOnce = function() {};

CanvasKit.RuntimeEffect.prototype.makeShader = function() {};
CanvasKit.RuntimeEffect.prototype.makeShaderWithChildren = function() {};

// Define StrokeOpts object
var StrokeOpts = {};
StrokeOpts.prototype.width;
StrokeOpts.prototype.miter_limit;
StrokeOpts.prototype.cap;
StrokeOpts.prototype.join;
StrokeOpts.prototype.precision;

// Define everything created in the canvas2d spec here
var HTMLCanvas = {};
HTMLCanvas.prototype.decodeImage = function() {};
HTMLCanvas.prototype.dispose = function() {};
HTMLCanvas.prototype.getContext = function() {};
HTMLCanvas.prototype.loadFont = function() {};
HTMLCanvas.prototype.makePath2D = function() {};
HTMLCanvas.prototype.toDataURL = function() {};

var ImageBitmapRenderingContext = {};
ImageBitmapRenderingContext.prototype.transferFromImageBitmap = function() {};

var CanvasRenderingContext2D = {};
CanvasRenderingContext2D.prototype.addHitRegion = function() {};
CanvasRenderingContext2D.prototype.arc = function() {};
CanvasRenderingContext2D.prototype.arcTo = function() {};
CanvasRenderingContext2D.prototype.beginPath = function() {};
CanvasRenderingContext2D.prototype.bezierCurveTo = function() {};
CanvasRenderingContext2D.prototype.clearHitRegions = function() {};
CanvasRenderingContext2D.prototype.clearRect = function() {};
CanvasRenderingContext2D.prototype.clip = function() {};
CanvasRenderingContext2D.prototype.closePath = function() {};
CanvasRenderingContext2D.prototype.createImageData = function() {};
CanvasRenderingContext2D.prototype.createLinearGradient = function() {};
CanvasRenderingContext2D.prototype.createPattern = function() {};
CanvasRenderingContext2D.prototype.createRadialGradient = function() {};
CanvasRenderingContext2D.prototype.drawFocusIfNeeded = function() {};
CanvasRenderingContext2D.prototype.drawImage = function() {};
CanvasRenderingContext2D.prototype.ellipse = function() {};
CanvasRenderingContext2D.prototype.fill = function() {};
CanvasRenderingContext2D.prototype.fillRect = function() {};
CanvasRenderingContext2D.prototype.fillText = function() {};
CanvasRenderingContext2D.prototype.getImageData = function() {};
CanvasRenderingContext2D.prototype.getLineDash = function() {};
CanvasRenderingContext2D.prototype.isPointInPath = function() {};
CanvasRenderingContext2D.prototype.isPointInStroke = function() {};
CanvasRenderingContext2D.prototype.lineTo = function() {};
CanvasRenderingContext2D.prototype.measureText = function() {};
CanvasRenderingContext2D.prototype.moveTo = function() {};
CanvasRenderingContext2D.prototype.putImageData = function() {};
CanvasRenderingContext2D.prototype.quadraticCurveTo = function() {};
CanvasRenderingContext2D.prototype.rect = function() {};
CanvasRenderingContext2D.prototype.removeHitRegion = function() {};
CanvasRenderingContext2D.prototype.resetTransform = function() {};
CanvasRenderingContext2D.prototype.restore = function() {};
CanvasRenderingContext2D.prototype.rotate = function() {};
CanvasRenderingContext2D.prototype.save = function() {};
CanvasRenderingContext2D.prototype.scale = function() {};
CanvasRenderingContext2D.prototype.scrollPathIntoView = function() {};
CanvasRenderingContext2D.prototype.setLineDash = function() {};
CanvasRenderingContext2D.prototype.setTransform = function() {};
CanvasRenderingContext2D.prototype.stroke = function() {};
CanvasRenderingContext2D.prototype.strokeRect = function() {};
CanvasRenderingContext2D.prototype.strokeText = function() {};
CanvasRenderingContext2D.prototype.transform = function() {};
CanvasRenderingContext2D.prototype.translate = function() {};

var Path2D = {};
Path2D.prototype.addPath = function() {};
Path2D.prototype.arc = function() {};
Path2D.prototype.arcTo = function() {};
Path2D.prototype.bezierCurveTo = function() {};
Path2D.prototype.closePath = function() {};
Path2D.prototype.ellipse = function() {};
Path2D.prototype.lineTo = function() {};
Path2D.prototype.moveTo = function() {};
Path2D.prototype.quadraticCurveTo = function() {};
Path2D.prototype.rect = function() {};

var LinearCanvasGradient = {};
LinearCanvasGradient.prototype.addColorStop = function() {};
var RadialCanvasGradient = {};
RadialCanvasGradient.prototype.addColorStop = function() {};
var CanvasPattern = {};
CanvasPattern.prototype.setTransform = function() {};

var ImageData = {
  /**
   * @type {Uint8ClampedArray}
   */
  data: {},
  height: {},
  width: {},
};

var DOMMatrix = {
  a: {},
  b: {},
  c: {},
  d: {},
  e: {},
  f: {},
};

// Not sure why this is needed - might be a bug in emsdk that this isn't properly declared.
function loadWebAssemblyModule() {};

// This is a part of emscripten's webgl glue code. Preserving this attribute is necessary
// to override it in the puppeteer tests
var LibraryEGL = {
  contextAttributes: {
    majorVersion: {}
  }
}
