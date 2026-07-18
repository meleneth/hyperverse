(async () => {
  const statusElement = document.getElementById("status");
  const canvasElement = document.getElementById("canvas");
  const outputElement = document.getElementById("output");

  if (!canvasElement) throw new Error("Missing #canvas element");

  outputElement.value = "";

  function setStatus(text) {
    if (statusElement) statusElement.textContent = text || "";
  }

  function fitCanvas() {
    const dpr = window.devicePixelRatio || 1;
    const width = Math.max(1, Math.floor(canvasElement.clientWidth * dpr));
    const height = Math.max(1, Math.floor(canvasElement.clientHeight * dpr));

    if (canvasElement.width !== width || canvasElement.height !== height) {
      canvasElement.width = width;
      canvasElement.height = height;
    }
  }

  async function webGpuIsAvailable() {
    if (!navigator.gpu) {
      setStatus("WebGPU is not available in this browser.");
      return false;
    }

    try {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        setStatus("No WebGPU adapter is available. Check browser GPU acceleration, GPU driver support, or WebGPU flags.");
        return false;
      }
    } catch (error) {
      setStatus(`WebGPU adapter check failed: ${error.message || error}`);
      return false;
    }

    return true;
  }

  window.Module = window.Module || {};
  const Module = window.Module;
  const buildId = "__HYPERVERSE_BUILD_ID__";
  const versioned = (path) => `${path}?v=${encodeURIComponent(buildId)}`;

  Module.canvas = canvasElement;
  Module.locateFile = (path) => versioned(path);
  Module.mainScriptUrlOrBlob = versioned("hyperverse.js");
  Module.setStatus = setStatus;
  Module.print = (...args) => {
    console.log(...args);
    outputElement.value += args.join(" ") + "\n";
    outputElement.scrollTop = outputElement.scrollHeight;
  };
  Module.printErr = (...args) => {
    console.error(...args);
    outputElement.value += args.join(" ") + "\n";
    outputElement.scrollTop = outputElement.scrollHeight;
  };
  Module.monitorRunDependencies = (remaining) => {
    setStatus(remaining ? `Preparing... (${remaining})` : "All downloads complete.");
  };
  Module.onRuntimeInitialized = () => {
    fitCanvas();
    canvasElement.focus();
    setStatus("");
  };

  canvasElement.addEventListener(
    "webglcontextlost",
    (event) => {
      setStatus("Graphics context lost. Reload the page to continue.");
      event.preventDefault();
    },
    false
  );
  canvasElement.addEventListener("pointerdown", () => canvasElement.focus());

  window.addEventListener("resize", fitCanvas);
  if (window.ResizeObserver) {
    new ResizeObserver(fitCanvas).observe(canvasElement);
  }
  window.onerror = () => {
    setStatus("Exception thrown; see JavaScript console.");
  };

  fitCanvas();
  setStatus("Checking WebGPU...");
  if (!(await webGpuIsAvailable())) {
    return;
  }

  setStatus("Downloading...");
  const script = document.createElement("script");
  script.async = true;
  script.src = versioned("./hyperverse.js");
  document.body.appendChild(script);
})();
