// Emscripten pre-js shim (plan.md §12.1 item 18 follow-up, web build).
//
// With -sALLOW_MEMORY_GROWTH the wasm heap is backed by a RESIZABLE
// ArrayBuffer, and newer browsers (Chrome 130+ implementing the current
// WebCrypto spec) reject crypto.getRandomValues() on views of resizable
// buffers outright -- which aborts Emscripten's own getentropy shim (hit
// by anything touching std::random_device during startup) before main()
// ever runs. Wrap it: try the direct call first (fast path, non-resizable
// views unaffected), and on the resizable-buffer TypeError fill a small
// temporary and copy. Remove once the toolchain's own shim handles this
// (tracked upstream in emscripten).
(function () {
    if (typeof crypto === 'undefined' || !crypto.getRandomValues) return;
    var original = crypto.getRandomValues.bind(crypto);
    crypto.getRandomValues = function (view) {
        try {
            return original(view);
        } catch (e) {
            var tmp = new Uint8Array(view.byteLength);
            original(tmp);
            new Uint8Array(view.buffer, view.byteOffset, view.byteLength).set(tmp);
            return view;
        }
    };
})();
