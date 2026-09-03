// pi.js and pi.wasm are a matched pair - a cached copy of one against a fresh
// copy of the other breaks at runtime, so both carry the version app.js passed
// on this worker's own URL.
const V = new URLSearchParams(self.location.search).get("v") || "";
const stamp = (path) => (V ? path + "?v=" + encodeURIComponent(V) : path);

importScripts(stamp("wasm/pi.js"));

let modulePromise = PiModule({ locateFile: (path) => stamp("wasm/" + path) });

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
