function Debug(msg) {
  console.warn(msg);
}
/** @const */ var IsDebug = true;

// Sanitizer builds need Emscripten's WasmOffsetConverter, but existing CanvasKit callers may
// invoke successCallback with only the instance. Capture the module and bytes from the caller's
// WebAssembly.instantiate() promise before its success callback runs.
if (Module['instantiateWasm']) {
  var userInstantiateWasm = Module['instantiateWasm'];
  Module['instantiateWasm'] = function(imports, successCallback) {
    var originalInstantiate = WebAssembly.instantiate;
    var interceptedInstantiate = function(wasmSource, wasmImports) {
      return Promise.resolve(originalInstantiate.call(WebAssembly, wasmSource, wasmImports))
          .then(function(result) {
            if (result && result.module && typeof WasmOffsetConverter !== 'undefined') {
              wasmOffsetConverter = new WasmOffsetConverter(
                  new Uint8Array(wasmSource), result.module);
            }
            restoreInstantiate();
            return result;
          }, function(error) {
            restoreInstantiate();
            throw error;
          });
    };
    var restoreInstantiate = function() {
      // Restore only our own hook. Another initializer may have installed its hook in the
      // meantime, and overwriting it would leave the global function in an inconsistent state.
      if (WebAssembly.instantiate === interceptedInstantiate) {
        WebAssembly.instantiate = originalInstantiate;
      }
    };
    WebAssembly.instantiate = interceptedInstantiate;
    try {
      return userInstantiateWasm.call(Module, imports, function(instance, module) {
        restoreInstantiate();
        successCallback(instance, module);
      });
    } catch (error) {
      restoreInstantiate();
      throw error;
    }
  };
}
