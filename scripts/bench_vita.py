#!/usr/bin/env python3
"""Unattended benchmark runs of the port on a PS Vita through vita-agent-use.

For every scenario and repetition: write ux0:data/crashwoc/env.txt, start a
vita-agent-use run (the first one can install the VPK), wait until the game
prints "reached --frames" and exits, download its log and summarise it with
bench_summary.py (fps, frame-time breakdown, hitches, CPU per core, memory).

Needs `serve` running. Paths: VAU_DIR (vita-agent-use checkout), VAU_PYTHON
(Python with `cryptography`), VITA_AGENT_CONFIG_DIR.

Usage: scripts/bench_vita.py OUTDIR [--install build-vita/crashwoc.vpk]
       [--reps 3] [--only hub,snow_m] [--frames 3200]
"""
import argparse
import json
import os
import pathlib
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
VAU_DIR = pathlib.Path(os.environ.get("VAU_DIR", pathlib.Path.home() / "vita-dev/third_party/vita-agent-use"))
VAU_PY = os.environ.get("VAU_PYTHON", str(pathlib.Path.home() / ".local/share/vita-devtools/bin/python3"))
CFG = pathlib.Path(os.environ.setdefault("VITA_AGENT_CONFIG_DIR", str(pathlib.Path.home() / "vita-dev/vita-agent-config")))
EVENTS = CFG / "pc/server/events.jsonl"
LOG = "ux0:data/crashwoc/crashwoc.log"
ENV = "ux0:data/crashwoc/env.txt"

# name -> level for CWOC_WARP (None: stay in the hub, where CWOC_INTRO=0 starts)
SCENARIOS = {
    "hub": None,
    "snow_m": 7,
    "castle": 10,
    "jungle_r": 8,
    "earth_r": 4,
}
# with a fresh save the first level entered from the hub opens with a story
# scene in the vortex (cut.c UpdateGameCut) that waits for A/START
SKIP_INPUT = "A@420-425,A@520-525,A@620-625"


def vau(*args, timeout=600):
    p = subprocess.run([VAU_PY, "client/vita_agent.py", *args], cwd=VAU_DIR, capture_output=True, text=True,
                       timeout=timeout)
    lines = [l for l in p.stdout.splitlines() if l.strip().startswith("{")]
    if not lines:
        raise RuntimeError("vita-agent-use %s: no JSON output: %s" % (args[0], (p.stdout + p.stderr)[-400:]))
    return json.loads(lines[-1])


def env_text(level, frames):
    lines = ["# benchmark run (written by scripts/bench_vita.py); delete this file for normal play",
             "CWOC_SAVE=ux0:data/crashwoc/save_bench", "CWOC_SKIP_LOGOS=1", "CWOC_INTRO=0",
             "CWOC_KEEPAWAKE=1", "CWOC_LONGFRAME=100", "CWOC_FRAMES=%d" % frames]
    if level is not None:
        lines += ["CWOC_WARP=%d@200" % level, "CWOC_INPUT=" + SKIP_INPUT]
    return "\n".join(lines) + "\n"


def run_one(out, name, level, rep, frames, install):
    tag = "%s_%d" % (name, rep)
    envf = out / (tag + ".env.txt")
    envf.write_text(env_text(level, frames))
    r = vau("fs", "upload", str(envf), ENV, "--overwrite", "--yes")
    if r.get("verified") is not True and (r.get("result") or {}).get("verified") is not True:
        raise RuntimeError("env.txt upload not verified: %s" % json.dumps(r)[-300:])
    spec = {"title_id": "CRWC00001", "timeout_s": 600, "performance_interval_ms": 1000,
            "close_on_completion": False,
            "completion": {"log": {"path": LOG, "literal": "reached --frames"}}}
    if install:
        spec["prepare"] = {"install": {"source": str(pathlib.Path(install).resolve())}}
    specf = out / (tag + ".run.json")
    specf.write_text(json.dumps(spec, indent=1))
    r = vau("run", "start", str(specf), *(["--yes"] if install else []))
    run_id = r["result"]["run_id"]
    t0 = time.time()
    while True:
        time.sleep(5)
        st = vau("run", "status")["result"]
        if st.get("phase") in ("completed", "finished", "failed", "cancelled"):
            break
    dt = time.time() - t0
    logf = out / (tag + ".log")
    vau("fs", "download", LOG, str(logf))
    summ = subprocess.run([sys.executable, str(HERE / "bench_summary.py"), str(logf), "--events", str(EVENTS),
                           "--run", run_id, "--json", str(out / (tag + ".json"))], capture_output=True, text=True)
    res = json.loads(summ.stdout) if summ.stdout.strip().startswith("{") else {}
    res.update({"scenario": name, "rep": rep, "run_id": run_id, "phase": st.get("phase"),
                "error": st.get("error"), "wall_s": round(dt)})
    (out / (tag + ".json")).write_text(json.dumps(res, indent=1))
    fps = (res.get("fps") or {}).get("mean")
    ms = (res.get("ms_frame") or {}).get("mean")
    print("%-10s rep %d: %s (%s) fps %s ms/frame %s windows %s hitches %s  [%ds]" % (
        name, rep, st.get("phase"), st.get("error") or "ok", fps, ms, res.get("windows"),
        (res.get("hitches") or {}).get("n"), dt), flush=True)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--install")
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--only")
    ap.add_argument("--frames", type=int, default=3200)
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    names = a.only.split(",") if a.only else list(SCENARIOS)
    install = a.install
    for rep in range(1, a.reps + 1):
        for name in names:
            res = run_one(out, name, SCENARIOS[name], rep, a.frames, install)
            install = None
            if res.get("phase") != "completed":
                print("stopping: run did not complete (locked screen, crash or dialog?)", flush=True)
                return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
