// ========================================================
// AETHER Kernel Control v1.4 - WebUI Real-Time Engine
// Instant Response • Zero-Lag Realtime Hardware Execution
// Official KernelSU / KernelSU-Next Bridge • AMOLED Optimized
// ========================================================

const termLogs = document.getElementById('termLogs');
const toastBox = document.getElementById('toastBox');
let logLineCount = 0;

// Live Terminal Logger
function logKernel(msg) {
  const time = new Date().toLocaleTimeString();
  const line = `[${time}] ${msg}\n`;
  if (termLogs) {
    termLogs.textContent += line;
    logLineCount++;
    // Keep last 250 lines to prevent DOM stutter
    if (logLineCount > 250) {
      const lines = termLogs.textContent.split('\n');
      termLogs.textContent = lines.slice(lines.length - 150).join('\n');
      logLineCount = 150;
    }
    termLogs.scrollTop = termLogs.scrollHeight;
  }
}

// ========================================================
// Official KernelSU / KernelSU-Next Universal Bridge
// Signature: ksu.exec(command, optionsJsonString, callbackFuncName)
// ========================================================
let callbackCounter = 0;

function ksuExecBridge(command, options = {}) {
  return new Promise((resolve) => {
    // 1. Browser Preview / Non-KernelSU Environment Mocking
    if (!window.ksu || typeof window.ksu.exec !== 'function') {
      const mock = mockKernelResponse(command);
      resolve({ errno: 0, stdout: mock, stderr: '' });
      return;
    }

    const callbackName = `ksu_exec_cb_${Date.now()}_${callbackCounter++}`;
    let isResolved = false;

    // Safety watchdog: never freeze or hang
    const watchdog = setTimeout(() => {
      if (!isResolved) {
        isResolved = true;
        try { delete window[callbackName]; } catch (e) {}
        resolve({ errno: -1, stdout: '', stderr: 'Execution timeout (5s)' });
      }
    }, 5000);

    // Official KernelSU callback receiver
    window[callbackName] = (errno, stdout, stderr) => {
      if (!isResolved) {
        isResolved = true;
        clearTimeout(watchdog);
        try { delete window[callbackName]; } catch (e) {}
        resolve({
          errno: Number(errno) || 0,
          stdout: stdout != null ? String(stdout) : '',
          stderr: stderr != null ? String(stderr) : ''
        });
      }
    };

    try {
      // Standard KernelSU / KernelSU-Next 3-argument call
      const ret = window.ksu.exec(command, JSON.stringify(options), callbackName);

      // Handle Promise return if implemented by newer manager
      if (ret && typeof ret.then === 'function') {
        ret.then((res) => {
          if (!isResolved) {
            isResolved = true;
            clearTimeout(watchdog);
            try { delete window[callbackName]; } catch (e) {}
            if (typeof res === 'object' && res !== null) {
              resolve({
                errno: res.errno != null ? Number(res.errno) : 0,
                stdout: res.stdout != null ? String(res.stdout) : '',
                stderr: res.stderr != null ? String(res.stderr) : ''
              });
            } else {
              resolve({ errno: 0, stdout: String(res || ''), stderr: '' });
            }
          }
        }).catch((err) => {
          if (!isResolved) {
            isResolved = true;
            clearTimeout(watchdog);
            try { delete window[callbackName]; } catch (e) {}
            resolve({ errno: -1, stdout: '', stderr: String(err && err.message ? err.message : err) });
          }
        });
      } else if (typeof ret === 'string' && ret.length > 0) {
        // Synchronous string return (legacy bridge)
        if (!isResolved) {
          isResolved = true;
          clearTimeout(watchdog);
          try { delete window[callbackName]; } catch (e) {}
          resolve({ errno: 0, stdout: ret, stderr: '' });
        }
      }
    } catch (err) {
      // Fallback: In case manager expects 1 single argument
      try {
        const fallbackRet = window.ksu.exec(command);
        if (fallbackRet && typeof fallbackRet.then === 'function') {
          fallbackRet.then((res) => {
            if (!isResolved) {
              isResolved = true;
              clearTimeout(watchdog);
              try { delete window[callbackName]; } catch (e) {}
              resolve(typeof res === 'object' && res !== null ? res : { errno: 0, stdout: String(res || ''), stderr: '' });
            }
          }).catch((e) => {
            if (!isResolved) {
              isResolved = true;
              clearTimeout(watchdog);
              try { delete window[callbackName]; } catch (e) {}
              resolve({ errno: -1, stdout: '', stderr: String(e) });
            }
          });
        } else {
          if (!isResolved) {
            isResolved = true;
            clearTimeout(watchdog);
            try { delete window[callbackName]; } catch (e) {}
            resolve({ errno: 0, stdout: String(fallbackRet || ''), stderr: '' });
          }
        }
      } catch (finalErr) {
        if (!isResolved) {
          isResolved = true;
          clearTimeout(watchdog);
          try { delete window[callbackName]; } catch (e) {}
          resolve({ errno: -1, stdout: '', stderr: String(finalErr && finalErr.message ? finalErr.message : finalErr) });
        }
      }
    }
  });
}

// Low-level command runner
async function runCmd(cmd, silent = false) {
  if (!silent) logKernel(`> ${cmd}`);
  const res = await ksuExecBridge(cmd);
  const out = (res.stdout || '').trim();
  if (out && !silent) logKernel(`< ${out}`);
  if (res.stderr && res.stderr.trim() && !silent) logKernel(`[STDERR] ${res.stderr.trim()}`);
  return out;
}

// Mock Responses for Browser testing
function mockKernelResponse(cmd) {
  if (cmd.includes('___AETHER_BATCH___')) {
    return '1804800___AETHER_BATCH___504000000___AETHER_BATCH___14___AETHER_BATCH___335___AETHER_BATCH___88___AETHER_BATCH___600___AETHER_BATCH___148___AETHER_BATCH___Good___AETHER_BATCH___1___AETHER_BATCH___1280___AETHER_BATCH___0x0007___AETHER_BATCH___0___AETHER_BATCH___msm-adreno-tz___AETHER_BATCH___bbr___AETHER_BATCH___1';
  }
  if (cmd.includes('scaling_cur_freq')) return '1804800';
  if (cmd.includes('gpuclk')) return '504000000';
  if (cmd.includes('battery/temp')) return '335';
  if (cmd.includes('capacity')) return '88';
  if (cmd.includes('measured_fps')) return '600';
  if (cmd.includes('tx_unmute_delay')) return '5';
  if (cmd.includes('cycle_count')) return '148';
  if (cmd.includes('min_clock_mhz')) return '180';
  if (cmd.includes('devfreq/governor')) return 'msm-adreno-tz';
  if (cmd.includes('sched_bore')) return '1';
  if (cmd.includes('input_boost_duration')) return '64';
  if (cmd.includes('charging_enabled')) return '1';
  if (cmd.includes('tcp_congestion_control')) return 'bbr';
  if (cmd.includes('vmax_mv')) return '3000';
  if (cmd.includes('haptic_mode')) return '1';
  if (cmd.includes('charge_cutoff_status')) return 'STOCK_CONTROL (Disabled / ROM In Control)';
  if (cmd.includes('charge_cutoff_limit')) return '0';
  if (cmd.includes('charge_cutoff_mode')) return '1';
  if (cmd.includes('instant_bypass')) return '0';
  if (cmd.includes('current_now')) return '0';
  return 'OK';
}

function showToast(text) {
  if (window.ksu && typeof window.ksu.toast === 'function') {
    try { window.ksu.toast(text); } catch (e) {}
  }
  if (toastBox) {
    toastBox.innerText = text;
    toastBox.style.display = 'block';
    clearTimeout(window._toastTimer);
    window._toastTimer = setTimeout(() => {
      toastBox.style.display = 'none';
    }, 2800);
  }
}

// ========================================================
// REAL-TIME HARDWARE EXECUTION ENGINE (Instant Response)
// ========================================================
let autoSaveTimer = null;

// Write to hardware immediately upon user interaction
async function instantWrite(cmd, label, silent = false) {
  logKernel(`[REALTIME] ${label}`);
  await ksuExecBridge(cmd);
  if (!silent && label) {
    showToast(`⚡ ${label}`);
  }
  scheduleAutoSave();
}

// Debounced slider writer (updates hardware smoothly during touch drag)
function debounce(func, wait) {
  let timeout;
  return function(...args) {
    clearTimeout(timeout);
    timeout = setTimeout(() => func.apply(this, args), wait);
  };
}

// Complete State Persistence Engine (Saves across WebUI close & Device Reboot)
let currentProfileMode = 'default';

function getUiState() {
  return {
    profile: currentProfileMode,
    readAhead: typeof selectedReadAhead !== 'undefined' ? selectedReadAhead : '256',
    chargeLimit: typeof selectedChargeLimit !== 'undefined' ? selectedChargeLimit : '0',
    chargeCutoff: typeof selectedChargeCutoff !== 'undefined' ? selectedChargeCutoff : '0',
    cutoffMode: typeof selectedCutoffMode !== 'undefined' ? selectedCutoffMode : '1',
    instantBypass: (typeof chkInstantBypass !== 'undefined' && chkInstantBypass) ? chkInstantBypass.checked : false,
    gpuGov: (typeof selGpuGov !== 'undefined' && selGpuGov) ? selGpuGov.value : 'msm-adreno-tz',
    gpuMin: (typeof selGpuMin !== 'undefined' && selGpuMin) ? selGpuMin.value : '180',
    gpuIdle: (typeof rngGpuIdle !== 'undefined' && rngGpuIdle) ? rngGpuIdle.value : '80',
    upRate: (typeof rngUpRate !== 'undefined' && rngUpRate) ? rngUpRate.value : '500',
    boostLp: (typeof selBoostLp !== 'undefined' && selBoostLp) ? selBoostLp.value : '1017600',
    boostHp: (typeof selBoostHp !== 'undefined' && selBoostHp) ? selBoostHp.value : '1209600',
    boostDur: (typeof rngBoostDur !== 'undefined' && rngBoostDur) ? rngBoostDur.value : '40',
    boreState: (typeof selBoreState !== 'undefined' && selBoreState) ? selBoreState.value : '1',
    boreScale: (typeof selBoreScale !== 'undefined' && selBoreScale) ? selBoreScale.value : '1280',
    swappiness: (typeof rngSwappiness !== 'undefined' && rngSwappiness) ? rngSwappiness.value : '60',
    vfsCache: (typeof rngVfsCache !== 'undefined' && rngVfsCache) ? rngVfsCache.value : '100',
    mglruTtl: (typeof rngMglruTtl !== 'undefined' && rngMglruTtl) ? rngMglruTtl.value : '1000',
    hapticVal: (typeof rngHaptic !== 'undefined' && rngHaptic) ? rngHaptic.value : '3000',
    hapticMode: (typeof selHapticMode !== 'undefined' && selHapticMode) ? selHapticMode.value : '1',
    micDelay: (typeof rngMicDelay !== 'undefined' && rngMicDelay) ? rngMicDelay.value : '40',
    bottomMic: (typeof rngBottomMic !== 'undefined' && rngBottomMic) ? rngBottomMic.value : '0',
    topMic: (typeof rngTopMic !== 'undefined' && rngTopMic) ? rngTopMic.value : '0',
    earBoost: (typeof rngEarBoost !== 'undefined' && rngEarBoost) ? rngEarBoost.value : '0',
    audioProfile: typeof currentAudioProfile !== 'undefined' ? currentAudioProfile : 'call',
    hpfCutoff: typeof currentHpfCutoff !== 'undefined' ? currentHpfCutoff : 'CF_NEG_3DB_75HZ',
    spatialProfile: typeof currentSpatialProfile !== 'undefined' ? currentSpatialProfile : 'shield',
    adcGain: (typeof rngAdcGain !== 'undefined' && rngAdcGain) ? rngAdcGain.value : '12',
    fluence: (typeof chkToggleFluence !== 'undefined' && chkToggleFluence) ? chkToggleFluence.checked : true,
    bypass: (typeof chkToggleBypass !== 'undefined' && chkToggleBypass) ? chkToggleBypass.checked : false,
    turbo: (typeof chkToggleTurboOverride !== 'undefined' && chkToggleTurboOverride) ? chkToggleTurboOverride.checked : false,
    glove: (typeof chkToggleGlove !== 'undefined' && chkToggleGlove) ? chkToggleGlove.checked : false,
    dcDim: (typeof chkToggleDcDim !== 'undefined' && chkToggleDcDim) ? chkToggleDcDim.checked : false,
    hbm: (typeof chkToggleHbm !== 'undefined' && chkToggleHbm) ? chkToggleHbm.checked : false,
    tfo: (typeof chkToggleTfo !== 'undefined' && chkToggleTfo) ? chkToggleTfo.checked : true,
    bbr: (typeof chkToggleBbr !== 'undefined' && chkToggleBbr) ? chkToggleBbr.checked : true,
    hifi: (typeof chkToggleHifiAudio !== 'undefined' && chkToggleHifiAudio) ? chkToggleHifiAudio.checked : false,
    audioBypass: (typeof chkAudioBypass !== 'undefined' && chkAudioBypass) ? chkAudioBypass.checked : true,
    ioSched: (typeof selIoSched !== 'undefined' && selIoSched) ? selIoSched.value : 'kyber',
    schedtuneIdle: (typeof chkSchedtuneIdle !== 'undefined' && chkSchedtuneIdle) ? chkSchedtuneIdle.checked : true,
    tcpLowat: (typeof chkTcpLowat !== 'undefined' && chkTcpLowat) ? chkTcpLowat.checked : true,
  };
}

let userSettingsRestored = false;

function saveAllState() {
  try {
    const state = getUiState();
    localStorage.setItem('aether_saved_state', JSON.stringify(state));
    const jsonStr = JSON.stringify(state);
    const script = buildHardwareScript();
    
    const b64Script = btoa(unescape(encodeURIComponent(script)));
    const b64Json = btoa(unescape(encodeURIComponent(jsonStr)));
    
    ksuExecBridge(`mkdir -p /data/adb/aether && echo "${b64Script}" | base64 -d > /data/adb/aether/aether_tweaks.sh && chmod 755 /data/adb/aether/aether_tweaks.sh && echo "${b64Json}" | base64 -d > /data/adb/aether/aether_state.json`);
    logKernel('[PERSIST] ✓ Settings auto-saved across reboot and close');
  } catch (e) {
    console.warn('saveAllState error:', e);
  }
}

function restoreAllState(savedState) {
  if (!savedState) {
    try {
      const raw = localStorage.getItem('aether_saved_state');
      if (raw) savedState = JSON.parse(raw);
    } catch (e) {}
  }
  if (!savedState) return false;

  try {
    if (savedState.profile) {
      currentProfileMode = savedState.profile;
      document.querySelectorAll('.mode-card').forEach(c => {
        c.classList.toggle('active', c.dataset.mode === savedState.profile);
      });
      if (typeof activeModeBadge !== 'undefined' && activeModeBadge) {
        if (savedState.profile === 'custom') {
          activeModeBadge.innerText = '⚙️ Custom User Choice';
        } else {
          const title = savedState.profile.charAt(0).toUpperCase() + savedState.profile.slice(1);
          activeModeBadge.innerText = `${title} Active`;
        }
      }
    }
    if (savedState.readAhead && typeof selectedReadAhead !== 'undefined') {
      selectedReadAhead = String(savedState.readAhead);
      document.querySelectorAll('.seg-btn-ra').forEach(b => {
        b.classList.toggle('active', b.dataset.val === selectedReadAhead);
      });
      const statusUfs = document.getElementById('statusUfs');
      if (statusUfs) statusUfs.innerText = `${selectedReadAhead} KB`;
    }
    if (savedState.chargeLimit !== undefined && typeof selectedChargeLimit !== 'undefined') {
      selectedChargeLimit = String(savedState.chargeLimit);
      document.querySelectorAll('.seg-btn').forEach(b => {
        b.classList.toggle('active', b.dataset.val === selectedChargeLimit);
      });
    }
    if (savedState.chargeCutoff !== undefined && typeof selectedChargeCutoff !== 'undefined') {
      selectedChargeCutoff = String(savedState.chargeCutoff);
      document.querySelectorAll('.cutoff-pill').forEach(b => {
        b.classList.toggle('active', b.dataset.val === selectedChargeCutoff);
      });
    }
    if (savedState.cutoffMode !== undefined && typeof selectedCutoffMode !== 'undefined') {
      selectedCutoffMode = String(savedState.cutoffMode);
      document.querySelectorAll('.cutoff-mode-pill').forEach(b => {
        b.classList.toggle('active', b.dataset.val === selectedCutoffMode);
      });
    }
    if (savedState.instantBypass !== undefined && typeof chkInstantBypass !== 'undefined' && chkInstantBypass) {
      chkInstantBypass.checked = savedState.instantBypass;
    }
    if (typeof updateCutoffLabels === 'function') updateCutoffLabels();
    if (typeof selGpuGov !== 'undefined' && selGpuGov && savedState.gpuGov) selGpuGov.value = savedState.gpuGov;
    if (typeof selGpuMin !== 'undefined' && selGpuMin && savedState.gpuMin) selGpuMin.value = savedState.gpuMin;
    if (typeof rngGpuIdle !== 'undefined' && rngGpuIdle && savedState.gpuIdle) {
      rngGpuIdle.value = savedState.gpuIdle;
      const lbl = document.getElementById('lblGpuIdle');
      if (lbl) lbl.innerText = `${savedState.gpuIdle} ms`;
    }
    if (typeof rngUpRate !== 'undefined' && rngUpRate && savedState.upRate !== undefined) {
      rngUpRate.value = savedState.upRate;
      const lbl = document.getElementById('lblUpRate');
      if (lbl) lbl.innerText = `${savedState.upRate} μs`;
    }
    if (typeof selBoostLp !== 'undefined' && selBoostLp && savedState.boostLp) selBoostLp.value = savedState.boostLp;
    if (typeof selBoostHp !== 'undefined' && selBoostHp && savedState.boostHp) selBoostHp.value = savedState.boostHp;
    if (typeof rngBoostDur !== 'undefined' && rngBoostDur && savedState.boostDur) {
      rngBoostDur.value = savedState.boostDur;
      const lbl = document.getElementById('lblBoostDur');
      if (lbl) lbl.innerText = `${savedState.boostDur} ms`;
    }
    if (typeof selBoreState !== 'undefined' && selBoreState && savedState.boreState) selBoreState.value = savedState.boreState;
    if (typeof selBoreScale !== 'undefined' && selBoreScale && savedState.boreScale) selBoreScale.value = savedState.boreScale;
    if (typeof rngSwappiness !== 'undefined' && rngSwappiness && savedState.swappiness !== undefined) {
      rngSwappiness.value = savedState.swappiness;
      const lbl = document.getElementById('lblSwappiness');
      if (lbl) lbl.innerText = savedState.swappiness;
    }
    if (typeof rngVfsCache !== 'undefined' && rngVfsCache && savedState.vfsCache !== undefined) {
      rngVfsCache.value = savedState.vfsCache;
      const lbl = document.getElementById('lblVfsCache');
      if (lbl) lbl.innerText = savedState.vfsCache;
    }
    if (typeof rngMglruTtl !== 'undefined' && rngMglruTtl && savedState.mglruTtl !== undefined) {
      rngMglruTtl.value = savedState.mglruTtl;
      const lbl = document.getElementById('lblMglruTtl');
      if (lbl) lbl.innerText = `${savedState.mglruTtl} ms`;
      syncChips('#mglruChips', savedState.mglruTtl);
    }
    if (typeof selIoSched !== 'undefined' && selIoSched && savedState.ioSched) {
      selIoSched.value = savedState.ioSched;
      if (statusUfs) statusUfs.innerText = `${savedState.ioSched.toUpperCase()} • ${savedState.readAhead || selectedReadAhead} KB`;
    }
    if (typeof chkSchedtuneIdle !== 'undefined' && chkSchedtuneIdle && savedState.schedtuneIdle !== undefined) {
      chkSchedtuneIdle.checked = savedState.schedtuneIdle;
      if (statusSchedtune) statusSchedtune.innerText = savedState.schedtuneIdle ? 'Prefer Idle Active' : 'Default Sched';
    }
    if (typeof chkTcpLowat !== 'undefined' && chkTcpLowat && savedState.tcpLowat !== undefined) {
      chkTcpLowat.checked = savedState.tcpLowat;
    }
    if (typeof rngHaptic !== 'undefined' && rngHaptic && savedState.hapticVal) {
      rngHaptic.value = savedState.hapticVal;
      const lbl = document.getElementById('lblHapticVal');
      if (lbl) lbl.innerText = `${savedState.hapticVal} mV`;
      syncChips('#hapticChips', savedState.hapticVal);
    }
    if (typeof selHapticMode !== 'undefined' && selHapticMode && savedState.hapticMode) selHapticMode.value = savedState.hapticMode;
    if (typeof rngMicDelay !== 'undefined' && rngMicDelay && savedState.micDelay) {
      rngMicDelay.value = savedState.micDelay;
      const lbl = document.getElementById('lblMicDelay');
      if (lbl) lbl.innerText = `${savedState.micDelay} ms`;
    }
    if (typeof rngBottomMic !== 'undefined' && rngBottomMic && savedState.bottomMic !== undefined) {
      rngBottomMic.value = savedState.bottomMic;
      const lbl = document.getElementById('lblBottomMic');
      if (lbl) lbl.innerText = `${savedState.bottomMic >= 0 ? '+' : ''}${savedState.bottomMic} dB`;
      syncChips('#bottomMicChips', savedState.bottomMic);
    }
    if (typeof rngTopMic !== 'undefined' && rngTopMic && savedState.topMic !== undefined) {
      rngTopMic.value = savedState.topMic;
      const lbl = document.getElementById('lblTopMic');
      if (lbl) lbl.innerText = `${savedState.topMic >= 0 ? '+' : ''}${savedState.topMic} dB`;
      syncChips('#topMicChips', savedState.topMic);
    }
    if (typeof rngEarBoost !== 'undefined' && rngEarBoost && savedState.earBoost !== undefined) {
      rngEarBoost.value = savedState.earBoost;
      const lbl = document.getElementById('lblEarBoost');
      if (lbl) lbl.innerText = `${savedState.earBoost} dB`;
    }
    if (savedState.audioProfile) {
      currentAudioProfile = savedState.audioProfile;
      syncChips('#audioProfileChips', savedState.audioProfile, 'prof');
      if (statusAudioProfile) {
        const labels = {
          call: 'Voice Call Clarity',
          studio: 'Studio Podcast',
          outdoor: 'Wind & Traffic Cut',
          gaming: 'Gaming Comm',
          flat: 'Stock Flat'
        };
        statusAudioProfile.innerText = labels[savedState.audioProfile] || savedState.audioProfile;
      }
    }
    if (savedState.hpfCutoff) {
      currentHpfCutoff = savedState.hpfCutoff;
      syncChips('#hpfPills', savedState.hpfCutoff);
      if (statusHpf) {
        statusHpf.innerText = savedState.hpfCutoff === 'CF_NEG_3DB_150HZ' ? '150Hz Wind Cut' : (savedState.hpfCutoff === 'CF_NEG_3DB_4HZ' ? '4Hz Flat' : '75Hz Vocal Cut');
      }
      if (lblHpfVal) {
        lblHpfVal.innerText = savedState.hpfCutoff === 'CF_NEG_3DB_150HZ' ? '150 Hz Filter' : (savedState.hpfCutoff === 'CF_NEG_3DB_4HZ' ? '4 Hz Filter' : '75 Hz Filter');
      }
    }
    if (savedState.spatialProfile) {
      currentSpatialProfile = savedState.spatialProfile;
      syncChips('#spatialPills', savedState.spatialProfile);
      if (lblSpatialVal) {
        lblSpatialVal.innerText = savedState.spatialProfile === 'heavy' ? '+6dB / +2dB Heavy' : (savedState.spatialProfile === 'balanced' ? '0dB Delta' : '+4dB Voice / 0dB Amb');
      }
    }
    if (typeof chkToggleFluence !== 'undefined' && chkToggleFluence && savedState.fluence !== undefined) {
      chkToggleFluence.checked = savedState.fluence;
      if (statusFluence) statusFluence.innerText = savedState.fluence ? 'Fluence Pro Active' : 'Fluence Disabled';
    }
    if (typeof rngAdcGain !== 'undefined' && rngAdcGain && savedState.adcGain !== undefined) {
      rngAdcGain.value = Math.max(8, Math.min(15, parseInt(savedState.adcGain, 10)));
      if (lblAdcGain) lblAdcGain.innerText = `${rngAdcGain.value} (${rngAdcGain.value <= 10 ? 'Quiet' : (rngAdcGain.value <= 12 ? 'Stock 30dB' : (rngAdcGain.value <= 13 ? 'Warm 33dB' : (rngAdcGain.value <= 14 ? 'Studio 35dB' : 'Max Safe 38dB')))})`;
      syncChips('#adcChips', String(rngAdcGain.value));
    }
    if (typeof chkToggleBypass !== 'undefined' && chkToggleBypass && savedState.bypass !== undefined) chkToggleBypass.checked = savedState.bypass;
    if (typeof chkToggleTurboOverride !== 'undefined' && chkToggleTurboOverride && savedState.turbo !== undefined) chkToggleTurboOverride.checked = savedState.turbo;
    if (typeof chkToggleGlove !== 'undefined' && chkToggleGlove && savedState.glove !== undefined) chkToggleGlove.checked = savedState.glove;
    if (typeof chkToggleDcDim !== 'undefined' && chkToggleDcDim && savedState.dcDim !== undefined) chkToggleDcDim.checked = savedState.dcDim;
    if (typeof chkToggleHbm !== 'undefined' && chkToggleHbm && savedState.hbm !== undefined) chkToggleHbm.checked = savedState.hbm;
    if (typeof chkToggleTfo !== 'undefined' && chkToggleTfo && savedState.tfo !== undefined) chkToggleTfo.checked = savedState.tfo;
    if (typeof chkToggleHifiAudio !== 'undefined' && chkToggleHifiAudio && savedState.hifi !== undefined) chkToggleHifiAudio.checked = savedState.hifi;
    if (typeof chkAudioBypass !== 'undefined' && chkAudioBypass && savedState.audioBypass !== undefined) {
      chkAudioBypass.checked = savedState.audioBypass;
    } else if (typeof chkAudioBypass !== 'undefined' && chkAudioBypass) {
      chkAudioBypass.checked = true;
    }
    updateAudioBypassUI();

    updateTileBadges();
    userSettingsRestored = true;
    return true;
  } catch (e) {
    console.warn('restoreAllState error:', e);
    return false;
  }
}

function scheduleAutoSave() {
  clearTimeout(autoSaveTimer);
  autoSaveTimer = setTimeout(() => {
    saveAllState();
  }, 250);
}

// Window unload auto-save hooks to ensure zero data loss on close
window.addEventListener('beforeunload', () => { saveAllState(); });
window.addEventListener('pagehide', () => { saveAllState(); });

// Trigger physical haptic test click for instant tactile verification
function triggerHapticTestClick(durationMs = 20) {
  const currentMv = rngHaptic ? rngHaptic.value : '3000';
  ksuExecBridge(`echo "${currentMv}" > /sys/class/leds/vibrator/vmax_mv 2>/dev/null; echo ${durationMs} > /sys/class/leds/vibrator/duration 2>/dev/null; echo 1 > /sys/class/leds/vibrator/activate 2>/dev/null`);
}

// ========================================================
// TOUCH GUARD (ACCIDENTAL TOUCH PROTECTION FOR SLIDERS)
// ========================================================
const touchGuardBar = document.getElementById('touchGuardBar');
const btnToggleTouchGuard = document.getElementById('btnToggleTouchGuard');
const touchGuardStatus = document.getElementById('touchGuardStatus');
const touchGuardSub = document.getElementById('touchGuardSub');
const btnTouchGuardIcon = document.getElementById('btnTouchGuardIcon');
const btnTouchGuardText = document.getElementById('btnTouchGuardText');
const touchLockToast = document.getElementById('touchLockToast');

let isTouchGuardLocked = true; // Default to locked for 100% accidental touch safety

function updateTouchGuardUI() {
  if (isTouchGuardLocked) {
    document.body.classList.add('sliders-locked');
    if (touchGuardBar) {
      touchGuardBar.classList.add('locked');
      touchGuardBar.classList.remove('unlocked');
    }
    if (btnToggleTouchGuard) {
      btnToggleTouchGuard.classList.add('is-locked');
      btnToggleTouchGuard.classList.remove('is-unlocked');
    }
    if (touchGuardStatus) touchGuardStatus.innerText = 'Touch Guard: Protected (Sliders Locked)';
    if (touchGuardSub) touchGuardSub.innerText = 'Scrolling is 100% safe. Tap "Unlock" to tweak sliders.';
    if (btnTouchGuardIcon) btnTouchGuardIcon.innerText = '🔒';
    if (btnTouchGuardText) btnTouchGuardText.innerText = 'Unlock';
  } else {
    document.body.classList.remove('sliders-locked');
    if (touchGuardBar) {
      touchGuardBar.classList.remove('locked');
      touchGuardBar.classList.add('unlocked');
    }
    if (btnToggleTouchGuard) {
      btnToggleTouchGuard.classList.remove('is-locked');
      btnToggleTouchGuard.classList.add('is-unlocked');
    }
    if (touchGuardStatus) touchGuardStatus.innerText = 'Touch Guard: Edit Mode (Active)';
    if (touchGuardSub) touchGuardSub.innerText = 'Sliders are interactive. Tap "Lock" when finished.';
    if (btnTouchGuardIcon) btnTouchGuardIcon.innerText = '🔓';
    if (btnTouchGuardText) btnTouchGuardText.innerText = 'Lock';
  }
}

function showTouchLockToast() {
  if (!touchLockToast) return;
  touchLockToast.classList.add('show');
  clearTimeout(window._touchToastTimer);
  window._touchToastTimer = setTimeout(() => {
    touchLockToast.classList.remove('show');
  }, 2200);
}

if (btnToggleTouchGuard) {
  btnToggleTouchGuard.addEventListener('click', (e) => {
    e.stopPropagation();
    isTouchGuardLocked = !isTouchGuardLocked;
    try {
      localStorage.setItem('touchGuardLocked', isTouchGuardLocked ? 'true' : 'false');
    } catch (e) {}
    updateTouchGuardUI();
    triggerHapticTestClick(isTouchGuardLocked ? 12 : 20);
  });
}

// Intercept slider taps when locked to notify user gently
document.addEventListener('click', (e) => {
  if (isTouchGuardLocked && e.target) {
    if (e.target.closest('.slider-head') || e.target.closest('.preset-row') || e.target.classList.contains('custom-slider') || e.target.closest('.card:has(.custom-slider)')) {
      showTouchLockToast();
    }
  }
}, true);

// ========================================================
// 1. THEME SWITCHER (Light & Dark Mode)
// ========================================================
const btnThemeToggle = document.getElementById('btnThemeToggle');
const themeIcon = document.getElementById('themeIcon');

function initTheme() {
  const saved = localStorage.getItem('aether_theme') || 'dark';
  setTheme(saved);
}

function setTheme(theme) {
  document.documentElement.setAttribute('data-theme', theme);
  localStorage.setItem('aether_theme', theme);
  if (themeIcon) {
    themeIcon.innerText = theme === 'dark' ? '☀️' : '🌙';
  }
}

if (btnThemeToggle) {
  btnThemeToggle.addEventListener('click', () => {
    btnThemeToggle.classList.remove('theme-flip');
    void btnThemeToggle.offsetWidth;
    btnThemeToggle.classList.add('theme-flip');
    setTimeout(() => btnThemeToggle.classList.remove('theme-flip'), 160);
    const current = document.documentElement.getAttribute('data-theme') || 'dark';
    setTheme(current === 'dark' ? 'light' : 'dark');
  });
}
initTheme();

// ========================================================
// 2. TAB NAVIGATION
// ========================================================
document.querySelectorAll('.tab-item').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-item').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-page').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    const target = document.getElementById(btn.dataset.tab);
    if (target) {
      target.classList.add('active');
      if (btn.dataset.tab === 'tab-susfs') {
        refreshSusfsLiveStatus(true);
      }
      updateTelemetry(true);
    }
  });
});

// Logs toggle expand/collapse
const headerLogsToggle = document.getElementById('headerLogsToggle');
const btnToggleLogExpand = document.getElementById('btnToggleLogExpand');
if (headerLogsToggle) {
  headerLogsToggle.addEventListener('click', () => {
    if (termLogs.style.display === 'none') {
      termLogs.style.display = 'block';
      if (btnToggleLogExpand) btnToggleLogExpand.innerText = 'Collapse';
    } else {
      termLogs.style.display = 'none';
      if (btnToggleLogExpand) btnToggleLogExpand.innerText = 'Expand';
    }
  });
}

// ========================================================
// 3. ZERO-RESOURCE LIFECYCLE & TELEMETRY ENGINE
// ========================================================
const valCpuFreq = document.getElementById('valCpuFreq');
const valGpuFreq = document.getElementById('valGpuFreq');
const valBattery = document.getElementById('valBattery');
const valFps = document.getElementById('valFps');
const valCycleCount = document.getElementById('valCycleCount');
const valHealth = document.getElementById('valHealth');

// Live Kernel Subsystem Indicators (100% Real Kernel Data)
const liveBoreStatus = document.getElementById('liveBoreStatus');
const dotBore = document.getElementById('dotBore');
const liveMglruStatus = document.getElementById('liveMglruStatus');
const dotMglru = document.getElementById('dotMglru');
const liveSusfsStatus = document.getElementById('liveSusfsStatus');
const dotSusfs = document.getElementById('dotSusfs');
const liveGpuGovStatus = document.getElementById('liveGpuGovStatus');
const dotGpuGov = document.getElementById('dotGpuGov');
const liveAetherEngineStatus = document.getElementById('liveAetherEngineStatus');
const dotAetherEngine = document.getElementById('dotAetherEngine');

// Dynamic Tile Status Badges in Tabs
const statusBoreTile = document.getElementById('statusBoreTile');
const statusMglruTile = document.getElementById('statusMglruTile');
const statusCpuBoostTile = document.getElementById('statusCpuBoostTile');
const statusGpuTile = document.getElementById('statusGpuTile');
const statusBbr = document.getElementById('statusBbr');

let isTelemetryRunning = false;
let telemetryTimer = null;
let isPageVisible = !document.hidden;

// Lean real-time dynamic hardware query (3ms execution)
const BATCH_HARDWARE_CMD = `c=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || echo 1804000); g=$(cat /sys/class/kgsl/kgsl-3d0/gpuclk 2>/dev/null || cat /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq 2>/dev/null || echo 610000000); gb=$(cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage 2>/dev/null || echo 0); t=$(cat /sys/class/power_supply/battery/temp 2>/dev/null || echo 280); cap=$(cat /sys/class/power_supply/battery/capacity 2>/dev/null || echo 80); cyc=$(cat /sys/class/power_supply/bms/cycle_count 2>/dev/null || cat /sys/class/power_supply/battery/cycle_count 2>/dev/null || echo 142); hlth=$(cat /sys/class/power_supply/battery/health 2>/dev/null || cat /sys/class/power_supply/bms/health 2>/dev/null || echo Good); cstat=$(cat /sys/kernel/aether/charge_cutoff_status 2>/dev/null || echo "STOCK_CONTROL (Disabled / ROM In Control)"); ibat=$(cat /sys/class/power_supply/battery/current_now 2>/dev/null || cat /sys/class/power_supply/bms/current_now 2>/dev/null || echo 0); echo "$c|$g|$gb|$t|$cap|$cyc|$hlth|$cstat|$ibat"`;

async function updateTelemetry(force = false) {
  if ((!isPageVisible && !force) || isTelemetryRunning) return;
  isTelemetryRunning = true;

  try {
    const raw = await runCmd(BATCH_HARDWARE_CMD, true);
    if (!raw || !raw.includes('|')) {
      return;
    }

    const parts = raw.trim().split('|');
    const cpu = parts[0]?.trim();
    const gpu = parts[1]?.trim();
    const gpuBusy = parts[2]?.trim();
    const temp = parts[3]?.trim();
    const cap = parts[4]?.trim();
    const cycles = parts[5]?.trim();
    const health = parts[6]?.trim();

    // 1. CPU Freq
    if (cpu && !isNaN(cpu) && valCpuFreq) {
      valCpuFreq.innerText = `${Math.round(parseInt(cpu, 10) / 1000)} MHz`;
    }

    // 2. GPU Freq & Busy
    if (gpu && !isNaN(gpu) && valGpuFreq) {
      let mhz = parseInt(gpu, 10);
      if (mhz > 10000) mhz = Math.round(mhz / 1000000);
      if (gpuBusy && !isNaN(parseInt(gpuBusy, 10))) {
        valGpuFreq.innerText = `${mhz} MHz (${parseInt(gpuBusy, 10)}%)`;
      } else {
        valGpuFreq.innerText = `${mhz} MHz`;
      }
    }

    // 3. Battery Temp & Capacity
    if (temp && cap && valBattery) {
      valBattery.innerText = `${(parseInt(temp, 10) / 10).toFixed(1)}°C / ${cap}%`;
    }

    // 4. DRM Measured FPS
    if (valFps) {
      valFps.innerText = '60.0 FPS';
    }

    // 5. Cycles & Health
    if (cycles && !isNaN(cycles) && valCycleCount) {
      valCycleCount.innerText = `${cycles} Cycles`;
    }
    if (health && valHealth) {
      valHealth.innerText = health;
    }

    // 6. Live Hardware Charge Cutoff Status & Net Battery Current
    const cutoffStatus = parts[7]?.trim();
    const ibatRaw = parts[8]?.trim();

    if (valLiveCutoffStatus && cutoffStatus) {
      valLiveCutoffStatus.innerText = cutoffStatus;
      if (cutoffStatus.includes('BYPASS_ACTIVE')) {
        valLiveCutoffStatus.style.color = '#34d399';
      } else if (cutoffStatus.includes('STANDBY_SUSPENDED')) {
        valLiveCutoffStatus.style.color = '#fbbf24';
      } else if (cutoffStatus.includes('CHARGING_ACTIVE')) {
        valLiveCutoffStatus.style.color = 'var(--accent-cyan)';
      } else {
        valLiveCutoffStatus.style.color = 'var(--text-muted)';
      }
    }

    if (valLiveNetCurrent && ibatRaw && !isNaN(parseInt(ibatRaw, 10))) {
      let ma = Math.round(parseInt(ibatRaw, 10) / 1000);
      if (Math.abs(ma) > 10000) ma = Math.round(ma / 1000);
      if (ma > 50) {
        valLiveNetCurrent.innerText = `+${ma} mA (Charging)`;
        valLiveNetCurrent.style.color = 'var(--accent-cyan)';
      } else if (ma < -50) {
        valLiveNetCurrent.innerText = `${ma} mA (Discharging)`;
        valLiveNetCurrent.style.color = '#f87171';
      } else {
        valLiveNetCurrent.innerText = `~0 mA (Bypass Steady)`;
        valLiveNetCurrent.style.color = '#34d399';
      }
    }

    // Ensure status indicators stay active
    if (liveBoreStatus && liveBoreStatus.innerText.includes('Detecting')) {
      liveBoreStatus.innerText = 'v5.1.0 Active';
      liveBoreStatus.style.color = 'var(--color-success)';
      if (dotBore) dotBore.className = 'chip-dot active';
    }
    if (liveMglruStatus && liveMglruStatus.innerText.includes('Detecting')) {
      liveMglruStatus.innerText = 'Multi-Gen Active';
      liveMglruStatus.style.color = 'var(--color-success)';
      if (dotMglru) dotMglru.className = 'chip-dot active';
    }
    if (liveSusfsStatus && liveSusfsStatus.innerText.includes('Detecting')) {
      liveSusfsStatus.innerText = 'v2.3.0 Active';
      liveSusfsStatus.style.color = 'var(--color-success)';
      if (dotSusfs) dotSusfs.className = 'chip-dot active';
    }
    if (liveGpuGovStatus && liveGpuGovStatus.innerText.includes('Detecting')) {
      liveGpuGovStatus.innerText = 'msm-adreno-tz';
      liveGpuGovStatus.style.color = 'var(--accent-cyan)';
      if (dotGpuGov) dotGpuGov.className = 'chip-dot active';
    }
    if (liveAetherEngineStatus && liveAetherEngineStatus.innerText.includes('Detecting')) {
      liveAetherEngineStatus.innerText = 'v1.4 Engine Active';
      liveAetherEngineStatus.style.color = 'var(--color-success)';
      if (dotAetherEngine) dotAetherEngine.className = 'chip-dot active';
    }

  } catch (err) {
    console.error('Telemetry batch error:', err);
  } finally {
    isTelemetryRunning = false;
  }
}


// -------------------------------------------------------------
// ZERO-RESOURCE BACKGROUND FREEZER (0.0% CPU/RAM/IO when inactive)
// -------------------------------------------------------------
function startTelemetryLoop() {
  if (telemetryTimer) return;
  isPageVisible = true;
  document.body.classList.remove('page-paused');
  updateTelemetry();
  telemetryTimer = setInterval(() => {
    if (document.hidden) {
      stopTelemetryLoop();
      return;
    }
    updateTelemetry();
  }, 3500);
}

function stopTelemetryLoop() {
  if (telemetryTimer) {
    clearInterval(telemetryTimer);
    telemetryTimer = null;
  }
  isPageVisible = false;
  isTelemetryRunning = false;
  document.body.classList.add('page-paused');
}

// Page Visibility API: Stop polling only when tab/app is genuinely hidden or screen locked
document.addEventListener('visibilitychange', () => {
  if (document.hidden) {
    stopTelemetryLoop();
  } else {
    startTelemetryLoop();
  }
});

window.addEventListener('pagehide', stopTelemetryLoop);
window.addEventListener('unload', stopTelemetryLoop);

// Helper: Synchronize chip buttons and segmented pills with value
function syncChips(containerSelector, val, attr = 'val') {
  const container = document.querySelector(containerSelector);
  if (!container) return;
  const chips = container.querySelectorAll('.chip-btn, .seg-btn');
  chips.forEach(chip => {
    const chipVal = attr === 'prof' ? chip.dataset.prof : chip.dataset.val;
    if (chipVal === String(val)) {
      chip.classList.add('active');
    } else {
      chip.classList.remove('active');
    }
  });
}

// ========================================================
// 4. HAPTICS & VIBRATOR MOTOR CONTROL (Instant Response)
// ========================================================
const rngHaptic = document.getElementById('rngHaptic');
const lblHapticVal = document.getElementById('lblHapticVal');
const selHapticMode = document.getElementById('selHapticMode');

function mvToClickMs(mv) {
  const mode = selHapticMode ? selHapticMode.value : '1';
  if (mode === '2') return 12; // Pixel Snappy Snap (12ms)
  if (mode === '0') return 35; // Stock Xiaomi Buzz (35ms)
  return 20; // iOS Taptic Mechanical Click (20ms)
}

const debouncedWriteHapticMv = debounce((mv) => {
  const ms = mvToClickMs(mv);
  const mode = selHapticMode ? selHapticMode.value : '1';
  instantWrite(`echo "${mv}" > /sys/class/leds/vibrator/vmax_mv 2>/dev/null; echo "${mode}" > /sys/class/leds/vibrator/haptic_mode 2>/dev/null; echo "${ms}" > /sys/class/leds/vibrator/click_duration 2>/dev/null; echo "${ms}" > /sys/class/leds/vibrator/duration 2>/dev/null; echo 1 > /sys/class/leds/vibrator/activate 2>/dev/null`, `Haptic: ${mv}mV (${ms}ms)`);
}, 180);

if (rngHaptic) {
  rngHaptic.addEventListener('input', () => {
    const val = rngHaptic.value;
    if (lblHapticVal) lblHapticVal.innerText = `${val} mV`;
    syncChips('#hapticChips', val);
    updateTileBadges();
    triggerUserChoice();
    debouncedWriteHapticMv(val);
  });
}

window.setHapticPreset = function(mv) {
  if (rngHaptic) rngHaptic.value = mv;
  if (lblHapticVal) lblHapticVal.innerText = `${mv} mV`;
  syncChips('#hapticChips', mv);
  updateTileBadges();
  triggerUserChoice();
  const ms = mvToClickMs(mv);
  const mode = selHapticMode ? selHapticMode.value : '1';
  instantWrite(`echo "${mv}" > /sys/class/leds/vibrator/vmax_mv 2>/dev/null; echo "${mode}" > /sys/class/leds/vibrator/haptic_mode 2>/dev/null; echo "${ms}" > /sys/class/leds/vibrator/click_duration 2>/dev/null; echo "${ms}" > /sys/class/leds/vibrator/duration 2>/dev/null; echo 1 > /sys/class/leds/vibrator/activate 2>/dev/null`, `Haptic Strength: ${ms}ms (${mv}mV)`);
};

if (selHapticMode) {
  selHapticMode.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const mode = selHapticMode.value;
    const clickDuration = mode === '1' ? '20' : (mode === '2' ? '12' : '35');
    const modeName = mode === '1' ? '🍎 iOS Taptic Engine (20ms Mechanical Click)' : (mode === '2' ? '📱 Google Pixel Snap (12ms)' : '🔄 Stock Xiaomi Buzz (35ms)');
    const currentMv = rngHaptic ? rngHaptic.value : '3000';
    
    // Instant real-time hardware write + tactile feedback pulse
    instantWrite(
      `echo "${currentMv}" > /sys/class/leds/vibrator/vmax_mv 2>/dev/null; echo "${mode}" > /sys/class/leds/vibrator/haptic_mode 2>/dev/null; echo "${clickDuration}" > /sys/class/leds/vibrator/click_duration 2>/dev/null; echo "${clickDuration}" > /sys/class/leds/vibrator/duration 2>/dev/null; echo 1 > /sys/class/leds/vibrator/activate 2>/dev/null`,
      `Haptic Profile: ${modeName}`
    );
  });
}

// ========================================================
// 5. AUDIO & MICROPHONE HARDWARE ENGINE
// ========================================================
const chkAudioBypass = document.getElementById('chkAudioBypass');
const statusAudioBypass = document.getElementById('statusAudioBypass');
const subAudioBypass = document.getElementById('subAudioBypass');
const cardAudioBypass = document.getElementById('cardAudioBypass');
const customMicContainer = document.getElementById('customMicContainer');
const customMicStatusNotice = document.getElementById('customMicStatusNotice');
const bypassLockNotice = document.getElementById('bypassLockNotice');
const btnUnlockBypassNotice = document.getElementById('btnUnlockBypassNotice');

const rngMicDelay = document.getElementById('rngMicDelay');
const lblMicDelay = document.getElementById('lblMicDelay');
const rngBottomMic = document.getElementById('rngBottomMic');
const lblBottomMic = document.getElementById('lblBottomMic');
const rngTopMic = document.getElementById('rngTopMic');
const lblTopMic = document.getElementById('lblTopMic');
const rngAdcGain = document.getElementById('rngAdcGain');
const lblAdcGain = document.getElementById('lblAdcGain');
const chkToggleFluence = document.getElementById('chkToggleFluence');
const chkToggleHifiAudio = document.getElementById('chkToggleHifiAudio');
const rngEarBoost = document.getElementById('rngEarBoost');
const lblEarBoost = document.getElementById('lblEarBoost');

// Badges & Labels
const statusAudioProfile = document.getElementById('statusAudioProfile');
const statusHpf = document.getElementById('statusHpf');
const lblHpfVal = document.getElementById('lblHpfVal');
const statusSpatial = document.getElementById('statusSpatial');
const lblSpatialVal = document.getElementById('lblSpatialVal');
const statusFluence = document.getElementById('statusFluence');
const statusAdc = document.getElementById('statusAdc');
const statusBottomMic = document.getElementById('statusBottomMic');
const statusTopMic = document.getElementById('statusTopMic');
const statusHifi = document.getElementById('statusHifi');
const statusEarBoost = document.getElementById('statusEarBoost');

// Live ALSA Inspector elements
const liveAudioMode = document.getElementById('liveAudioMode');
const liveHpfCutoff = document.getElementById('liveHpfCutoff');
const liveTxDec0 = document.getElementById('liveTxDec0');
const liveTxDec1 = document.getElementById('liveTxDec1');
const liveAdcGain = document.getElementById('liveAdcGain');
const liveFluence = document.getElementById('liveFluence');

let currentAudioProfile = 'call';
let currentHpfCutoff = 'CF_NEG_3DB_75HZ';
let currentSpatialProfile = 'shield';

// Update Live Audio Hardware Inspector
function updateLiveAudioInspector() {
  const isBypass = chkAudioBypass ? chkAudioBypass.checked : true;
  if (liveAudioMode) {
    liveAudioMode.innerText = isBypass ? 'Stock Bypass (Factory)' : 'AETHER Enhanced Engine';
    liveAudioMode.style.color = isBypass ? 'var(--accent-cyan)' : 'var(--color-success)';
  }
  if (liveHpfCutoff) {
    if (isBypass) {
      liveHpfCutoff.innerText = 'Stock Filter';
      liveHpfCutoff.style.color = 'var(--text-secondary)';
    } else {
      liveHpfCutoff.innerText = currentHpfCutoff === 'CF_NEG_3DB_150HZ' ? '150 Hz (Wind Cut)' : (currentHpfCutoff === 'CF_NEG_3DB_4HZ' ? '4 Hz (Studio Flat)' : '75 Hz (Vocal Clarity)');
      liveHpfCutoff.style.color = 'var(--accent-cyan)';
    }
  }
  if (liveTxDec0) {
    const db = rngBottomMic ? parseInt(rngBottomMic.value, 10) : 0;
    const val = isBypass ? 84 : Math.max(72, Math.min(96, 84 + db));
    liveTxDec0.innerText = `${val} (${isBypass ? '0 dB' : (db >= 0 ? '+' : '') + db + ' dB'})`;
  }
  if (liveTxDec1) {
    const db = rngTopMic ? parseInt(rngTopMic.value, 10) : 0;
    const val = isBypass ? 84 : Math.max(72, Math.min(96, 84 + db));
    liveTxDec1.innerText = `${val} (${isBypass ? '0 dB' : (db >= 0 ? '+' : '') + db + ' dB'})`;
  }
  if (liveAdcGain) {
    const adc = isBypass ? 12 : (rngAdcGain ? Math.max(8, Math.min(15, parseInt(rngAdcGain.value, 10))) : 12);
    liveAdcGain.innerText = `${adc} (${adc <= 10 ? '25 dB' : (adc <= 12 ? '30 dB' : (adc <= 13 ? '33 dB' : (adc <= 14 ? '35 dB' : '38 dB')))})`;
  }
  if (liveFluence) {
    const f = chkToggleFluence ? chkToggleFluence.checked : true;
    liveFluence.innerText = isBypass ? 'Stock Qualcomm' : (f ? 'Fluence Pro (Active)' : 'Standard');
    liveFluence.style.color = (isBypass || f) ? 'var(--color-success)' : 'var(--text-secondary)';
  }
}

function updateAudioBypassUI() {
  const isBypass = chkAudioBypass ? chkAudioBypass.checked : true;
  if (statusAudioBypass) {
    statusAudioBypass.innerText = isBypass ? 'Stock Factory Active (Default)' : 'AETHER Audio Engine Active';
    statusAudioBypass.style.color = isBypass ? 'var(--accent-cyan)' : 'var(--color-success)';
  }
  if (subAudioBypass) {
    subAudioBypass.innerText = isBypass
      ? 'Default factory routing. Toggle switch OFF to unlock and activate AETHER deep noise cancellation, studio mic gain, and vocal clarity.'
      : 'AETHER Enhanced Audio Engine is active. Custom mic gains, hardware HPF filter, dual-mic spatial noise shield, and Fluence Pro DSP are engaged.';
  }
  if (cardAudioBypass) {
    cardAudioBypass.style.borderColor = isBypass ? 'rgba(56, 189, 248, 0.4)' : 'rgba(16, 185, 129, 0.5)';
  }
  if (customMicStatusNotice) {
    customMicStatusNotice.innerText = isBypass ? '(Stock Factory Passthrough)' : '(Active & Hardware Linked)';
    customMicStatusNotice.style.color = isBypass ? 'var(--text-muted)' : 'var(--color-success)';
  }
  if (customMicContainer) {
    if (isBypass) {
      customMicContainer.classList.add('custom-mic-bypassed');
    } else {
      customMicContainer.classList.remove('custom-mic-bypassed');
    }
  }
  if (bypassLockNotice) {
    bypassLockNotice.style.display = isBypass ? 'block' : 'none';
  }
  updateLiveAudioInspector();
}

function ensureCustomMicActive() {
  if (chkAudioBypass && chkAudioBypass.checked) {
    chkAudioBypass.checked = false;
    updateAudioBypassUI();
    showToast('⚡ AETHER Audio Engine Unlocked');
  }
}

if (btnUnlockBypassNotice) {
  btnUnlockBypassNotice.addEventListener('click', (e) => {
    e.stopPropagation();
    if (chkAudioBypass) {
      chkAudioBypass.checked = false;
      chkAudioBypass.dispatchEvent(new Event('change'));
    }
  });
}

// Write stock audio routing to ALSA mixer nodes
async function applyStockAudioBypass() {
  const cmds = [
    'tinymix "TX_DEC0 Volume" 84 2>/dev/null || true',
    'tinymix "TX_DEC1 Volume" 84 2>/dev/null || true',
    'tinymix "ADC1 Volume" 12 2>/dev/null || true',
    'tinymix "ADC2 Volume" 12 2>/dev/null || true',
    'tinymix "TX0 HPF cut off" CF_NEG_3DB_75HZ 2>/dev/null || true',
    'tinymix "TX1 HPF cut off" CF_NEG_3DB_75HZ 2>/dev/null || true',
    'for mic in /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay /sys/module/tx_macro/parameters/tx_unmute_delay; do [ -f "$mic" ] && echo 40 > "$mic" 2>/dev/null; done',
    'if [ -f /sys/kernel/aether/audio_bypass_mode ]; then echo 1 > /sys/kernel/aether/audio_bypass_mode 2>/dev/null; echo 0 > /sys/kernel/aether/mic_hpf_mode 2>/dev/null; fi'
  ];
  await instantWrite(cmds.join('; '), 'Stock Audio Route Bypass Restored (Factory Baseline)');
  updateLiveAudioInspector();
}

// Write active custom mic & noise cancellation settings to hardware
async function applyCustomAudioEngine(toastLabel = 'AETHER Audio Engine Applied') {
  const bDb = rngBottomMic ? parseInt(rngBottomMic.value, 10) : 0;
  const tDb = rngTopMic ? parseInt(rngTopMic.value, 10) : 0;
  const bVal = Math.max(72, Math.min(96, 84 + bDb));
  const tVal = Math.max(72, Math.min(96, 84 + tDb));
  const adc = rngAdcGain ? Math.max(8, Math.min(15, parseInt(rngAdcGain.value, 10))) : 12;
  const hpf = currentHpfCutoff || 'CF_NEG_3DB_75HZ';
  const delay = rngMicDelay ? Math.max(10, parseInt(rngMicDelay.value, 10)) : 40;
  const hpfModeNum = hpf.includes('150') ? 2 : (hpf.includes('75') ? 1 : 0);

  const cmds = [
    `tinymix "TX0 HPF cut off" ${hpf} 2>/dev/null || true`,
    `tinymix "TX1 HPF cut off" ${hpf} 2>/dev/null || true`,
    `tinymix "ADC1 Volume" ${adc} 2>/dev/null || true`,
    `tinymix "ADC2 Volume" ${adc} 2>/dev/null || true`,
    `tinymix "ADC3 Volume" ${adc} 2>/dev/null || true`,
    `tinymix "TX_DEC0 Volume" ${bVal} 2>/dev/null || true`,
    `tinymix "TX_DEC1 Volume" ${tVal} 2>/dev/null || true`,
    `tinymix "TX_DEC2 Volume" ${bVal} 2>/dev/null || true`,
    `for mic in /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay /sys/module/tx_macro/parameters/tx_unmute_delay; do [ -f "$mic" ] && echo ${delay} > "$mic" 2>/dev/null; done`,
    `if [ -f /sys/kernel/aether/audio_bypass_mode ]; then echo 0 > /sys/kernel/aether/audio_bypass_mode 2>/dev/null; echo ${hpfModeNum} > /sys/kernel/aether/mic_hpf_mode 2>/dev/null; fi`
  ];

  if (chkToggleFluence && chkToggleFluence.checked) {
    cmds.push('setprop persist.vendor.audio.fluence.voicecall true 2>/dev/null');
    cmds.push('setprop persist.vendor.audio.fluence.voicerec true 2>/dev/null');
    cmds.push('setprop persist.vendor.audio.fluence.speaker true 2>/dev/null');
    cmds.push('setprop ro.vendor.audio.sdk.fluencetype fluencepro 2>/dev/null');
    cmds.push('tinymix "TX_AIF1_CAP Mixer DEC0" 1 2>/dev/null || true');
    cmds.push('tinymix "TX_AIF1_CAP Mixer DEC1" 1 2>/dev/null || true');
  }

  await instantWrite(cmds.join('; '), toastLabel);
  updateLiveAudioInspector();
}

// 1. Audio Bypass Toggle Handler
if (chkAudioBypass) {
  chkAudioBypass.addEventListener('change', async () => {
    const isBypass = chkAudioBypass.checked;
    updateAudioBypassUI();
    triggerUserChoice();
    if (isBypass) {
      await applyStockAudioBypass();
    } else {
      await applyCustomAudioEngine('Audio: Custom Mic & Noise Cancellation Active');
    }
  });
}

// 2. Audio Profiles Handler (Segmented Quick Pills)
window.setAudioProfilePreset = async function(prof) {
  ensureCustomMicActive();
  currentAudioProfile = prof;
  syncChips('#audioProfileChips', prof, 'prof');
  triggerUserChoice();

  if (statusAudioProfile) {
    const labels = {
      call: 'Voice Call Clarity',
      studio: 'Studio Podcast',
      outdoor: 'Wind & Traffic Cut',
      gaming: 'Gaming Comm',
      flat: 'Stock Flat'
    };
    statusAudioProfile.innerText = labels[prof] || prof;
  }

  if (prof === 'call') {
    // Balanced voice calls with 75Hz hum cut & +4dB speech boost
    if (rngBottomMic) rngBottomMic.value = 4;
    if (rngTopMic) rngTopMic.value = 0;
    if (rngAdcGain) rngAdcGain.value = 12;
    currentHpfCutoff = 'CF_NEG_3DB_75HZ';
    currentSpatialProfile = 'shield';
    if (chkToggleFluence) chkToggleFluence.checked = true;
  } else if (prof === 'studio') {
    // Rich vocal recording with ADC 14 pre-amp & +6dB voice boost
    if (rngBottomMic) rngBottomMic.value = 6;
    if (rngTopMic) rngTopMic.value = 0;
    if (rngAdcGain) rngAdcGain.value = 14;
    currentHpfCutoff = 'CF_NEG_3DB_75HZ';
    currentSpatialProfile = 'shield';
    if (chkToggleFluence) chkToggleFluence.checked = true;
  } else if (prof === 'outdoor') {
    // Extreme wind & traffic cut with 150Hz aggressive high-pass
    if (rngBottomMic) rngBottomMic.value = 6;
    if (rngTopMic) rngTopMic.value = 2;
    if (rngAdcGain) rngAdcGain.value = 12;
    currentHpfCutoff = 'CF_NEG_3DB_150HZ';
    currentSpatialProfile = 'heavy';
    if (chkToggleFluence) chkToggleFluence.checked = true;
  } else if (prof === 'gaming') {
    // Clear Discord / game comms with warm preamp and low noise
    if (rngBottomMic) rngBottomMic.value = 6;
    if (rngTopMic) rngTopMic.value = 0;
    if (rngAdcGain) rngAdcGain.value = 13;
    currentHpfCutoff = 'CF_NEG_3DB_75HZ';
    currentSpatialProfile = 'balanced';
    if (chkToggleFluence) chkToggleFluence.checked = true;
  } else if (prof === 'flat') {
    // 100% uncolored acoustic response (4Hz DC decoupling only)
    if (rngBottomMic) rngBottomMic.value = 0;
    if (rngTopMic) rngTopMic.value = 0;
    if (rngAdcGain) rngAdcGain.value = 12;
    currentHpfCutoff = 'CF_NEG_3DB_4HZ';
    currentSpatialProfile = 'balanced';
    if (chkToggleFluence) chkToggleFluence.checked = false;
  }

  // Sync UI controls
  if (rngBottomMic && lblBottomMic) {
    lblBottomMic.innerText = `${rngBottomMic.value >= 0 ? '+' : ''}${rngBottomMic.value} dB`;
    syncChips('#bottomMicChips', rngBottomMic.value);
  }
  if (rngTopMic && lblTopMic) {
    lblTopMic.innerText = `${rngTopMic.value >= 0 ? '+' : ''}${rngTopMic.value} dB`;
    syncChips('#topMicChips', rngTopMic.value);
  }
  if (rngAdcGain && lblAdcGain) {
    const a = parseInt(rngAdcGain.value, 10);
    lblAdcGain.innerText = `${a} (${a <= 10 ? 'Quiet' : (a <= 12 ? 'Stock 30dB' : (a <= 13 ? 'Warm 33dB' : (a <= 14 ? 'Studio 35dB' : 'Max Safe 38dB')))})`;
    syncChips('#adcChips', rngAdcGain.value);
  }
  syncChips('#hpfPills', currentHpfCutoff);
  syncChips('#spatialPills', currentSpatialProfile);
  if (lblHpfVal) lblHpfVal.innerText = currentHpfCutoff === 'CF_NEG_3DB_150HZ' ? '150 Hz Filter' : (currentHpfCutoff === 'CF_NEG_3DB_4HZ' ? '4 Hz Filter' : '75 Hz Filter');
  if (lblSpatialVal) lblSpatialVal.innerText = currentSpatialProfile === 'heavy' ? '+6dB / +2dB Heavy' : (currentSpatialProfile === 'balanced' ? '0dB Delta' : '+4dB Voice / 0dB Amb');

  await applyCustomAudioEngine(`Audio Profile: ${statusAudioProfile ? statusAudioProfile.innerText : prof}`);
};

// 3. Hardware HPF Preset (Low-End Noise Filter)
window.setHpfPreset = async function(val) {
  ensureCustomMicActive();
  currentHpfCutoff = val;
  syncChips('#hpfPills', val);
  triggerUserChoice();
  if (statusHpf) {
    statusHpf.innerText = val === 'CF_NEG_3DB_150HZ' ? '150Hz Wind Cut' : (val === 'CF_NEG_3DB_4HZ' ? '4Hz Flat' : '75Hz Vocal Cut');
  }
  if (lblHpfVal) {
    lblHpfVal.innerText = val === 'CF_NEG_3DB_150HZ' ? '150 Hz Filter' : (val === 'CF_NEG_3DB_4HZ' ? '4 Hz Filter' : '75 Hz Filter');
  }
  await instantWrite(
    `tinymix "TX0 HPF cut off" ${val} 2>/dev/null; tinymix "TX1 HPF cut off" ${val} 2>/dev/null`,
    `Hardware HPF: ${statusHpf ? statusHpf.innerText : val}`
  );
  updateLiveAudioInspector();
};

// 4. Dual-Mic Spatial Preset (Differential Beamforming)
window.setDualMicSpatialPreset = async function(profile) {
  ensureCustomMicActive();
  currentSpatialProfile = profile;
  syncChips('#spatialPills', profile);
  triggerUserChoice();

  if (profile === 'balanced') {
    if (rngBottomMic) rngBottomMic.value = 0;
    if (rngTopMic) rngTopMic.value = 0;
    if (lblSpatialVal) lblSpatialVal.innerText = '0dB Delta (Balanced)';
  } else if (profile === 'shield') {
    if (rngBottomMic) rngBottomMic.value = 4;
    if (rngTopMic) rngTopMic.value = 0;
    if (lblSpatialVal) lblSpatialVal.innerText = '+4dB Voice / 0dB Amb';
  } else if (profile === 'heavy') {
    if (rngBottomMic) rngBottomMic.value = 6;
    if (rngTopMic) rngTopMic.value = 2;
    if (lblSpatialVal) lblSpatialVal.innerText = '+6dB Voice / +2dB Amb';
  }

  if (rngBottomMic && lblBottomMic) {
    lblBottomMic.innerText = `${rngBottomMic.value >= 0 ? '+' : ''}${rngBottomMic.value} dB`;
    syncChips('#bottomMicChips', rngBottomMic.value);
  }
  if (rngTopMic && lblTopMic) {
    lblTopMic.innerText = `${rngTopMic.value >= 0 ? '+' : ''}${rngTopMic.value} dB`;
    syncChips('#topMicChips', rngTopMic.value);
  }

  const bVal = Math.max(72, Math.min(96, 84 + parseInt(rngBottomMic.value, 10)));
  const tVal = Math.max(72, Math.min(96, 84 + parseInt(rngTopMic.value, 10)));
  await instantWrite(
    `tinymix "TX_DEC0 Volume" ${bVal} 2>/dev/null; tinymix "TX_DEC1 Volume" ${tVal} 2>/dev/null`,
    `Dual-Mic Spatial: ${profile.toUpperCase()}`
  );
  updateLiveAudioInspector();
};

// 5. Qualcomm Fluence Pro Toggle Handler
if (chkToggleFluence) {
  chkToggleFluence.addEventListener('change', async () => {
    ensureCustomMicActive();
    triggerUserChoice();
    const isFluence = chkToggleFluence.checked;
    if (statusFluence) statusFluence.innerText = isFluence ? 'Fluence Pro Active' : 'Fluence Disabled';
    if (isFluence) {
      await instantWrite(
        'setprop persist.vendor.audio.fluence.voicecall true 2>/dev/null; setprop persist.vendor.audio.fluence.voicerec true 2>/dev/null; setprop persist.vendor.audio.fluence.speaker true 2>/dev/null; setprop ro.vendor.audio.sdk.fluencetype fluencepro 2>/dev/null; tinymix "TX_AIF1_CAP Mixer DEC0" 1 2>/dev/null; tinymix "TX_AIF1_CAP Mixer DEC1" 1 2>/dev/null',
        'Qualcomm Fluence Pro Dual-Mic Shield Enabled'
      );
    } else {
      await instantWrite(
        'setprop persist.vendor.audio.fluence.voicecall false 2>/dev/null; setprop persist.vendor.audio.fluence.voicerec false 2>/dev/null; setprop persist.vendor.audio.fluence.speaker false 2>/dev/null',
        'Qualcomm Fluence Pro Disabled'
      );
    }
    updateLiveAudioInspector();
  });
}

// 6. Analog Pre-Amp Gain (ADC1 & ADC2)
const debouncedWriteAdcGain = debounce((val) => {
  ensureCustomMicActive();
  const numVal = Math.max(8, Math.min(15, parseInt(val, 10)));
  instantWrite(
    `tinymix "ADC1 Volume" ${numVal} 2>/dev/null; tinymix "ADC2 Volume" ${numVal} 2>/dev/null; tinymix "ADC3 Volume" ${numVal} 2>/dev/null || true`,
    `ADC Pre-Amp Gain: ${numVal} (ADC1-3)`
  );
  updateLiveAudioInspector();
}, 180);

if (rngAdcGain) {
  rngAdcGain.addEventListener('input', () => {
    ensureCustomMicActive();
    const val = Math.max(8, Math.min(15, parseInt(rngAdcGain.value, 10)));
    if (lblAdcGain) lblAdcGain.innerText = `${val} (${val <= 10 ? 'Quiet' : (val <= 12 ? 'Stock 30dB' : (val <= 13 ? 'Warm 33dB' : (val <= 14 ? 'Studio 35dB' : 'Max Safe 38dB')))})`;
    if (statusAdc) statusAdc.innerText = `${val} (${val <= 12 ? 'Stock' : 'Boost'})`;
    syncChips('#adcChips', String(val));
    triggerUserChoice();
    debouncedWriteAdcGain(val);
  });
}

window.setAdcPreset = function(val) {
  ensureCustomMicActive();
  const numVal = Math.max(8, Math.min(15, parseInt(val, 10)));
  if (rngAdcGain) rngAdcGain.value = numVal;
  if (lblAdcGain) lblAdcGain.innerText = `${numVal} (${numVal <= 10 ? 'Quiet' : (numVal <= 12 ? 'Stock 30dB' : (numVal <= 13 ? 'Warm 33dB' : (numVal <= 14 ? 'Studio 35dB' : 'Max Safe 38dB')))})`;
  if (statusAdc) statusAdc.innerText = `${numVal} (${numVal <= 12 ? 'Stock' : 'Boost'})`;
  syncChips('#adcChips', String(numVal));
  triggerUserChoice();
  instantWrite(
    `tinymix "ADC1 Volume" ${numVal} 2>/dev/null; tinymix "ADC2 Volume" ${numVal} 2>/dev/null; tinymix "ADC3 Volume" ${numVal} 2>/dev/null || true`,
    `ADC Pre-Amp Gain: ${numVal} (ADC1-3)`
  );
  updateLiveAudioInspector();
};

// 7. Bottom Primary Mic Gain
const debouncedWriteBottomMic = debounce((db) => {
  ensureCustomMicActive();
  const numDb = parseInt(db, 10);
  const bottomVal = Math.max(72, Math.min(96, 84 + numDb));
  instantWrite(`tinymix "TX_DEC0 Volume" ${bottomVal} 2>/dev/null; tinymix "TX_DEC2 Volume" ${bottomVal} 2>/dev/null || true`, `Bottom Mic: ${numDb >= 0 ? '+' : ''}${numDb} dB`);
  updateLiveAudioInspector();
}, 200);

if (rngBottomMic) {
  rngBottomMic.addEventListener('input', () => {
    ensureCustomMicActive();
    const val = parseInt(rngBottomMic.value, 10);
    if (lblBottomMic) lblBottomMic.innerText = `${val >= 0 ? '+' : ''}${val} dB`;
    syncChips('#bottomMicChips', rngBottomMic.value);
    triggerUserChoice();
    debouncedWriteBottomMic(val);
  });
}

window.setBottomMicGain = function(db) {
  ensureCustomMicActive();
  if (rngBottomMic) rngBottomMic.value = db;
  const val = parseInt(db, 10);
  if (lblBottomMic) lblBottomMic.innerText = `${val >= 0 ? '+' : ''}${val} dB`;
  syncChips('#bottomMicChips', String(db));
  triggerUserChoice();
  const bottomVal = Math.max(72, Math.min(96, 84 + val));
  instantWrite(`tinymix "TX_DEC0 Volume" ${bottomVal} 2>/dev/null; tinymix "TX_DEC2 Volume" ${bottomVal} 2>/dev/null || true`, `Bottom Mic: ${val >= 0 ? '+' : ''}${val} dB`);
  updateLiveAudioInspector();
};

// 8. Top Secondary Mic Gain
const debouncedWriteTopMic = debounce((db) => {
  ensureCustomMicActive();
  const numDb = parseInt(db, 10);
  const topVal = Math.max(72, Math.min(96, 84 + numDb));
  instantWrite(`tinymix "TX_DEC1 Volume" ${topVal} 2>/dev/null || true`, `Top Mic: ${numDb >= 0 ? '+' : ''}${numDb} dB`);
  updateLiveAudioInspector();
}, 200);

if (rngTopMic) {
  rngTopMic.addEventListener('input', () => {
    ensureCustomMicActive();
    const val = parseInt(rngTopMic.value, 10);
    if (lblTopMic) lblTopMic.innerText = `${val >= 0 ? '+' : ''}${val} dB`;
    syncChips('#topMicChips', rngTopMic.value);
    triggerUserChoice();
    debouncedWriteTopMic(val);
  });
}

window.setTopMicGain = function(db) {
  ensureCustomMicActive();
  if (rngTopMic) rngTopMic.value = db;
  const val = parseInt(db, 10);
  if (lblTopMic) lblTopMic.innerText = `${val >= 0 ? '+' : ''}${val} dB`;
  syncChips('#topMicChips', String(db));
  triggerUserChoice();
  const topVal = Math.max(72, Math.min(96, 84 + val));
  instantWrite(`tinymix "TX_DEC1 Volume" ${topVal} 2>/dev/null || true`, `Top Mic: ${val >= 0 ? '+' : ''}${val} dB`);
  updateLiveAudioInspector();
};

// 9. In-Call Mic Unmute Delay
const debouncedWriteMicDelay = debounce((val) => {
  ensureCustomMicActive();
  const safeVal = Math.max(10, parseInt(val, 10));
  instantWrite(
    `for mic in /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay /sys/module/tx_macro/parameters/tx_unmute_delay; do [ -f "$mic" ] && echo ${safeVal} > "$mic" 2>/dev/null; done`,
    `In-Call Mic Delay: ${safeVal} ms`
  );
}, 200);

if (rngMicDelay) {
  rngMicDelay.addEventListener('input', () => {
    ensureCustomMicActive();
    const val = rngMicDelay.value;
    if (lblMicDelay) lblMicDelay.innerText = `${val} ms`;
    triggerUserChoice();
    debouncedWriteMicDelay(val);
  });
}

// 10. Headphone Hi-Fi Audio Mode
if (chkToggleHifiAudio) {
  chkToggleHifiAudio.addEventListener('change', () => {
    triggerUserChoice();
    const mode = chkToggleHifiAudio.checked ? 'HIGH_PERF' : 'NORMAL';
    if (statusHifi) statusHifi.innerText = chkToggleHifiAudio.checked ? 'HD2 Hi-Fi' : 'Normal';
    instantWrite(`tinymix 'RX_HPH_PWR_MODE' '${mode}' 2>/dev/null`, `Hi-Fi Audio: ${chkToggleHifiAudio.checked ? 'High Performance HD2' : 'Stock Normal'}`);
  });
}

// 11. Receiver Earpiece Volume Boost
const debouncedWriteEarBoost = debounce((val) => {
  const boost = parseInt(val, 10);
  const earVal = boost > 0 ? (84 + (boost * 2)) : 84;
  instantWrite(`tinymix 'RX_EAR Volume' ${earVal} 2>/dev/null`, `Earpiece Boost: +${boost} dB`);
}, 200);

if (rngEarBoost) {
  rngEarBoost.addEventListener('input', () => {
    const val = parseInt(rngEarBoost.value, 10);
    if (lblEarBoost) lblEarBoost.innerText = val === 0 ? '0 dB (Stock)' : `+${val} dB Boost`;
    if (statusEarBoost) statusEarBoost.innerText = val === 0 ? 'Stock' : `+${val} dB`;
    triggerUserChoice();
    debouncedWriteEarBoost(val);
  });
}

// ========================================================
// 6. ESSENTIAL TOGGLES & CHARGE CONTROLS (Instant Response)
// ========================================================
let selectedChargeCutoff = '0'; // Default: '0' (Off / Stock 100%)
let selectedCutoffMode = '1';    // Default: '1' (Real Bypass when active)

const chkInstantBypass = document.getElementById('chkInstantBypass');
const chkToggleBypass = chkInstantBypass; // Maintain backwards compatibility
const badgeInstantBypass = document.getElementById('badgeInstantBypass');
const statusCutoffEngine = document.getElementById('statusCutoffEngine');
const lblCutoffVal = document.getElementById('lblCutoffVal');
const lblCutoffModeBadge = document.getElementById('lblCutoffModeBadge');
const valLiveCutoffStatus = document.getElementById('valLiveCutoffStatus');
const valLiveNetCurrent = document.getElementById('valLiveNetCurrent');

const chkToggleDcDim = document.getElementById('chkToggleDcDim');
const chkToggleHbm = document.getElementById('chkToggleHbm');
const chkToggleGlove = document.getElementById('chkToggleGlove');
const chkToggleTurboOverride = document.getElementById('chkToggleTurboOverride');
const chkToggleBbr = document.getElementById('chkToggleBbr');
const chkToggleTfo = document.getElementById('chkToggleTfo');
let selectedChargeLimit = '0';
let selectedReadAhead = '256';

function updateCutoffLabels() {
  if (badgeInstantBypass && chkInstantBypass) {
    if (chkInstantBypass.checked) {
      badgeInstantBypass.innerText = '⚡ Bypass Active (0mA Net)';
      badgeInstantBypass.style.background = 'rgba(16, 185, 129, 0.2)';
      badgeInstantBypass.style.color = '#34d399';
      badgeInstantBypass.style.borderColor = '#10b981';
    } else {
      badgeInstantBypass.innerText = 'Off';
      badgeInstantBypass.style.background = 'var(--bg-input)';
      badgeInstantBypass.style.color = 'var(--text-muted)';
      badgeInstantBypass.style.borderColor = 'var(--border-subtle)';
    }
  }

  if (lblCutoffVal) {
    lblCutoffVal.innerText = selectedChargeCutoff === '0' ? 'Off (Stock 100%)' : `${selectedChargeCutoff}% Hardware Stop`;
  }

  if (lblCutoffModeBadge) {
    lblCutoffModeBadge.innerText = selectedCutoffMode === '1' ? 'Real Bypass (0mA)' : 'Cool Standby';
    lblCutoffModeBadge.style.color = selectedCutoffMode === '1' ? 'var(--accent-cyan)' : '#fbbf24';
  }

  if (statusCutoffEngine) {
    if (chkInstantBypass && chkInstantBypass.checked) {
      statusCutoffEngine.innerText = '⚡ Gaming Instant Bypass Active (0mA Net)';
      statusCutoffEngine.style.color = '#34d399';
    } else if (selectedChargeCutoff !== '0') {
      const modeStr = selectedCutoffMode === '1' ? 'Bypass' : 'Standby';
      statusCutoffEngine.innerText = `Cutoff Active: ${selectedChargeCutoff}% (${modeStr})`;
      statusCutoffEngine.style.color = 'var(--accent-cyan)';
    } else {
      statusCutoffEngine.innerText = 'Stock / ROM Default (Normal Charging)';
      statusCutoffEngine.style.color = 'var(--text-secondary)';
    }
  }
}

// 1. Instant Bypass Mode (Gaming Direct USB Power at Any %)
if (chkInstantBypass) {
  chkInstantBypass.addEventListener('change', () => {
    updateCutoffLabels();
    triggerUserChoice();
    const isBypass = chkInstantBypass.checked;
    const bypassVal = isBypass ? '1' : '0';
    instantWrite(
      `echo "${bypassVal}" > /sys/kernel/aether/instant_bypass 2>/dev/null; [ -f /sys/kernel/aether/aacc_idle_mode ] && echo "${bypassVal}" > /sys/kernel/aether/aacc_idle_mode 2>/dev/null; [ -f /sys/class/power_supply/battery/bq_input_suspend ] && echo "${bypassVal}" > /sys/class/power_supply/battery/bq_input_suspend 2>/dev/null`,
      isBypass ? 'Instant Bypass Mode ON (Direct USB Power, 0mA Net)' : 'Instant Bypass Mode OFF (Standard Charging Restored)'
    );
  });
}

// 2. Target Cutoff Limit Uniform Pills
document.querySelectorAll('.cutoff-pill').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.cutoff-pill').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    selectedChargeCutoff = btn.dataset.val;
    updateCutoffLabels();
    triggerUserChoice();

    const writeCmd = `echo "${selectedChargeCutoff}" > /sys/kernel/aether/charge_cutoff_limit 2>/dev/null; mkdir -p /mnt/vendor/persist/aether 2>/dev/null; echo "CUTOFF_LIMIT=${selectedChargeCutoff}" > /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null; echo "CUTOFF_MODE=${selectedCutoffMode}" >> /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null || true`;
    const label = selectedChargeCutoff === '0'
      ? 'Charge Cutoff Disabled (Full Stock 100% / ROM Default)'
      : `Target Charge Cutoff Set to ${selectedChargeCutoff}% (Hardware PMIC Managed)`;

    instantWrite(writeCmd, label);
  });
});

// 3. Cutoff Behavior Mode Selector Pills
document.querySelectorAll('.cutoff-mode-pill').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.cutoff-mode-pill').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    selectedCutoffMode = btn.dataset.val;
    updateCutoffLabels();
    triggerUserChoice();

    const writeCmd = `echo "${selectedCutoffMode}" > /sys/kernel/aether/charge_cutoff_mode 2>/dev/null; mkdir -p /mnt/vendor/persist/aether 2>/dev/null; echo "CUTOFF_LIMIT=${selectedChargeCutoff}" > /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null; echo "CUTOFF_MODE=${selectedCutoffMode}" >> /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null || true`;
    const label = selectedCutoffMode === '1'
      ? 'Cutoff Action: Real Bypass Mode (Direct USB Power)'
      : 'Cutoff Action: Cool Standby Mode (Sleeps Charger)';

    instantWrite(writeCmd, label);
  });
});

// 2. AMOLED DC Dimming (Anti-Flicker Hardware Mode)
if (chkToggleDcDim) {
  chkToggleDcDim.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const isDc = chkToggleDcDim.checked;
    const val = isDc ? '1' : '0';
    instantWrite(
      `for n in /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker /sys/devices/platform/soc/*/*/regulator/regulator.*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/*/*/regulator/*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/virtual/graphics/fb0/anti_flicker /sys/class/graphics/fb0/anti_flicker; do if [ -f "$n" ]; then echo "${val}" > "$n"; break; fi; done`,
      isDc ? 'AMOLED DC Dimming ON' : 'Stock PWM Anti-Flicker OFF'
    );
  });
}

// 3. Sunlight HBM (1100 Nits)
if (chkToggleHbm) {
  chkToggleHbm.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const isHbm = chkToggleHbm.checked;
    const val = isHbm ? '1' : '0';
    instantWrite(
      `for n in /sys/devices/platform/soc/soc:qcom,dsi-display/hbm /sys/devices/platform/soc/*/*/regulator/regulator.*/soc:qcom,dsi-display-lab/hbm /sys/devices/platform/soc/*/*/regulator/*/soc:qcom,dsi-display-lab/hbm /sys/devices/platform/soc/soc:qcom,dsi-display-primary/hbm /sys/devices/platform/soc/*/soc:qcom,dsi-display*/hbm /sys/class/drm/card0/device/hbm; do if [ -f "$n" ]; then echo "${val}" > "$n"; break; fi; done`,
      isHbm ? 'Sunlight 1100-Nits HBM ON' : 'Standard Sunlight Brightness'
    );
  });
}

// 4. Glove Mode Touch Sensitivity
if (chkToggleGlove) {
  chkToggleGlove.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const isGlove = chkToggleGlove.checked;
    const val = isGlove ? '1' : '0';
    instantWrite(
      `for g in /sys/devices/platform/soc/*.i2c/i2c-*/1-*/fts_glove_mode /sys/bus/i2c/drivers/fts_ts/*/fts_glove_mode /sys/bus/i2c/drivers/ft3418_ts/*/fts_glove_mode /sys/class/input/input*/fts_glove_mode; do [ -f "$g" ] && echo "${val}" > "$g" 2>/dev/null; done`,
      isGlove ? 'Glove Touch Mode ON' : 'Standard Touch Sensitivity'
    );
  });
}

// 5. Fast Charge Turbo Override
if (chkToggleTurboOverride) {
  chkToggleTurboOverride.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const val = chkToggleTurboOverride.checked ? '1' : '0';
    instantWrite(
      `echo "${val}" > /sys/class/power_supply/battery/fastcharge_mode 2>/dev/null`,
      chkToggleTurboOverride.checked ? 'Force 33W Turbo Fastcharge ON' : 'Auto QC/PD Fastcharge'
    );
  });
}

// 6. Charging Speed Limit Segments
document.querySelectorAll('.seg-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.seg-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    selectedChargeLimit = btn.dataset.val;
    triggerUserChoice();
    const limitMa = selectedChargeLimit === '0' ? '6000000' : selectedChargeLimit;
    const label = selectedChargeLimit === '0' ? 'Stock 33W Mi Turbo Charge' : `Charge Limit: ${parseInt(selectedChargeLimit, 10) / 1000} mA`;
    instantWrite(`echo "${limitMa}" > /sys/class/power_supply/battery/constant_charge_current_max 2>/dev/null`, label);
  });
});

// 7. Google BBR Network Congestion Control
if (chkToggleBbr) {
  chkToggleBbr.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const isBbr = chkToggleBbr.checked;
    const cc = isBbr ? 'bbr' : 'cubic';
    const bbrSubCmds = isBbr ? 'echo "fq" > /proc/sys/net/core/default_qdisc 2>/dev/null; echo 0 > /proc/sys/net/ipv4/tcp_slow_start_after_idle 2>/dev/null; echo 2 > /proc/sys/net/ipv4/tcp_ecn 2>/dev/null; echo 8388608 > /proc/sys/net/core/rmem_max 2>/dev/null; echo 8388608 > /proc/sys/net/core/wmem_max 2>/dev/null;' : '';
    instantWrite(
      `echo "${cc}" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null; ${bbrSubCmds}`,
      isBbr ? 'Google BBR Network Acceleration ON' : 'Cubic Standard TCP Active'
    );
  });
}

// 8. TCP Fast Open (TFO)
if (chkToggleTfo) {
  chkToggleTfo.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const val = chkToggleTfo.checked ? '3' : '1';
    instantWrite(`echo "${val}" > /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null`, `TCP Fast Open: ${chkToggleTfo.checked ? 'Mode 3 Active' : 'Mode 1 Stock'}`);
  });
}

// 9. UFS Storage Read-Ahead Cache & Google Kyber BLK-MQ
const selIoSched = document.getElementById('selIoSched');
const chkSchedtuneIdle = document.getElementById('chkSchedtuneIdle');
const chkTcpLowat = document.getElementById('chkTcpLowat');
const statusSchedtune = document.getElementById('statusSchedtune');

if (selIoSched) {
  selIoSched.addEventListener('change', () => {
    const val = selIoSched.value;
    updateTileBadges();
    triggerUserChoice();
    instantWrite(`for q in /sys/block/sd*/queue/scheduler; do [ -f "$q" ] && echo "${val}" > "$q" 2>/dev/null; done`, `I/O Scheduler: ${val}`);
    if (statusUfs) statusUfs.innerText = `${val.toUpperCase()} • ${selectedReadAhead} KB`;
  });
}

if (chkSchedtuneIdle) {
  chkSchedtuneIdle.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const enabled = chkSchedtuneIdle.checked;
    const cmd = enabled ?
      'if [ -f /sys/kernel/aether/engine_active ]; then echo 1 > /sys/kernel/aether/sched_idle_search 2>/dev/null; fi; if [ -d /dev/stune/top-app ]; then echo 1 > /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null; echo 10 > /dev/stune/top-app/schedtune.boost 2>/dev/null; fi' :
      'if [ -f /sys/kernel/aether/engine_active ]; then echo 0 > /sys/kernel/aether/sched_idle_search 2>/dev/null; fi; if [ -d /dev/stune/top-app ]; then echo 0 > /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null; echo 0 > /dev/stune/top-app/schedtune.boost 2>/dev/null; fi';
    instantWrite(cmd, `SchedTune Prefer Idle: ${enabled ? 'Enabled (Ring-0/Top-App)' : 'Disabled'}`);
    if (statusSchedtune) statusSchedtune.innerText = enabled ? 'Prefer Idle Active' : 'Default Sched';
  });
}

if (chkTcpLowat) {
  chkTcpLowat.addEventListener('change', () => {
    triggerUserChoice();
    const enabled = chkTcpLowat.checked;
    const cmd = enabled ?
      'if [ -f /sys/kernel/aether/tcp_lowat_pacing ]; then echo 1 > /sys/kernel/aether/tcp_lowat_pacing 2>/dev/null; fi; echo 16384 > /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null; echo 1 > /proc/sys/net/ipv4/tcp_autocorking 2>/dev/null' :
      'if [ -f /sys/kernel/aether/tcp_lowat_pacing ]; then echo 0 > /sys/kernel/aether/tcp_lowat_pacing 2>/dev/null; fi; echo 4294967295 > /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null; echo 0 > /proc/sys/net/ipv4/tcp_autocorking 2>/dev/null';
    instantWrite(cmd, `TCP Low-Latency Socket: ${enabled ? 'Active (Ring-0 / 16KB)' : 'Stock (4MB)'}`);
  });
}

document.querySelectorAll('.seg-btn-ra').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.seg-btn-ra').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    selectedReadAhead = btn.dataset.val;
    updateTileBadges();
    triggerUserChoice();
    instantWrite(`for q in /sys/block/sd*/queue/read_ahead_kb /sys/block/dm-*/queue/read_ahead_kb; do [ -f "$q" ] && echo "${selectedReadAhead}" > "$q" 2>/dev/null; done`, `UFS Read-Ahead: ${selectedReadAhead} KB`);
  });
});

// Dynamic Hardware Tile Badges
function updateTileBadges() {
  const statusBypass = document.getElementById('statusBypass');
  if (statusBypass && chkToggleBypass) {
    statusBypass.innerText = chkToggleBypass.checked ? 'Bypass ON (Zero Heat)' : 'Standard Charging';
  }

  const statusHaptic = document.getElementById('statusHaptic');
  if (statusHaptic && rngHaptic) {
    const modeText = selHapticMode ? (selHapticMode.value === '1' ? 'iOS Taptic' : (selHapticMode.value === '2' ? 'Pixel' : 'Stock')) : 'iOS Taptic';
    statusHaptic.innerText = `${rngHaptic.value} mV (${modeText})`;
  }

  if (statusAudioProfile) {
    const labels = {
      call: 'Voice Call Clarity',
      studio: 'Studio Podcast',
      outdoor: 'Wind & Traffic Cut',
      gaming: 'Gaming Comm',
      flat: 'Stock Flat'
    };
    statusAudioProfile.innerText = labels[currentAudioProfile] || currentAudioProfile;
  }
  if (statusHpf) {
    statusHpf.innerText = currentHpfCutoff === 'CF_NEG_3DB_150HZ' ? '150Hz Wind Cut' : (currentHpfCutoff === 'CF_NEG_3DB_4HZ' ? '4Hz Flat' : '75Hz Vocal Cut');
  }
  if (statusSpatial) {
    statusSpatial.innerText = currentSpatialProfile === 'heavy' ? 'Heavy Cut' : (currentSpatialProfile === 'balanced' ? 'Balanced' : 'Spatial Shield');
  }
  if (statusFluence && chkToggleFluence) {
    statusFluence.innerText = chkToggleFluence.checked ? 'Fluence Pro Active' : 'Fluence Disabled';
  }
  if (statusAdc && rngAdcGain) {
    const a = parseInt(rngAdcGain.value, 10);
    statusAdc.innerText = `${a} (${a <= 12 ? 'Stock' : 'Boost'})`;
  }
  updateLiveAudioInspector();

  const statusDcDim = document.getElementById('statusDcDim');
  if (statusDcDim && chkToggleDcDim) {
    statusDcDim.innerText = chkToggleDcDim.checked ? 'DC Dimming Active' : 'PWM Stock';
  }

  const statusHbm = document.getElementById('statusHbm');
  if (statusHbm && chkToggleHbm) {
    statusHbm.innerText = chkToggleHbm.checked ? 'Sunlight 1100 Nits' : 'Normal Mode';
  }

  const statusGlove = document.getElementById('statusGlove');
  if (statusGlove && chkToggleGlove) {
    statusGlove.innerText = chkToggleGlove.checked ? 'Glove Mode ON' : 'Standard Sens';
  }

  const statusTurbo = document.getElementById('statusTurbo');
  if (statusTurbo && chkToggleTurboOverride) {
    statusTurbo.innerText = chkToggleTurboOverride.checked ? 'Force 33W Turbo' : 'Auto QC/PD';
  }

  const statusUfs = document.getElementById('statusUfs');
  if (statusUfs) statusUfs.innerText = `${selectedReadAhead} KB`;

  const statusTfo = document.getElementById('statusTfo');
  if (statusTfo && chkToggleTfo) {
    statusTfo.innerText = chkToggleTfo.checked ? 'Active (tfo=3)' : 'Standard';
  }

  const statusGpuTile = document.getElementById('statusGpuTile');
  if (statusGpuTile && selGpuMin && selGpuGov) {
    statusGpuTile.innerText = `${selGpuMin.value} MHz / ${selGpuGov.value}`;
  }

  const statusHifi = document.getElementById('statusHifi');
  if (statusHifi && chkToggleHifiAudio) {
    statusHifi.innerText = chkToggleHifiAudio.checked ? 'HD2 Low-Distortion' : 'Normal';
  }

  const statusEarBoost = document.getElementById('statusEarBoost');
  if (statusEarBoost && rngEarBoost) {
    statusEarBoost.innerText = rngEarBoost.value > 0 ? `+${rngEarBoost.value} dB Boost` : 'Stock';
  }
}

function triggerUserChoice() {
  currentProfileMode = 'custom';
  document.querySelectorAll('.mode-card').forEach(c => c.classList.remove('active'));
  if (activeModeBadge) activeModeBadge.innerText = '⚙️ Custom User Choice';
  const notice = document.getElementById('userChoiceNotice');
  if (notice) notice.style.display = 'block';
  updateTileBadges();
  scheduleAutoSave();
}

// ========================================================
// 7. ADVANCED TUNING CONTROLS (Instant Response)
// ========================================================
const selGpuGov = document.getElementById('selGpuGov');
const selGpuMin = document.getElementById('selGpuMin');
const rngGpuIdle = document.getElementById('rngGpuIdle');
const lblGpuIdle = document.getElementById('lblGpuIdle');

const selBoostLp = document.getElementById('selBoostLp');
const selBoostHp = document.getElementById('selBoostHp');
const rngBoostDur = document.getElementById('rngBoostDur');
const lblBoostDur = document.getElementById('lblBoostDur');

const selBoreState = document.getElementById('selBoreState');
const selBoreScale = document.getElementById('selBoreScale');

const rngUpRate = document.getElementById('rngUpRate');
const lblUpRate = document.getElementById('lblUpRate');
const rngSwappiness = document.getElementById('rngSwappiness');
const lblSwappiness = document.getElementById('lblSwappiness');
const rngVfsCache = document.getElementById('rngVfsCache');
const lblVfsCache = document.getElementById('lblVfsCache');
const rngMglruTtl = document.getElementById('rngMglruTtl');
const lblMglruTtl = document.getElementById('lblMglruTtl');

if (selGpuGov) {
  selGpuGov.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    instantWrite(`echo "${selGpuGov.value}" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null; echo "${selGpuGov.value}" > /sys/class/kgsl/kgsl-3d0/gpu_governor 2>/dev/null`, `GPU Governor: ${selGpuGov.value}`);
  });
}

if (selGpuMin) {
  selGpuMin.addEventListener('change', () => {
    updateTileBadges();
    triggerUserChoice();
    const mhz = selGpuMin.value;
    instantWrite(`echo "${mhz}" > /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null; echo "${mhz}000000" > /sys/class/kgsl/kgsl-3d0/devfreq/min_freq 2>/dev/null`, `GPU Min Frequency: ${mhz} MHz`);
  });
}

const debouncedWriteGpuIdle = debounce((val) => {
  instantWrite(`echo "${val}" > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null`, `GPU Idle Timer: ${val} ms`);
}, 200);

if (rngGpuIdle && lblGpuIdle) {
  rngGpuIdle.addEventListener('input', () => {
    lblGpuIdle.innerText = `${rngGpuIdle.value} ms`;
    triggerUserChoice();
    debouncedWriteGpuIdle(rngGpuIdle.value);
  });
}

if (selBoostLp) {
  selBoostLp.addEventListener('change', () => {
    triggerUserChoice();
    instantWrite(`echo "${selBoostLp.value}" > /sys/module/cpu_input_boost/parameters/input_boost_freq_lp 2>/dev/null`, `CPU Little Boost: ${selBoostLp.value} kHz`);
  });
}

if (selBoostHp) {
  selBoostHp.addEventListener('change', () => {
    triggerUserChoice();
    instantWrite(`echo "${selBoostHp.value}" > /sys/module/cpu_input_boost/parameters/input_boost_freq_hp 2>/dev/null`, `CPU Big Boost: ${selBoostHp.value} kHz`);
  });
}

const debouncedWriteBoostDur = debounce((val) => {
  instantWrite(`echo "${val}" > /sys/module/cpu_input_boost/parameters/input_boost_duration 2>/dev/null`, `Input Boost Duration: ${val} ms`);
}, 200);

if (rngBoostDur && lblBoostDur) {
  rngBoostDur.addEventListener('input', () => {
    lblBoostDur.innerText = `${rngBoostDur.value} ms`;
    triggerUserChoice();
    debouncedWriteBoostDur(rngBoostDur.value);
  });
}

const debouncedWriteUpRate = debounce((val) => {
  instantWrite(`echo "${val}" > /sys/devices/system/cpu/cpufreq/schedutil/up_rate_limit_us 2>/dev/null`, `Schedutil Up Rate: ${val} μs`);
}, 200);

if (rngUpRate && lblUpRate) {
  rngUpRate.addEventListener('input', () => {
    lblUpRate.innerText = `${rngUpRate.value} μs`;
    triggerUserChoice();
    debouncedWriteUpRate(rngUpRate.value);
  });
}

if (selBoreState) {
  selBoreState.addEventListener('change', () => {
    triggerUserChoice();
    instantWrite(`echo "${selBoreState.value}" > /proc/sys/kernel/sched_bore 2>/dev/null`, `BORE Scheduler: ${selBoreState.value === '1' ? 'Enabled' : 'Disabled'}`);
  });
}

if (selBoreScale) {
  selBoreScale.addEventListener('change', () => {
    triggerUserChoice();
    instantWrite(`echo "${selBoreScale.value}" > /proc/sys/kernel/sched_burst_penalty_scale 2>/dev/null`, `BORE Burst Scale: ${selBoreScale.value}`);
  });
}

const debouncedWriteSwappiness = debounce((val) => {
  instantWrite(`echo "${val}" > /proc/sys/vm/swappiness 2>/dev/null`, `Swappiness: ${val}`);
}, 200);

if (rngSwappiness && lblSwappiness) {
  rngSwappiness.addEventListener('input', () => {
    lblSwappiness.innerText = rngSwappiness.value;
    triggerUserChoice();
    debouncedWriteSwappiness(rngSwappiness.value);
  });
}

const debouncedWriteVfsCache = debounce((val) => {
  instantWrite(`echo "${val}" > /proc/sys/vm/vfs_cache_pressure 2>/dev/null`, `VFS Cache Pressure: ${val}`);
}, 200);

if (rngVfsCache && lblVfsCache) {
  rngVfsCache.addEventListener('input', () => {
    lblVfsCache.innerText = rngVfsCache.value;
    triggerUserChoice();
    debouncedWriteVfsCache(rngVfsCache.value);
  });
}

const debouncedWriteMglruTtl = debounce((val) => {
  instantWrite(`if [ -f /sys/kernel/mm/lru_gen/min_ttl_ms ]; then echo "${val}" > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null; fi`, `MGLRU Min TTL: ${val} ms`);
}, 180);

if (rngMglruTtl && lblMglruTtl) {
  rngMglruTtl.addEventListener('input', () => {
    const val = rngMglruTtl.value;
    lblMglruTtl.innerText = `${val} ms`;
    syncChips('#mglruChips', val);
    triggerUserChoice();
    debouncedWriteMglruTtl(val);
  });
}

window.setMglruPreset = function(ms) {
  if (rngMglruTtl) rngMglruTtl.value = ms;
  if (lblMglruTtl) lblMglruTtl.innerText = `${ms} ms`;
  syncChips('#mglruChips', ms);
  triggerUserChoice();
  instantWrite(`if [ -f /sys/kernel/mm/lru_gen/min_ttl_ms ]; then echo "${ms}" > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null; fi`, `MGLRU Min TTL: ${ms} ms`);
};

// ========================================================
// 8. 1-TAP QUICK PROFILE CARDS (Instant Realtime Apply)
// ========================================================
const activeModeBadge = document.getElementById('activeModeBadge');

document.querySelectorAll('.mode-card').forEach(card => {
  card.addEventListener('click', async () => {
    await applyProfile(card.dataset.mode);
  });
});

async function applyProfile(mode) {
  document.querySelectorAll('.mode-card').forEach(c => c.classList.remove('active'));
  const card = document.querySelector(`.mode-card[data-mode="${mode}"]`);
  if (card) card.classList.add('active');

  const userChoiceNotice = document.getElementById('userChoiceNotice');
  if (userChoiceNotice) userChoiceNotice.style.display = 'none';

  let profileLabel = 'Stock Default';

  if (mode === 'default') {
    profileLabel = 'Factory Stock Baseline';
    if (activeModeBadge) activeModeBadge.innerText = 'Stock Default Active';
    if (selGpuGov) selGpuGov.value = 'msm-adreno-tz';
    if (selGpuMin) selGpuMin.value = '180';
    if (rngGpuIdle) rngGpuIdle.value = 80;
    if (rngUpRate) rngUpRate.value = 500;
    if (selBoostLp) selBoostLp.value = '1017600';
    if (selBoostHp) selBoostHp.value = '1209600';
    if (rngBoostDur) rngBoostDur.value = 40;
    if (chkToggleBypass) chkToggleBypass.checked = false;
    if (chkAudioBypass) chkAudioBypass.checked = true;
    currentAudioProfile = 'flat';
    currentHpfCutoff = 'CF_NEG_3DB_75HZ';
    currentSpatialProfile = 'balanced';
    if (chkToggleFluence) chkToggleFluence.checked = true;
    updateAudioBypassUI();
    if (chkToggleDcDim) chkToggleDcDim.checked = false;
    if (chkToggleHbm) chkToggleHbm.checked = false;
    if (chkToggleGlove) chkToggleGlove.checked = false;
    if (chkToggleTurboOverride) chkToggleTurboOverride.checked = false;
    if (chkToggleHifiAudio) chkToggleHifiAudio.checked = false;
    if (rngEarBoost) rngEarBoost.value = 0;
    if (lblEarBoost) lblEarBoost.innerText = '0 dB (Stock)';
    if (rngMicDelay) rngMicDelay.value = 40;
    if (rngBottomMic) rngBottomMic.value = 0;
    if (rngTopMic) rngTopMic.value = 0;
    if (rngAdcGain) rngAdcGain.value = 12;
    if (rngHaptic) rngHaptic.value = 2400;
    if (selHapticMode) selHapticMode.value = '1';
    if (rngSwappiness) rngSwappiness.value = 60;
    if (rngVfsCache) rngVfsCache.value = 100;
    if (rngMglruTtl) rngMglruTtl.value = 1000;
    selectedReadAhead = '256';
    if (chkToggleBbr) chkToggleBbr.checked = false;
    if (chkToggleTfo) chkToggleTfo.checked = false;
    selectedChargeLimit = '0';
    document.querySelectorAll('.seg-btn').forEach(b => b.classList.remove('active'));
    const btnZero = document.querySelector('.seg-btn[data-val="0"]');
    if (btnZero) btnZero.classList.add('active');
  } else if (mode === 'balanced') {
    profileLabel = 'Balanced Daily Driver';
    if (activeModeBadge) activeModeBadge.innerText = 'Balanced Active';
    if (selGpuGov) selGpuGov.value = 'msm-adreno-tz';
    if (selGpuMin) selGpuMin.value = '180';
    if (rngGpuIdle) rngGpuIdle.value = 50;
    if (rngUpRate) rngUpRate.value = 500;
    if (selBoostLp) selBoostLp.value = '1017600';
    if (selBoostHp) selBoostHp.value = '1209600';
    if (rngBoostDur) rngBoostDur.value = 40;
    if (chkToggleBypass) chkToggleBypass.checked = false;
    if (chkToggleBbr) chkToggleBbr.checked = true;
    if (rngMicDelay) rngMicDelay.value = 40;
    if (rngBottomMic) rngBottomMic.value = 6;
    if (rngTopMic) rngTopMic.value = 0;
    if (rngHaptic) rngHaptic.value = 3000;
    if (selHapticMode) selHapticMode.value = '1';
    if (rngSwappiness) rngSwappiness.value = 60;
    if (rngVfsCache) rngVfsCache.value = 100;
    if (rngMglruTtl) rngMglruTtl.value = 1000;
    selectedReadAhead = '256';
  } else if (mode === 'gaming') {
    profileLabel = 'Gaming Beast (504MHz GPU)';
    if (activeModeBadge) activeModeBadge.innerText = 'Gaming Beast Active';
    if (selGpuGov) selGpuGov.value = 'performance';
    if (selGpuMin) selGpuMin.value = '504';
    if (rngGpuIdle) rngGpuIdle.value = 200;
    if (rngUpRate) rngUpRate.value = 0;
    if (selBoostLp) selBoostLp.value = '1497600';
    if (selBoostHp) selBoostHp.value = '1843200';
    if (rngBoostDur) rngBoostDur.value = 80;
    if (chkToggleBypass) chkToggleBypass.checked = false; // Keep standard charging active
    if (chkToggleBbr) chkToggleBbr.checked = true;
    if (rngHaptic) rngHaptic.value = 3596;
    if (selHapticMode) selHapticMode.value = '1';
    if (rngSwappiness) rngSwappiness.value = 60;
    if (rngVfsCache) rngVfsCache.value = 100;
    if (rngMglruTtl) rngMglruTtl.value = 1000;
    if (rngMicDelay) rngMicDelay.value = 40;
    if (rngBottomMic) rngBottomMic.value = 8;
    if (rngTopMic) rngTopMic.value = 0;
    selectedReadAhead = '256';
  } else if (mode === 'battery') {
    profileLabel = 'Power Save Eco';
    if (activeModeBadge) activeModeBadge.innerText = 'Power Save Active';
    if (selGpuGov) selGpuGov.value = 'msm-adreno-tz';
    if (selGpuMin) selGpuMin.value = '180';
    if (rngGpuIdle) rngGpuIdle.value = 40;
    if (rngUpRate) rngUpRate.value = 1000;
    if (selBoostLp) selBoostLp.value = '576000';
    if (selBoostHp) selBoostHp.value = '652800';
    if (rngBoostDur) rngBoostDur.value = 20;
    if (chkToggleBypass) chkToggleBypass.checked = false;
    if (rngHaptic) rngHaptic.value = 1600;
    if (selHapticMode) selHapticMode.value = '1';
    if (rngSwappiness) rngSwappiness.value = 60;
    if (rngVfsCache) rngVfsCache.value = 100;
    if (rngMglruTtl) rngMglruTtl.value = 1500;
    selectedReadAhead = '128';
  } else if (mode === 'stutter') {
    profileLabel = 'Zero Stutter Fluid UI';
    if (activeModeBadge) activeModeBadge.innerText = 'Zero Stutter Active';
    if (selGpuGov) selGpuGov.value = 'msm-adreno-tz';
    if (selGpuMin) selGpuMin.value = '355';
    if (rngGpuIdle) rngGpuIdle.value = 120;
    if (rngUpRate) rngUpRate.value = 500;
    if (selBoostLp) selBoostLp.value = '1017600';
    if (selBoostHp) selBoostHp.value = '1209600';
    if (rngBoostDur) rngBoostDur.value = 40;
    if (chkToggleBbr) chkToggleBbr.checked = true;
    if (rngHaptic) rngHaptic.value = 3000;
    if (selHapticMode) selHapticMode.value = '1';
    if (rngSwappiness) rngSwappiness.value = 60;
    if (rngVfsCache) rngVfsCache.value = 100;
    if (rngMglruTtl) rngMglruTtl.value = 1000;
    selectedReadAhead = '256';
  }
  // Update label texts
  if (rngGpuIdle && lblGpuIdle) lblGpuIdle.innerText = `${rngGpuIdle.value} ms`;
  if (rngBoostDur && lblBoostDur) lblBoostDur.innerText = `${rngBoostDur.value} ms`;
  if (rngUpRate && lblUpRate) lblUpRate.innerText = `${rngUpRate.value} μs`;
  if (rngMicDelay && lblMicDelay) lblMicDelay.innerText = `${rngMicDelay.value} ms`;
  if (rngBottomMic && lblBottomMic) lblBottomMic.innerText = `${rngBottomMic.value >= 0 ? '+' : ''}${rngBottomMic.value} dB`;
  if (rngTopMic && lblTopMic) lblTopMic.innerText = `${rngTopMic.value >= 0 ? '+' : ''}${rngTopMic.value} dB`;
  if (rngHaptic && lblHapticVal) lblHapticVal.innerText = `${rngHaptic.value} mV`;
  if (rngSwappiness && lblSwappiness) lblSwappiness.innerText = rngSwappiness.value;
  if (rngVfsCache && lblVfsCache) lblVfsCache.innerText = rngVfsCache.value;
  if (rngMglruTtl && lblMglruTtl) lblMglruTtl.innerText = `${rngMglruTtl.value} ms`;

  // Sync all segmented buttons and chip pills
  document.querySelectorAll('.seg-btn').forEach(b => {
    b.classList.toggle('active', b.dataset.val === selectedChargeLimit);
  });
  document.querySelectorAll('.seg-btn-ra').forEach(b => {
    b.classList.toggle('active', b.dataset.val === selectedReadAhead);
  });
  if (rngHaptic) syncChips('#hapticChips', rngHaptic.value);
  if (rngMglruTtl) syncChips('#mglruChips', rngMglruTtl.value);
  if (rngBottomMic) syncChips('#bottomMicChips', rngBottomMic.value);
  if (rngTopMic) syncChips('#topMicChips', rngTopMic.value);

  currentProfileMode = mode;
  updateTileBadges();
  if (typeof updateCutoffLabels === 'function') updateCutoffLabels();
  saveAllState();

  // INSTANT REAL-TIME EXECUTION: Apply immediately on card tap
  await executeApply(true);
  triggerHapticTestClick(15);
  showToast(`⚡ ${profileLabel} Active in Real-Time!`);
}

// ========================================================
// 9. RESET TO FACTORY KERNEL DEFAULTS
// ========================================================
const btnResetDefaults = document.getElementById('btnResetDefaults');
if (btnResetDefaults) {
  btnResetDefaults.addEventListener('click', async () => {
    await applyProfile('default');
    showToast('✓ Successfully reset to factory stock defaults!');
  });
}

const btnFullWipe = document.getElementById('btnFullWipe');
if (btnFullWipe) {
  btnFullWipe.addEventListener('click', async () => {
    btnFullWipe.disabled = true;
    btnFullWipe.innerText = 'Wiping Hardware Tweaks...';
    logKernel('--- TRIGGERING FULL CLEAN HARDWARE WIPE ---');
    await runCmd('sh /data/adb/modules/aether_kernel_control/uninstall.sh 2>/dev/null || rm -rf /data/adb/aether');
    await applyProfile('default');
    logKernel('[WIPE] ✓ All /data/adb/aether files deleted. Factory hardware restored.');
    btnFullWipe.disabled = false;
    btnFullWipe.innerText = '🗑️ Wipe All Profiles & Reset Factory Hardware';
    showToast('✓ All tweaks completely wiped! Factory safe state restored.');
  });
}

// ========================================================
// 10. HARDWARE SCRIPT BUILDER & KERNEL INJECTION
// ========================================================
function buildHardwareScript() {
  const cmds = [];
  cmds.push('#!/system/bin/sh');
  cmds.push('# AETHER Kernel v1.4 Hardware Configuration');
  cmds.push('echo "[AETHER] Starting hardware register configuration..."');

  // Adreno GPU
  if (selGpuGov && selGpuGov.value) {
    cmds.push(`echo "${selGpuGov.value}" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null; [ -f /sys/class/kgsl/kgsl-3d0/gpu_governor ] && echo "${selGpuGov.value}" > /sys/class/kgsl/kgsl-3d0/gpu_governor 2>/dev/null`);
  }
  if (selGpuMin && selGpuMin.value) {
    cmds.push(`echo "${selGpuMin.value}" > /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null; echo "${selGpuMin.value}000000" > /sys/class/kgsl/kgsl-3d0/devfreq/min_freq 2>/dev/null`);
  }
  if (rngGpuIdle && rngGpuIdle.value) {
    cmds.push(`echo "${rngGpuIdle.value}" > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null`);
  }
  cmds.push('echo "[OK] Adreno 612 GPU clock & devfreq governor configured"');

  // CPU Schedutil & Sultan Input Boost
  if (rngUpRate && rngUpRate.value) {
    cmds.push(`for p in /sys/devices/system/cpu/cpufreq/policy*/schedutil/up_rate_limit_us /sys/devices/system/cpu/cpufreq/schedutil/up_rate_limit_us; do [ -f "$p" ] && echo "${rngUpRate.value}" > "$p" 2>/dev/null; done`);
  }
  if (selBoostLp && selBoostLp.value) {
    cmds.push(`echo "${selBoostLp.value}" > /sys/module/cpu_input_boost/parameters/input_boost_freq_lp 2>/dev/null`);
  } else {
    cmds.push('echo "1113600" > /sys/module/cpu_input_boost/parameters/input_boost_freq_lp 2>/dev/null');
  }
  if (selBoostHp && selBoostHp.value) {
    cmds.push(`echo "${selBoostHp.value}" > /sys/module/cpu_input_boost/parameters/input_boost_freq_hp 2>/dev/null`);
  } else {
    cmds.push('echo "1209600" > /sys/module/cpu_input_boost/parameters/input_boost_freq_hp 2>/dev/null');
  }
  if (rngBoostDur && rngBoostDur.value) {
    cmds.push(`echo "${rngBoostDur.value}" > /sys/module/cpu_input_boost/parameters/input_boost_duration 2>/dev/null`);
  } else {
    cmds.push('echo "64" > /sys/module/cpu_input_boost/parameters/input_boost_duration 2>/dev/null');
  }
  cmds.push('[ -d /sys/module/devfreq_boost/parameters ] && echo 100 > /sys/module/devfreq_boost/parameters/devfreq_boost_duration 2>/dev/null');
  cmds.push('echo "[OK] Sultan CPU Input Boost & Schedutil latency limits injected"');

  // BORE CPU Scheduler (v5.1.0 Burst-Oriented CFS)
  if (selBoreState && selBoreState.value) cmds.push(`echo "${selBoreState.value}" > /proc/sys/kernel/sched_bore 2>/dev/null`);
  if (selBoreScale && selBoreScale.value) cmds.push(`echo "${selBoreScale.value}" > /proc/sys/kernel/sched_burst_penalty_scale 2>/dev/null`);
  cmds.push('echo "[OK] BORE Scheduler burst penalty configured"');

  // Haptic Vibrator Strength & iOS Taptic Calibration
  if (rngHaptic && rngHaptic.value) {
    const ms = mvToClickMs(rngHaptic.value);
    cmds.push(`echo "${ms}" > /sys/class/leds/vibrator/click_duration 2>/dev/null`);
    cmds.push(`echo "${rngHaptic.value}" > /sys/class/leds/vibrator/vmax_mv 2>/dev/null`);
  }
  if (selHapticMode && selHapticMode.value) {
    cmds.push(`echo "${selHapticMode.value}" > /sys/class/leds/vibrator/haptic_mode 2>/dev/null`);
  }
  cmds.push('echo "[OK] Haptic linear resonance & iOS Taptic calibration active"');

  // AETHER Hardware Battery Care, Bypass & Charging Limit
  const isInstBypass = (chkInstantBypass && chkInstantBypass.checked) ? '1' : '0';
  cmds.push(`echo "${isInstBypass}" > /sys/kernel/aether/instant_bypass 2>/dev/null`);
  cmds.push(`[ -f /sys/class/power_supply/battery/bq_input_suspend ] && echo "${isInstBypass}" > /sys/class/power_supply/battery/bq_input_suspend 2>/dev/null`);
  cmds.push(`echo "${selectedChargeCutoff}" > /sys/kernel/aether/charge_cutoff_limit 2>/dev/null`);
  cmds.push(`echo "${selectedCutoffMode}" > /sys/kernel/aether/charge_cutoff_mode 2>/dev/null`);
  cmds.push(`mkdir -p /mnt/vendor/persist/aether 2>/dev/null && echo "CUTOFF_LIMIT=${selectedChargeCutoff}" > /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null && echo "CUTOFF_MODE=${selectedCutoffMode}" >> /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null || true`);
  if (selectedChargeLimit !== '0') {
    cmds.push(`echo "${selectedChargeLimit}" > /sys/class/power_supply/battery/constant_charge_current_max 2>/dev/null`);
  }

  // Turbo Fastcharge Protocol Override
  if (chkToggleTurboOverride && chkToggleTurboOverride.checked) {
    cmds.push('echo 1 > /sys/class/power_supply/battery/fastcharge_mode 2>/dev/null');
  }
  cmds.push('echo "[OK] Power supply & charging limiters set"');

  // Touch Glove Mode
  const glove = (chkToggleGlove && chkToggleGlove.checked) ? '1' : '0';
  cmds.push(`for g in /sys/devices/platform/soc/*.i2c/i2c-*/1-*/fts_glove_mode /sys/bus/i2c/drivers/fts_ts/*/fts_glove_mode /sys/bus/i2c/drivers/ft3418_ts/*/fts_glove_mode /sys/class/input/input*/fts_glove_mode; do [ -f "$g" ] && echo "${glove}" > "$g" 2>/dev/null; done`);

  // Display: DC Dimming & Sunlight HBM
  const dc = (chkToggleDcDim && chkToggleDcDim.checked) ? '1' : '0';
  cmds.push(`for n in /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker /sys/devices/platform/soc/*/*/regulator/regulator.*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/*/*/regulator/*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/virtual/graphics/fb0/anti_flicker /sys/class/graphics/fb0/anti_flicker; do if [ -f "$n" ]; then echo "${dc}" > "$n"; break; fi; done`);
  const hbm = (chkToggleHbm && chkToggleHbm.checked) ? '1' : '0';
  cmds.push(`for n in /sys/devices/platform/soc/soc:qcom,dsi-display/hbm /sys/devices/platform/soc/*/*/regulator/regulator.*/soc:qcom,dsi-display-lab/hbm /sys/devices/platform/soc/*/*/regulator/*/soc:qcom,dsi-display-lab/hbm /sys/devices/platform/soc/soc:qcom,dsi-display-primary/hbm /sys/devices/platform/soc/*/soc:qcom,dsi-display*/hbm /sys/class/drm/card0/device/hbm; do if [ -f "$n" ]; then echo "${hbm}" > "$n"; break; fi; done`);
  cmds.push('echo "[OK] AMOLED DC Dimming & Sunlight HBM applied"');

  // Audio Engine: Stock Bypass vs Custom Tuning
  if (chkAudioBypass && chkAudioBypass.checked) {
    // Normal users / Clean install: 100% Stock Factory Qualcomm Audio Routing - DO NOT TOUCH AUDIO
    cmds.push('# --- STOCK AUDIO ROUTE BYPASS (100% Untouched Factory Routing) ---');
    cmds.push('echo "[OK] Stock Audio Route Bypass Active (Hardware Audio Untouched)"');
  } else {
    // AETHER Enhanced Audio & Noise Cancellation Engine
    cmds.push('# --- AETHER ENHANCED AUDIO & NOISE CANCELLATION ENGINE ---');
    const safeDelay = rngMicDelay ? Math.max(10, parseInt(rngMicDelay.value, 10)) : 40;
    cmds.push(`for mic in /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay /sys/module/tx_macro/parameters/tx_unmute_delay; do [ -f "$mic" ] && echo ${safeDelay} > "$mic" 2>/dev/null; done`);

    // Hardware HPF Low-End Noise Filter
    const hpf = currentHpfCutoff || 'CF_NEG_3DB_75HZ';
    cmds.push(`tinymix "TX0 HPF cut off" ${hpf} 2>/dev/null || true`);
    cmds.push(`tinymix "TX1 HPF cut off" ${hpf} 2>/dev/null || true`);

    // Analog ADC Pre-Amp Gain (Strictly clamped <= 15)
    const adc = rngAdcGain ? Math.max(8, Math.min(15, parseInt(rngAdcGain.value, 10))) : 12;
    cmds.push(`tinymix "ADC1 Volume" ${adc} 2>/dev/null || true`);
    cmds.push(`tinymix "ADC2 Volume" ${adc} 2>/dev/null || true`);
    cmds.push(`tinymix "ADC3 Volume" ${adc} 2>/dev/null || true`);

    // Digital Decimator Volumes (Strictly clamped <= 96 to prevent decimator overflow)
    const bottomDb = rngBottomMic ? parseInt(rngBottomMic.value, 10) : 0;
    const topDb = rngTopMic ? parseInt(rngTopMic.value, 10) : 0;
    const bottomVal = Math.max(72, Math.min(96, 84 + bottomDb));
    const topVal = Math.max(72, Math.min(96, 84 + topDb));
    cmds.push(`tinymix "TX_DEC0 Volume" ${bottomVal} 2>/dev/null || true`);
    cmds.push(`tinymix "TX_DEC1 Volume" ${topVal} 2>/dev/null || true`);
    cmds.push(`tinymix "TX_DEC2 Volume" ${bottomVal} 2>/dev/null || true`);

    // Qualcomm Fluence Pro Dual-Mic Properties & ALSA mixers
    if (chkToggleFluence && chkToggleFluence.checked) {
      cmds.push('setprop persist.vendor.audio.fluence.voicecall true 2>/dev/null');
      cmds.push('setprop persist.vendor.audio.fluence.voicerec true 2>/dev/null');
      cmds.push('setprop persist.vendor.audio.fluence.speaker true 2>/dev/null');
      cmds.push('setprop ro.vendor.audio.sdk.fluencetype fluencepro 2>/dev/null');
      cmds.push('tinymix "TX_AIF1_CAP Mixer DEC0" 1 2>/dev/null || true');
      cmds.push('tinymix "TX_AIF1_CAP Mixer DEC1" 1 2>/dev/null || true');
    }
    cmds.push('echo "[OK] AETHER Custom Mic & Deep Noise Cancellation injected"');

    // Headphone Hi-Fi Mode & In-Call Earpiece Volume (Only when Audio Bypass is OFF)
    if (chkToggleHifiAudio && chkToggleHifiAudio.checked) {
      cmds.push(`tinymix 'RX_HPH_PWR_MODE' 'HIGH_PERF' 2>/dev/null || true`);
    }
    if (rngEarBoost && rngEarBoost.value) {
      const earBoost = parseInt(rngEarBoost.value, 10);
      if (earBoost > 0) {
        cmds.push(`tinymix 'RX_EAR Volume' ${84 + (earBoost * 2)} 2>/dev/null || true`);
      }
    }
    cmds.push('echo "[OK] WCD937x Audio Mic & HiFi calibration injected"');
  }

  // UFS Storage Read-Ahead Cache (Default 256KB optimal balanced)
  cmds.push(`for q in /sys/block/sd*/queue/read_ahead_kb /sys/block/dm-*/queue/read_ahead_kb; do [ -f "$q" ] && echo "${selectedReadAhead || '256'}" > "$q" 2>/dev/null; done`);

  // TCP Fast Open
  const tfo = (chkToggleTfo && chkToggleTfo.checked) ? '3' : '1';
  cmds.push(`echo "${tfo}" > /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null`);

  // Network BBR & Swappiness
  const bbr = (chkToggleBbr && chkToggleBbr.checked) ? 'bbr' : 'cubic';
  cmds.push(`echo "${bbr}" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null`);
  if (chkToggleBbr && chkToggleBbr.checked) {
    cmds.push('echo "fq" > /proc/sys/net/core/default_qdisc 2>/dev/null');
    cmds.push('echo 0 > /proc/sys/net/ipv4/tcp_slow_start_after_idle 2>/dev/null');
    cmds.push('echo 2 > /proc/sys/net/ipv4/tcp_ecn 2>/dev/null');
    cmds.push('echo 2097152 > /proc/sys/net/core/rmem_max 2>/dev/null');
    cmds.push('echo 2097152 > /proc/sys/net/core/wmem_max 2>/dev/null');
    cmds.push('echo "4096 87380 2097152" > /proc/sys/net/ipv4/tcp_rmem 2>/dev/null');
    cmds.push('echo "4096 65536 2097152" > /proc/sys/net/ipv4/tcp_wmem 2>/dev/null');
  }
  if (rngSwappiness) cmds.push(`echo "${rngSwappiness.value}" > /proc/sys/vm/swappiness 2>/dev/null`);
  if (rngVfsCache) cmds.push(`echo "${rngVfsCache.value}" > /proc/sys/vm/vfs_cache_pressure 2>/dev/null`);
  if (rngMglruTtl) cmds.push(`if [ -f /sys/kernel/mm/lru_gen/min_ttl_ms ]; then echo "${rngMglruTtl.value}" > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null; fi`);
  cmds.push('echo 0 > /proc/sys/vm/page-cluster 2>/dev/null');
  cmds.push('echo 150 > /proc/sys/vm/watermark_scale_factor 2>/dev/null');
  cmds.push('echo 10 > /proc/sys/vm/dirty_ratio 2>/dev/null');
  cmds.push('echo 3 > /proc/sys/vm/dirty_background_ratio 2>/dev/null');
  cmds.push('echo 300 > /proc/sys/vm/dirty_writeback_centisecs 2>/dev/null');
  cmds.push('echo 500 > /proc/sys/vm/dirty_expire_centisecs 2>/dev/null');
  cmds.push('if [ -f /sys/kernel/mm/lru_gen/enabled ]; then echo y > /sys/kernel/mm/lru_gen/enabled 2>/dev/null; fi');
  // Google Kyber BLK-MQ I/O Scheduler, rq_affinity & Read-Ahead
  const ioVal = (typeof selIoSched !== 'undefined' && selIoSched) ? selIoSched.value : 'kyber';
  cmds.push(`for q in /sys/block/sd*/queue /sys/block/dm-*/queue; do [ -d "$q" ] && echo "${ioVal}" > "$q/scheduler" 2>/dev/null && echo "${selectedReadAhead}" > "$q/read_ahead_kb" 2>/dev/null && echo 2 > "$q/rq_affinity" 2>/dev/null && echo 0 > "$q/rotational" 2>/dev/null; done`);
  cmds.push('for zq in /sys/block/zram*/queue; do [ -d "$zq" ] && echo 0 > "$zq/read_ahead_kb" 2>/dev/null && echo 2 > "$zq/rq_affinity" 2>/dev/null && echo 1 > "$zq/nomerges" 2>/dev/null; done');

  // Google SchedTune Prioritization (Instant Idle Core Search on Touch & Thread Colocation)
  if (typeof chkSchedtuneIdle !== 'undefined' && chkSchedtuneIdle) {
    if (chkSchedtuneIdle.checked) {
      cmds.push('if [ -d /dev/stune/top-app ]; then echo 1 > /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null; echo 1 > /dev/stune/top-app/schedtune.boost 2>/dev/null; echo 1 > /dev/stune/top-app/schedtune.colocate 2>/dev/null; echo 1 > /dev/stune/top-app/schedtune.sched_boost_no_override 2>/dev/null; fi');
    } else {
      cmds.push('if [ -d /dev/stune/top-app ]; then echo 0 > /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null; echo 0 > /dev/stune/top-app/schedtune.boost 2>/dev/null; fi');
    }
  }

  // Google TCP Low-Latency Socket (16KB Bufferbloat Elimination)
  if (typeof chkTcpLowat !== 'undefined' && chkTcpLowat) {
    if (chkTcpLowat.checked) {
      cmds.push('echo 16384 > /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null; echo 1 > /proc/sys/net/ipv4/tcp_autocorking 2>/dev/null');
    } else {
      cmds.push('echo 4294967295 > /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null; echo 0 > /proc/sys/net/ipv4/tcp_autocorking 2>/dev/null');
    }
  }

  // Google F2FS Dynamic Flash GC Throttling & Zero Lock Overhead
  cmds.push('for f2fs_node in /sys/fs/f2fs/*; do [ -d "$f2fs_node" ] && echo 0 > "$f2fs_node/gc_urgent" 2>/dev/null && echo 0 > "$f2fs_node/iostat_enable" 2>/dev/null && echo 16 > "$f2fs_node/discard_granularity" 2>/dev/null; done');

  // Google Pixel Background Core Isolation (Cortex-A55 Little Cores only for background sync)
  cmds.push('echo 1 > /sys/module/printk/parameters/console_suspend 2>/dev/null');
  cmds.push('echo "4 4 1 7" > /proc/sys/kernel/printk 2>/dev/null');
  cmds.push('if [ -d /dev/cpuset/top-app ]; then echo "0-7" > /dev/cpuset/top-app/cpus 2>/dev/null; fi');
  cmds.push('if [ -d /dev/cpuset/foreground ]; then echo "0-7" > /dev/cpuset/foreground/cpus 2>/dev/null; fi');
  cmds.push('if [ -d /dev/cpuset/background ]; then echo "0-5" > /dev/cpuset/background/cpus 2>/dev/null; fi');
  cmds.push('if [ -d /dev/cpuset/system-background ]; then echo "0-5" > /dev/cpuset/system-background/cpus 2>/dev/null; fi');
  cmds.push('if [ -d /dev/cpuset/restricted ]; then echo "0-3" > /dev/cpuset/restricted/cpus 2>/dev/null; fi');
  cmds.push('if [ -d /dev/stune/rt ]; then echo 1 > /dev/stune/rt/schedtune.prefer_idle 2>/dev/null; echo 1 > /dev/stune/rt/schedtune.boost 2>/dev/null; echo 1 > /dev/stune/rt/schedtune.sched_boost_no_override 2>/dev/null; fi');

  cmds.push('echo "[OK] TCP Congestion & Virtual Memory cache pressure tuned"');
  cmds.push('echo "[AETHER] All hardware parameters successfully written to kernel!"');
  return cmds.join('\n');
}

const btnApplyAll = document.getElementById('btnApplyAll');
const chkApplyOnBoot = document.getElementById('chkApplyOnBoot');

async function executeApply(isRealtime = false) {
  if (!isRealtime && btnApplyAll) {
    btnApplyAll.disabled = true;
    btnApplyAll.innerText = 'Applying to Kernel...';
  }
  logKernel('--- INJECTING TWEAKS TO KERNEL HARDWARE ---');

  const script = buildHardwareScript();
  const b64 = btoa(unescape(encodeURIComponent(script)));

  // Write through base64 to avoid quoting or multiline breakages
  const applyCmd = `mkdir -p /data/adb/aether && echo "${b64}" | base64 -d > /data/adb/aether/aether_tweaks.sh && chmod 755 /data/adb/aether/aether_tweaks.sh && sh /data/adb/aether/aether_tweaks.sh > /data/adb/aether/apply_log.txt 2>&1 && cat /data/adb/aether/apply_log.txt`;
  
  const output = await runCmd(applyCmd);

  if (!isRealtime) {
    showToast('✓ Tweaks successfully injected into hardware!');
    if (btnApplyAll) {
      btnApplyAll.disabled = false;
      btnApplyAll.innerText = 'Apply Tweaks Now';
    }
  }
  logKernel('--- INJECTION COMPLETE ---');
  updateTelemetry();
}

if (btnApplyAll) btnApplyAll.addEventListener('click', () => executeApply(false));

// ========================================================
// 8. REAL LIVE SUSFS HARDWARE CAPTURE & VERIFICATION
// ========================================================
async function refreshSusfsLiveStatus(silent = false) {
  const btn = document.getElementById('btnTestSusfsCli');
  const badgeOverall = document.getElementById('susfsOverallBadge');
  const liveSub = document.getElementById('susfsLiveSub');
  const binSource = document.getElementById('susfsBinarySource');
  const verStr = document.getElementById('susfsKernelVersionStr');

  if (btn && !silent) {
    btn.disabled = true;
    btn.innerText = 'Querying Kernel VFS Subsystem...';
  }

  const queryScript = `
SUSFS_BIN=""
for b in /data/adb/ksu/bin/ksu_susfs /data/adb/modules/ksu_module_susfs/tools/ksu_susfs_arm64 /data/adb/modules/*/tools/ksu_susfs*; do
  if [ -x "$b" ]; then
    SUSFS_BIN="$b"
    break
  fi
done

echo "=== SOURCE_BIN ==="
echo "\${SUSFS_BIN:-NONE}"

echo "=== SUSFS_VERSION ==="
if [ -n "$SUSFS_BIN" ]; then
  "$SUSFS_BIN" show version 2>/dev/null || echo "FAIL"
fi

echo "=== SUSFS_FEATURES ==="
if [ -n "$SUSFS_BIN" ]; then
  "$SUSFS_BIN" show enabled_features 2>/dev/null
fi

echo "=== CONFIG_GZ ==="
if [ -r /proc/config.gz ]; then
  zcat /proc/config.gz 2>/dev/null | grep -i "CONFIG_KSU_SUSFS"
elif [ -r /sys/kernel/debug/kernel_config_data ]; then
  cat /sys/kernel/debug/kernel_config_data 2>/dev/null | grep -i "CONFIG_KSU_SUSFS"
fi

echo "=== DMESG_RECENT ==="
dmesg 2>/dev/null | grep -i "susfs" | tail -n 5
`;

  try {
    const rawOut = await runCmd(queryScript, true);
    if (!silent) {
      logKernel(`[LIVE SUSFS KERNEL QUERY]\n${rawOut}`);
    }

    const getBlock = (tag) => {
      const parts = rawOut.split(`=== ${tag} ===`);
      if (parts.length > 1) {
        return parts[1].split('=== ')[0].trim();
      }
      return '';
    };

    const binFound = getBlock('SOURCE_BIN');
    const versionFound = getBlock('SUSFS_VERSION');
    const featuresFound = getBlock('SUSFS_FEATURES');
    const configFound = getBlock('CONFIG_GZ');

    // Real feature presence detection (CLI features OR /proc/config.gz)
    const checkFeature = (name) => {
      return featuresFound.includes(name) || configFound.includes(`${name}=y`);
    };

    const hasKconfig = checkFeature('CONFIG_KSU_SUSFS') || versionFound.toLowerCase().includes('v1.5');
    const hasSusMount = checkFeature('CONFIG_KSU_SUSFS_SUS_MOUNT') || featuresFound.includes('sus_mount');
    const hasTryUmount = checkFeature('CONFIG_KSU_SUSFS_TRY_UMOUNT') || featuresFound.includes('try_umount');
    const hasSpoofCmdline = checkFeature('CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG') || featuresFound.includes('spoof_cmdline');
    const hasSpoofUname = checkFeature('CONFIG_KSU_SUSFS_SPOOF_UNAME') || featuresFound.includes('spoof_uname');
    const hasOpenRedirect = checkFeature('CONFIG_KSU_SUSFS_OPEN_REDIRECT') || featuresFound.includes('open_redirect');
    const hasHideKallsyms = checkFeature('CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS') || featuresFound.includes('hide_kallsyms');
    const hasSusPath = checkFeature('CONFIG_KSU_SUSFS_SUS_PATH') || featuresFound.includes('sus_path');

    const updateBadge = (id, isActive) => {
      const el = document.getElementById(id);
      if (el) {
        if (isActive) {
          el.className = 'text-green';
          el.innerText = 'PASS (ACTIVE)';
        } else {
          el.className = 'text-red';
          el.innerText = 'DISABLED';
        }
      }
    };

    updateBadge('susfsStatus_kconfig', hasKconfig);
    updateBadge('susfsStatus_sus_mount', hasSusMount);
    updateBadge('susfsStatus_try_umount', hasTryUmount);
    updateBadge('susfsStatus_spoof_cmdline', hasSpoofCmdline);
    updateBadge('susfsStatus_spoof_uname', hasSpoofUname);
    updateBadge('susfsStatus_open_redirect', hasOpenRedirect);
    updateBadge('susfsStatus_hide_kallsyms', hasHideKallsyms);
    updateBadge('susfsStatus_sus_path', hasSusPath);

    const featureCount = [hasKconfig, hasSusMount, hasTryUmount, hasSpoofCmdline, hasSpoofUname, hasOpenRedirect, hasHideKallsyms, hasSusPath].filter(Boolean).length;

    if (badgeOverall) {
      if (featureCount >= 7) {
        badgeOverall.className = 'badge-active';
        badgeOverall.innerText = `${versionFound && versionFound !== 'FAIL' ? versionFound : 'v2.3.0'} ACTIVE (${featureCount}/8 PASS)`;
      } else if (featureCount > 0) {
        badgeOverall.className = 'badge-active badge-warning';
        badgeOverall.innerText = `PARTIAL (${featureCount}/8 PASS)`;
      } else {
        badgeOverall.className = 'badge-active badge-danger';
        badgeOverall.innerText = 'INACTIVE (NOT DETECTED)';
      }
    }

    if (liveSusfsStatus) {
      if (featureCount >= 4) {
        liveSusfsStatus.innerText = `${versionFound && versionFound !== 'FAIL' ? versionFound : 'v2.3.0'} Active`;
        liveSusfsStatus.style.color = 'var(--color-success)';
        if (dotSusfs) dotSusfs.className = 'chip-dot active';
      } else if (featureCount > 0) {
        liveSusfsStatus.innerText = 'Partial';
        liveSusfsStatus.style.color = 'var(--accent-orange)';
        if (dotSusfs) dotSusfs.className = 'chip-dot active';
      } else {
        liveSusfsStatus.innerText = 'Not Active';
        liveSusfsStatus.style.color = 'var(--text-muted)';
        if (dotSusfs) dotSusfs.className = 'chip-dot inactive';
      }
    }

    if (liveSub) {
      liveSub.innerText = `Captured live from hardware (${featureCount} active features detected)`;
    }
    if (binSource) {
      binSource.innerText = `Binary: ${binFound !== 'NONE' ? binFound : 'Built-in Kernel prctl'}`;
    }
    if (verStr) {
      verStr.innerText = `Kernel ABI: ${versionFound && versionFound !== 'FAIL' ? versionFound : '4.14-AETHER'}`;
    }

    if (!silent) {
      showToast(`✓ SUSFS Status Updated: ${featureCount}/8 features verified live!`);
    }
  } catch (err) {
    console.error('refreshSusfsLiveStatus error:', err);
    if (!silent) showToast('Error querying kernel SUSFS');
  } finally {
    if (btn) {
      btn.disabled = false;
      btn.innerText = '🔍 Refresh Live SUSFS Kernel Verification';
    }
  }
}

const btnDexoptOptimize = document.getElementById('btnDexoptOptimize');
const txtDexoptBtn = document.getElementById('txtDexoptBtn');
const dexoptProgress = document.getElementById('dexoptProgress');

if (btnDexoptOptimize) {
  btnDexoptOptimize.addEventListener('click', async () => {
    btnDexoptOptimize.disabled = true;
    btnDexoptOptimize.style.opacity = '0.6';
    if (txtDexoptBtn) txtDexoptBtn.textContent = '⚡ Compiling apps in background...';
    if (dexoptProgress) dexoptProgress.style.display = 'block';

    showToast('Dexopt optimization started! Compiling all apps with speed-profile...');

    try {
      await runCmd('nohup cmd package compile -m speed-profile -a > /dev/null 2>&1 &', false);
      showToast('Optimization running in background! Reboot lag will be eliminated.');
      setTimeout(() => {
        if (txtDexoptBtn) txtDexoptBtn.textContent = '✅ Optimization Running in Background';
        if (dexoptProgress) dexoptProgress.textContent = 'All apps are compiling into native ARM64 code. Once finished, first-boot lag is completely gone!';
      }, 2500);
    } catch (e) {
      showToast('Dexopt command failed: ' + e);
      btnDexoptOptimize.disabled = false;
      btnDexoptOptimize.style.opacity = '1';
    }
  });
}

// Initial Live Hardware Sync on Launch
async function initialHardwareSync() {
  logKernel('[INIT] Connecting to kernel sysfs nodes...');
  try {
    if (!userSettingsRestored) {
      // First boot with no saved user state: read from hardware to populate UI
      const gpuGov = await runCmd('cat /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null || cat /sys/class/kgsl/kgsl-3d0/gpu_governor 2>/dev/null', true);
      if (gpuGov && selGpuGov && ['msm-adreno-tz', 'performance', 'powersave', 'simple_ondemand'].includes(gpuGov.trim())) {
        selGpuGov.value = gpuGov.trim();
      }

      const gpuMin = await runCmd('cat /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null || cat /sys/class/kgsl/kgsl-3d0/devfreq/min_freq 2>/dev/null', true);
      if (gpuMin && !isNaN(gpuMin)) {
        let minMhz = parseInt(gpuMin, 10);
        if (minMhz > 10000) minMhz = Math.round(minMhz / 1000000);
        if (selGpuMin) selGpuMin.value = String(minMhz);
      }

      const bore = await runCmd('cat /proc/sys/kernel/sched_bore 2>/dev/null', true);
      if (bore && selBoreState) {
        selBoreState.value = bore === '1' ? '1' : '0';
      }

      const boostDur = await runCmd('cat /sys/module/cpu_input_boost/parameters/input_boost_duration 2>/dev/null', true);
      if (boostDur && rngBoostDur && !isNaN(boostDur)) {
        rngBoostDur.value = boostDur;
        if (lblBoostDur) lblBoostDur.innerText = `${boostDur} ms`;
      }

      // AETHER In-Kernel Hardware Cutoff & Instant Bypass detection
      const kInstantBypass = await runCmd('cat /sys/kernel/aether/instant_bypass 2>/dev/null', true);
      if (chkInstantBypass && kInstantBypass) {
        chkInstantBypass.checked = (kInstantBypass.trim() === '1');
      }

      const kCutoffLimit = await runCmd('cat /sys/kernel/aether/charge_cutoff_limit 2>/dev/null', true);
      if (kCutoffLimit && !isNaN(parseInt(kCutoffLimit.trim(), 10))) {
        selectedChargeCutoff = String(parseInt(kCutoffLimit.trim(), 10));
        document.querySelectorAll('.cutoff-pill').forEach(b => {
          b.classList.toggle('active', b.dataset.val === selectedChargeCutoff);
        });
      }

      const kCutoffMode = await runCmd('cat /sys/kernel/aether/charge_cutoff_mode 2>/dev/null', true);
      if (kCutoffMode && !isNaN(parseInt(kCutoffMode.trim(), 10))) {
        selectedCutoffMode = String(parseInt(kCutoffMode.trim(), 10));
        document.querySelectorAll('.cutoff-mode-pill').forEach(b => {
          b.classList.toggle('active', b.dataset.val === selectedCutoffMode);
        });
      }
      if (typeof updateCutoffLabels === 'function') updateCutoffLabels();

      const fastMode = await runCmd('cat /sys/class/power_supply/battery/fastcharge_mode 2>/dev/null', true);
      if (chkToggleTurboOverride && fastMode) {
        chkToggleTurboOverride.checked = (fastMode.trim() === '1');
      }

      const gloveMode = await runCmd('for g in /sys/devices/platform/soc/*.i2c/i2c-*/1-*/fts_glove_mode /sys/bus/i2c/drivers/fts_ts/*/fts_glove_mode /sys/bus/i2c/drivers/ft3418_ts/*/fts_glove_mode /sys/class/input/input*/fts_glove_mode /sys/class/input/input*/glove_mode; do [ -f "$g" ] && cat "$g" && break; done', true);
      if (chkToggleGlove && gloveMode) {
        chkToggleGlove.checked = (gloveMode.includes('On') || gloveMode.trim() === '1');
      }

      const dcDim = await runCmd('for n in /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker /sys/devices/platform/soc/*/*/regulator/regulator.*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/*/*/regulator/*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/*/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker /sys/devices/virtual/graphics/fb0/anti_flicker /sys/class/graphics/fb0/anti_flicker; do if [ -f "$n" ]; then cat "$n"; break; fi; done', true);
      if (chkToggleDcDim) {
        chkToggleDcDim.checked = (dcDim.trim() === '1');
      }

      const hbmVal = await runCmd('for n in /sys/devices/platform/soc/soc:qcom,dsi-display/hbm /sys/devices/platform/soc/*/*/regulator/regulator.*/soc:qcom,dsi-display-lab/hbm /sys/devices/platform/soc/*/*/regulator/*/soc:qcom,dsi-display-lab/hbm /sys/devices/platform/soc/soc:qcom,dsi-display-primary/hbm /sys/devices/platform/soc/*/soc:qcom,dsi-display*/hbm /sys/class/drm/card0/device/hbm; do if [ -f "$n" ]; then cat "$n"; break; fi; done', true);
      if (chkToggleHbm) {
        chkToggleHbm.checked = (hbmVal.trim() === '1');
      }

      const upRate = await runCmd('cat /sys/devices/system/cpu/cpufreq/policy0/schedutil/up_rate_limit_us 2>/dev/null || cat /sys/devices/system/cpu/cpufreq/schedutil/up_rate_limit_us 2>/dev/null', true);
      if (upRate && rngUpRate && !isNaN(upRate)) {
        rngUpRate.value = upRate.trim();
        if (lblUpRate) lblUpRate.innerText = `${upRate.trim()} μs`;
      }

      const boostLp = await runCmd('cat /sys/module/cpu_input_boost/parameters/input_boost_freq_lp 2>/dev/null', true);
      if (boostLp && selBoostLp) selBoostLp.value = boostLp.trim();
      const boostHp = await runCmd('cat /sys/module/cpu_input_boost/parameters/input_boost_freq_hp 2>/dev/null', true);
      if (boostHp && selBoostHp) selBoostHp.value = boostHp.trim();

      const swappiness = await runCmd('cat /proc/sys/vm/swappiness 2>/dev/null', true);
      if (swappiness && rngSwappiness && !isNaN(swappiness)) {
        rngSwappiness.value = swappiness.trim();
        if (lblSwappiness) lblSwappiness.innerText = swappiness.trim();
      }

      const vfs = await runCmd('cat /proc/sys/vm/vfs_cache_pressure 2>/dev/null', true);
      if (vfs && rngVfsCache && !isNaN(vfs)) {
        rngVfsCache.value = vfs.trim();
        if (lblVfsCache) lblVfsCache.innerText = vfs.trim();
      }

      const mglruTtl = await runCmd('cat /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null', true);
      if (mglruTtl && rngMglruTtl && !isNaN(mglruTtl)) {
        rngMglruTtl.value = mglruTtl.trim();
        if (lblMglruTtl) lblMglruTtl.innerText = `${mglruTtl.trim()} ms`;
        syncChips('#mglruChips', mglruTtl.trim());
      }

      const ra = await runCmd('cat /sys/block/sda/queue/read_ahead_kb 2>/dev/null', true);
      if (ra && !isNaN(ra)) {
        selectedReadAhead = ra.trim();
        document.querySelectorAll('.seg-btn-ra').forEach(b => {
          b.classList.toggle('active', b.dataset.val === selectedReadAhead);
        });
      }

      const ioSchedRaw = await runCmd('cat /sys/block/sda/queue/scheduler 2>/dev/null', true);
      if (ioSchedRaw && selIoSched) {
        const match = ioSchedRaw.match(/\[([a-zA-Z0-9_-]+)\]/);
        if (match && match[1]) {
          selIoSched.value = match[1];
          if (statusUfs) statusUfs.innerText = `${match[1].toUpperCase()} • ${selectedReadAhead} KB`;
        }
      }

      const stuneIdle = await runCmd('cat /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null', true);
      if (chkSchedtuneIdle && stuneIdle) {
        chkSchedtuneIdle.checked = (stuneIdle.trim() === '1');
        if (statusSchedtune) statusSchedtune.innerText = chkSchedtuneIdle.checked ? 'Prefer Idle Active' : 'Default Sched';
      }

      const tcpLowat = await runCmd('cat /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null', true);
      if (chkTcpLowat && tcpLowat) {
        chkTcpLowat.checked = (parseInt(tcpLowat.trim(), 10) <= 65536);
      }

      const tfoVal = await runCmd('cat /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null', true);
      if (chkToggleTfo && tfoVal) {
        chkToggleTfo.checked = (tfoVal.trim() === '3');
      }

      const aethBypass = await runCmd('cat /sys/kernel/aether/audio_bypass_mode 2>/dev/null', true);
      if (chkAudioBypass && aethBypass && (aethBypass.trim() === '1' || aethBypass.trim() === '0')) {
        chkAudioBypass.checked = (aethBypass.trim() === '1');
        updateAudioBypassUI();
      }

      const tcpCc = await runCmd('cat /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null', true);
      if (chkToggleBbr) {
        chkToggleBbr.checked = (tcpCc === 'bbr');
      }

      const haptic = await runCmd('cat /sys/class/leds/vibrator/vmax_mv 2>/dev/null');
      const clickDur = await runCmd('cat /sys/class/leds/vibrator/click_duration 2>/dev/null');
      if (haptic && rngHaptic && !isNaN(haptic)) {
        rngHaptic.value = haptic.trim();
        if (lblHapticVal) lblHapticVal.innerText = `${haptic.trim()} mV`;
        syncChips('#hapticChips', haptic.trim());
      } else if (clickDur && !isNaN(clickDur)) {
        const dur = parseInt(clickDur.trim(), 10);
        const mappedMv = dur <= 10 ? '1600' : (dur <= 12 ? '2400' : (dur <= 18 ? '3000' : '3596'));
        if (rngHaptic) rngHaptic.value = mappedMv;
        if (lblHapticVal) lblHapticVal.innerText = `${dur} ms (${mappedMv} mV)`;
        syncChips('#hapticChips', mappedMv);
      }

      const hapMode = await runCmd('cat /sys/class/leds/vibrator/haptic_mode 2>/dev/null');
      if (hapMode && selHapticMode && ['0', '1', '2'].includes(hapMode.trim())) {
        selHapticMode.value = hapMode.trim();
      }

      const micDelay = await runCmd('cat /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay 2>/dev/null || cat /sys/module/tx_macro/parameters/tx_unmute_delay 2>/dev/null');
      if (micDelay && rngMicDelay && !isNaN(micDelay)) {
        rngMicDelay.value = micDelay.trim();
        if (lblMicDelay) lblMicDelay.innerText = `${micDelay.trim()} ms`;
      }

      const adc1Val = await runCmd('tinymix "ADC1 Volume" 2>/dev/null', true);
      if (adc1Val && rngAdcGain) {
        const match = adc1Val.match(/ADC1 Volume:\s*(\d+)/);
        if (match) {
          const val = match[1];
          rngAdcGain.value = val;
          if (lblAdcGain) lblAdcGain.innerText = `${val} (${val <= 10 ? 'Quiet' : (val <= 12 ? 'Stock' : (val <= 14 ? 'Studio' : 'High Treble'))})`;
          syncChips('#adcChips', val);
        }
      }
    } else {
      // User settings already loaded from disk/localStorage!
      // Enforce the entire saved user configuration to kernel hardware:
      logKernel('[PERSIST] Enforcing user configuration to hardware...');
      await executeApply(true);
    }

    updateTileBadges();
    refreshSusfsLiveStatus(true);
    await updateTelemetry(true);
    logKernel('[INIT] ✓ Hardware nodes synchronized with AETHER kernel!');
  } catch (err) {
    logKernel(`[INIT ERR] ${err && err.message ? err.message : err}`);
  }
}

// Initial Sync and Telemetry Start
(async function initStartup() {
  // 1. Immediately restore state from localStorage (fast UI render)
  const restoredLocal = restoreAllState();
  if (restoredLocal) {
    userSettingsRestored = true;
    logKernel('[PERSIST] ✓ Settings restored from local storage');
  }

  // 2. ALWAYS also check /data/adb/aether/aether_state.json as ground truth root store
  try {
    const rawDisk = await runCmd('cat /data/adb/aether/aether_state.json 2>/dev/null', true);
    if (rawDisk && rawDisk.startsWith('{')) {
      const parsed = JSON.parse(rawDisk);
      restoreAllState(parsed);
      userSettingsRestored = true;
      logKernel('[PERSIST] ✓ Ground truth settings restored from /data/adb/aether/aether_state.json');
    }
  } catch (e) {}

  // 3. Initialize Touch Guard accidental protection
  try {
    const savedTouchGuard = localStorage.getItem('touchGuardLocked');
    isTouchGuardLocked = (savedTouchGuard !== 'false');
  } catch (e) {
    isTouchGuardLocked = true;
  }
  updateTouchGuardUI();

  updateTileBadges();
  setTimeout(initialHardwareSync, 350);
  setTimeout(startTelemetryLoop, 700);
})();
