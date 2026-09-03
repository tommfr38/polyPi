const MAX_WEB_DIGITS = 1000000;

// TODO: fill these in once the repo is published.
const DESKTOP_DOWNLOAD_URL = "#";
const SOURCE_CODE_URL = "#";

const els = {
  digits: document.getElementById("digitsInput"),
  presets: document.querySelectorAll(".chip"),
  computeBtn: document.getElementById("computeBtn"),
  speedSlider: document.getElementById("speedSlider"),
  stageOverlay: document.getElementById("stageOverlay"),
  overlayBig: document.getElementById("overlayBig"),
  overlaySmall: document.getElementById("overlaySmall"),
  toast: document.getElementById("lockToast"),
  results: document.getElementById("results"),
  resultsStats: document.getElementById("resultsStats"),
  digitsBox: document.getElementById("digitsBox"),
  copyBtn: document.getElementById("copyBtn"),
  saveBtn: document.getElementById("saveBtn"),
  canvas: document.getElementById("piCanvas"),
};

["desktopLink1", "desktopLink2", "toastDesktop"].forEach(
  (id) => (document.getElementById(id).href = DESKTOP_DOWNLOAD_URL)
);
["sourceLink1", "sourceLink2", "toastSource"].forEach(
  (id) => (document.getElementById(id).href = SOURCE_CODE_URL)
);

// ---------- particle field ----------

const ctx = els.canvas.getContext("2d");
let W = 0, H = 0, DPR = Math.min(window.devicePixelRatio || 1, 2);
let particles = [];
let animState = "idle"; // idle | forming | holding | pulse
let animStart = 0;
const FORM_DURATION = 1300;

function resizeCanvas() {
  const rect = els.canvas.getBoundingClientRect();
  W = Math.max(1, Math.floor(rect.width));
  H = Math.max(1, Math.floor(rect.height));
  els.canvas.width = W * DPR;
  els.canvas.height = H * DPR;
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
  buildParticles();
}

function samplePiPoints(targetCount) {
  const off = document.createElement("canvas");
  off.width = W;
  off.height = H;
  const octx = off.getContext("2d");
  const fontSize = Math.floor(H * 0.62);
  octx.fillStyle = "#fff";
  octx.font = `900 ${fontSize}px "Arial Black", "Helvetica Neue", Arial, sans-serif`;
  octx.textAlign = "center";
  octx.textBaseline = "middle";
  octx.fillText("π", W / 2, H / 2 + fontSize * 0.04);
  const data = octx.getImageData(0, 0, W, H).data;
  const pts = [];
  const step = Math.max(2, Math.floor(Math.min(W, H) / 140));
  for (let y = 0; y < H; y += step) {
    for (let x = 0; x < W; x += step) {
      if (data[(y * W + x) * 4 + 3] > 128) pts.push({ x, y });
    }
  }
  for (let i = pts.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [pts[i], pts[j]] = [pts[j], pts[i]];
  }
  return pts.slice(0, targetCount);
}

function buildParticles() {
  const targets = samplePiPoints(1600);
  particles = targets.map((t) => {
    const angle = Math.random() * Math.PI * 2;
    const dist = Math.max(W, H) * (0.6 + Math.random() * 0.6);
    return {
      sx: W / 2 + Math.cos(angle) * dist,
      sy: H / 2 + Math.sin(angle) * dist,
      tx: t.x,
      ty: t.y,
      x: 0,
      y: 0,
      delay: Math.random() * 300,
      jitterPhase: Math.random() * Math.PI * 2,
      r: 1.1 + Math.random() * 1.3,
    };
  });
  // scatter positions for idle state
  particles.forEach((p) => {
    p.x = p.sx;
    p.y = p.sy;
  });
}

function easeOutCubic(t) { return 1 - Math.pow(1 - t, 3); }
function easeOutBack(t) {
  const c1 = 1.70158, c3 = c1 + 1;
  return 1 + c3 * Math.pow(t - 1, 3) + c1 * Math.pow(t - 1, 2);
}

function startForming() {
  animState = "forming";
  animStart = performance.now();
}

function pulseDone() {
  animState = "pulse";
  animStart = performance.now();
}

function drawFrame(now) {
  ctx.clearRect(0, 0, W, H);

  if (animState === "idle") {
    ctx.fillStyle = "rgba(57,255,106,0.25)";
    const t = now / 1800;
    particles.forEach((p, i) => {
      const drift = Math.sin(t + i) * 1.2;
      ctx.beginPath();
      ctx.arc(p.x, p.y + drift, p.r * 0.7, 0, Math.PI * 2);
      ctx.fill();
    });
  } else if (animState === "forming" || animState === "holding") {
    if (animState === "forming") {
      const elapsed = now - animStart;
      let allDone = true;
      particles.forEach((p) => {
        const local = Math.min(1, Math.max(0, (elapsed - p.delay) / FORM_DURATION));
        if (local < 1) allDone = false;
        const eased = easeOutBack(local);
        p.x = p.sx + (p.tx - p.sx) * eased;
        p.y = p.sy + (p.ty - p.sy) * eased;
      });
      if (allDone) animState = "holding";
    }
    const jitterT = now / 260;
    particles.forEach((p, i) => {
      const jx = Math.sin(jitterT + p.jitterPhase) * 0.6;
      const jy = Math.cos(jitterT * 1.3 + p.jitterPhase) * 0.6;
      const drawX = animState === "holding" ? p.tx + jx : p.x;
      const drawY = animState === "holding" ? p.ty + jy : p.y;
      ctx.beginPath();
      ctx.fillStyle = "rgba(57,255,106,0.9)";
      ctx.shadowColor = "rgba(57,255,106,0.9)";
      ctx.shadowBlur = 6;
      ctx.arc(drawX, drawY, p.r, 0, Math.PI * 2);
      ctx.fill();
    });
    ctx.shadowBlur = 0;
  } else if (animState === "pulse") {
    const elapsed = now - animStart;
    const t = Math.min(1, elapsed / 900);
    const glow = 6 + Math.sin(t * Math.PI) * 22;
    particles.forEach((p) => {
      ctx.beginPath();
      ctx.fillStyle = `rgba(${Math.round(57 + t * 140)},255,${Math.round(106 + t * 80)},1)`;
      ctx.shadowColor = "rgba(120,255,180,1)";
      ctx.shadowBlur = glow;
      ctx.arc(p.tx, p.ty, p.r * (1 + t * 0.4), 0, Math.PI * 2);
      ctx.fill();
    });
    ctx.shadowBlur = 0;
    if (t >= 1) animState = "holding";
  }

  requestAnimationFrame(drawFrame);
}

window.addEventListener("resize", resizeCanvas);
resizeCanvas();
requestAnimationFrame(drawFrame);

// ---------- worker ----------

const worker = new Worker("worker.js");
let pending = false;

worker.onmessage = (e) => {
  const msg = e.data;
  pending = false;
  els.computeBtn.disabled = false;
  els.computeBtn.textContent = "COMPUTE π";

  if (!msg.ok) {
    setOverlay(true, "ERROR", "computation failed — try fewer digits");
    return;
  }

  pulseDone();
  showResults(msg);
};

function setOverlay(visible, big, small) {
  els.stageOverlay.classList.toggle("visible", visible);
  els.overlayBig.textContent = big;
  els.overlaySmall.textContent = small;
}

function showResults(msg) {
  setOverlay(false, "", "");
  els.results.classList.add("visible");
  const digitsPerSec = Math.round(msg.digits / (msg.elapsedMs / 1000));
  els.resultsStats.innerHTML =
    `<b>${msg.digits.toLocaleString()}</b> digits in <b>${(msg.elapsedMs / 1000).toFixed(2)}s</b>` +
    ` &middot; ~<b>${digitsPerSec.toLocaleString()}</b> digits/sec`;

  streamDigits(msg.result);
  els.digitsBox.dataset.full = msg.result;
}

function streamDigits(full) {
  const head = full.slice(0, 2); // "3."
  const rest = full.slice(2);
  const steps = 40;
  const chunk = Math.max(1, Math.ceil(rest.length / steps));
  let shown = 0;
  els.digitsBox.innerHTML = `<span class="digits-3">${head}</span><span id="streamRest"></span><span class="cursor"></span>`;
  const restEl = document.getElementById("streamRest");

  function tick() {
    shown = Math.min(rest.length, shown + chunk);
    restEl.textContent = rest.slice(0, shown);
    if (shown < rest.length) {
      requestAnimationFrame(tick);
    } else {
      els.digitsBox.querySelector(".cursor")?.remove();
    }
  }
  requestAnimationFrame(tick);
}

// ---------- controls ----------

function clampDigits() {
  let v = parseInt(els.digits.value, 10);
  if (isNaN(v) || v < 1) v = 1;
  if (v > MAX_WEB_DIGITS) v = MAX_WEB_DIGITS;
  els.digits.value = v;
  return v;
}

els.digits.addEventListener("change", clampDigits);

els.presets.forEach((chip) => {
  chip.addEventListener("click", () => {
    els.presets.forEach((c) => c.classList.remove("active"));
    chip.classList.add("active");
    els.digits.value = chip.dataset.digits;
  });
});

els.digits.addEventListener("input", () => {
  els.presets.forEach((c) => c.classList.toggle("active", c.dataset.digits === els.digits.value));
});

els.computeBtn.addEventListener("click", () => {
  if (pending) return;
  const digits = clampDigits();
  pending = true;
  els.computeBtn.disabled = true;
  els.computeBtn.textContent = "COMPUTING…";
  els.results.classList.remove("visible");
  setOverlay(true, "COMPUTING", `assembling ${digits.toLocaleString()} digits`);
  startForming();
  worker.postMessage({ digits });
});

let toastTimer = null;
function showToast() {
  els.toast.classList.add("visible");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => els.toast.classList.remove("visible"), 6000);
}

["pointerdown", "mousedown", "touchstart", "keydown"].forEach((evt) => {
  els.speedSlider.addEventListener(evt, (e) => {
    e.preventDefault();
    showToast();
  });
});
els.speedSlider.addEventListener("click", (e) => e.preventDefault());

els.copyBtn.addEventListener("click", async () => {
  const full = els.digitsBox.dataset.full || "";
  try {
    await navigator.clipboard.writeText(full);
    els.copyBtn.textContent = "COPIED";
    setTimeout(() => (els.copyBtn.textContent = "COPY"), 1200);
  } catch {
    els.copyBtn.textContent = "COPY FAILED";
  }
});

els.saveBtn.addEventListener("click", () => {
  const full = els.digitsBox.dataset.full || "";
  const blob = new Blob([full], { type: "text/plain" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = `pi_${(full.length - 2)}_digits.txt`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
});
