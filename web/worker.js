importScripts("wasm/pi.js");

let modulePromise = PiModule({ locateFile: (path) => "wasm/" + path });

self.onmessage = async (e) => {
  const { digits } = e.data;
  const Module = await modulePromise;
  const t0 = performance.now();

  const compute = Module.cwrap("wasm_compute_pi", "number", ["number"]);
  const ptr = compute(digits);

  if (!ptr) {
    self.postMessage({ ok: false, error: "compute failed" });
    return;
  }

  const result = Module.UTF8ToString(ptr);
  Module.ccall("wasm_free", null, ["number"], [ptr]);

  const elapsedMs = performance.now() - t0;
  self.postMessage({ ok: true, digits, result, elapsedMs });
};
