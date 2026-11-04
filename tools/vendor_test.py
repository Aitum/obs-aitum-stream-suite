#!/usr/bin/env python3
"""Exercises the aitum-stream-suite obs-websocket vendor against a running OBS.

Usage:
    python tools/vendor_test.py [--port 4455] [--password PW]
    python tools/vendor_test.py --live --output NAME

The password can also come from the OBS_WS_PASSWORD environment variable, which
keeps it out of the process list. Needs the `websockets` package and an OBS with
obs-websocket enabled.

By default nothing in OBS changes: the run covers read requests, the logging
settings (restored afterwards), and the error path of every other request, using
names that do not exist. The start_all_* / stop_all_* requests are never sent,
because they act on every configured output at once.

--live starts and stops the one output named by --output, as a real stream to
wherever it points. Point it somewhere harmless. A local RTMP listener such as

    ffmpeg -f flv -listen 1 -i rtmp://127.0.0.1:1936/live -f null -

keeps everything on the machine. The run refuses to touch an output that is
already active, so it cannot end a stream that was running before it started.

Each step prints PASS / FAIL / SKIP with the actual response. Exit code is 0 when
nothing failed, 1 on a failed check, 2 when the test could not run at all.
"""
import argparse
import asyncio
import base64
import hashlib
import json
import os
import sys

import websockets

VENDOR = "aitum-stream-suite"
MISSING = "__no_such_name__"
REQUEST_TIMEOUT = 5.0
EVENT_TIMEOUT = 15.0

ALL_REQUESTS = {
    "version", "status", "get_canvas", "switch_scene", "current_scene", "get_scenes",
    "get_outputs", "start_output", "stop_output", "get_stats", "set_logging", "get_logging",
    "start_all_outputs", "stop_all_outputs", "start_all_streams", "stop_all_streams",
    "start_all_recordings", "stop_all_recordings", "save_backtrack", "add_chapter",
    "get_dock_modes", "switch_dock_mode", "get_docks", "dock_show", "dock_hide",
    "get_live_scenes", "live_scenes_add", "live_scenes_remove",
    "canvas_dock_show_panel", "canvas_dock_hide_panel",
    "get_transitions", "switch_transition", "transitions_add", "transitions_remove",
    "refresh_browser_panel", "reset_browser_panel",
}

# Requests that change state, so only their error path runs here.
ERROR_PATHS = [
    ("switch_scene", {}),
    ("current_scene", {}),
    ("current_scene", {"canvas": MISSING}),
    ("get_scenes", {"canvas": MISSING}),
    ("get_transitions", {}),
    ("get_transitions", {"canvas": MISSING}),
    ("switch_transition", {"canvas": MISSING, "transition": MISSING}),
    ("transitions_add", {"canvas": MISSING, "name": MISSING, "type": MISSING}),
    ("transitions_remove", {"canvas": MISSING, "name": MISSING}),
    ("save_backtrack", {}),
    ("save_backtrack", {"output": MISSING}),
    ("add_chapter", {}),
    ("add_chapter", {"output": MISSING, "chapter_name": "vendor_test"}),
    ("switch_dock_mode", {}),
    ("switch_dock_mode", {"mode": MISSING}),
    ("dock_show", {"dock": MISSING}),
    ("dock_hide", {"dock": MISSING}),
    # Only without a name: live_scenes_add accepts any name it is given, so a
    # made-up one would end up in the user's saved Live Scenes.
    ("live_scenes_add", {}),
    ("live_scenes_remove", {}),
    ("canvas_dock_show_panel", {"canvas": MISSING, "panel": MISSING}),
    ("canvas_dock_hide_panel", {"canvas": MISSING, "panel": MISSING}),
    ("refresh_browser_panel", {"panel": MISSING}),
    ("reset_browser_panel", {"panel": MISSING}),
]

STAT_FIELDS = {
    "name", "active", "reconnecting", "congestion", "connect_time_ms", "total_frames",
    "dropped_frames", "dropped_percent", "total_bytes", "bitrate_kbps",
}
LOGGING_FIELDS = ("log_events", "stats_events", "stats_interval_ms", "level")
LOGGING_DEFAULTS = {"log_events": True, "stats_events": True, "stats_interval_ms": 2000, "level": "debug"}
SECRET_FIELDS = ("stream_key", "key", "bearer_token", "password")


class Obs:
    """obs-websocket client. A reader task routes replies by id and banks events."""

    def __init__(self, ws):
        self.ws = ws
        self.next_id = 0
        self.pending = {}
        self.events = []
        self.called = set()
        self.changed = asyncio.Event()
        self.reader = None

    def start(self):
        self.reader = asyncio.create_task(self._read())

    async def _read(self):
        try:
            async for raw in self.ws:
                msg = json.loads(raw)
                if msg["op"] == 5:
                    data = msg["d"]
                    if data.get("eventType") == "VendorEvent" and data["eventData"].get("vendorName") == VENDOR:
                        self.events.append(data["eventData"])
                        self.changed.set()
                elif msg["op"] == 7:
                    future = self.pending.pop(msg["d"]["requestId"], None)
                    if future and not future.done():
                        future.set_result(msg["d"])
        finally:
            for future in self.pending.values():
                if not future.done():
                    future.set_exception(ConnectionError("connection to OBS closed"))
            self.pending.clear()

    async def send(self, request_type, data=None):
        """Send without waiting, returning the future for the reply."""
        self.next_id += 1
        rid = str(self.next_id)
        future = asyncio.get_running_loop().create_future()
        self.pending[rid] = future
        await self.ws.send(json.dumps({
            "op": 6,
            "d": {"requestType": request_type, "requestId": rid, "requestData": data or {}},
        }))
        return future

    async def vendor_send(self, request_type, data=None):
        self.called.add(request_type)
        return await self.send("CallVendorRequest", {
            "vendorName": VENDOR, "requestType": request_type, "requestData": data or {},
        })

    @staticmethod
    def unwrap(reply):
        status = reply["requestStatus"]
        if not status["result"]:
            return {"__transport_error": status.get("comment"), "__code": status.get("code")}
        return reply.get("responseData", {}).get("responseData", {})

    async def vendor(self, request_type, data=None):
        """CallVendorRequest, unwrapped to the vendor's own response. Never hangs."""
        future = await self.vendor_send(request_type, data)
        try:
            return self.unwrap(await asyncio.wait_for(future, REQUEST_TIMEOUT))
        except asyncio.TimeoutError:
            return {"__timeout": f"no reply to {request_type} in {REQUEST_TIMEOUT}s"}

    async def wait_event(self, event_type, predicate=lambda d: True, timeout=EVENT_TIMEOUT):
        """Return the data of the first banked or incoming match, removing it."""
        loop = asyncio.get_running_loop()
        end = loop.time() + timeout
        while True:
            for event in self.events:
                data = event.get("eventData") or {}
                if event.get("eventType") == event_type and predicate(data):
                    self.events.remove(event)
                    return data
            remaining = end - loop.time()
            if remaining <= 0:
                return None
            self.changed.clear()
            try:
                await asyncio.wait_for(self.changed.wait(), remaining)
            except asyncio.TimeoutError:
                pass


async def connect(port, password):
    ws = await websockets.connect(f"ws://127.0.0.1:{port}", max_size=None)
    hello = json.loads(await ws.recv())
    # Vendors is bit 9; bit 16 would be InputVolumeMeters.
    identify = {"rpcVersion": hello["d"]["rpcVersion"], "eventSubscriptions": 1 << 9}
    auth = hello["d"].get("authentication")
    if auth:
        if not password:
            raise SystemExit("obs-websocket requires a password; use --password or OBS_WS_PASSWORD")
        secret = base64.b64encode(hashlib.sha256((password + auth["salt"]).encode()).digest())
        identify["authentication"] = base64.b64encode(
            hashlib.sha256(secret + auth["challenge"].encode()).digest()).decode()
    await ws.send(json.dumps({"op": 1, "d": identify}))
    while json.loads(await ws.recv())["op"] != 2:
        pass
    obs = Obs(ws)
    obs.start()
    return obs


RESULTS = []


def check(name, ok, detail):
    RESULTS.append((name, ok))
    tag = "PASS" if ok is True else ("SKIP" if ok is None else "FAIL")
    print(f"[{tag}] {name}\n       {detail}", flush=True)


def dump(value):
    return json.dumps(value)[:400]


def is_error(response, code):
    return response.get("success") is False and response.get("error_code") == code \
        and bool(response.get("error"))


def canvas_names(output):
    return [c.get("name") if isinstance(c, dict) else None for c in output.get("canvases", [])]


def leaked_fields(value):
    blob = json.dumps(value)
    return [k for k in SECRET_FIELDS if f'"{k}"' in blob]


async def test_read_requests(obs):
    """Returns the get_outputs response, or None when the vendor is not there."""
    st = await obs.vendor("status")
    ok = st.get("success") is True and st.get("api") == 1 and "version" in st \
        and isinstance(st.get("main_active"), bool)
    check("status answers with version, api and main_active", ok, dump(st))
    if "__transport_error" in st or "__timeout" in st:
        return None

    r = await obs.vendor("version")
    check("version answers", r.get("success") is True and bool(r.get("version")), dump(r))

    canvases = await obs.vendor("get_canvas")
    check("get_canvas answers", canvases.get("success") is True, dump(canvases))

    outputs = await obs.vendor("get_outputs")
    items = outputs.get("outputs")
    shape = isinstance(items, list) and isinstance(outputs.get("main_active"), bool) and all(
        isinstance(o.get("name"), str) and "type" in o and isinstance(o.get("active"), bool)
        and isinstance(o.get("reconnecting"), bool) and isinstance(o.get("canvases"), list)
        for o in items)
    check("get_outputs returns the contract fields", outputs.get("success") is True and shape, dump(outputs))

    leaked = leaked_fields(outputs)
    check("no secret field in the get_outputs response", not leaked, f"present: {leaked}")

    named = all(n for o in items or [] for n in canvas_names(o))
    check("every output canvas has a real name", named, dump([o.get("canvases") for o in items or []]))

    r = await obs.vendor("get_outputs", {"canvas": MISSING})
    check("get_outputs with an unknown canvas reports canvas_not_found", is_error(r, "canvas_not_found"), dump(r))

    listed = canvases.get("canvas") or []
    if listed:
        first = listed[0]
        by_name = await obs.vendor("get_outputs", {"canvas": first["name"]})
        by_uuid = await obs.vendor("get_outputs", {"canvas": first["uuid"]})
        names = sorted(o["name"] for o in by_name.get("outputs", []))
        check("get_outputs filters to the canvas, by name and by uuid alike",
              by_name.get("success") is True and all(
                  first["name"] in canvas_names(o) for o in by_name["outputs"])
              and names == sorted(o["name"] for o in by_uuid.get("outputs", [])),
              f"canvas={first['name']!r} outputs={names}")

        name = first["name"]
        for request in ("current_scene", "get_scenes", "get_transitions"):
            r = await obs.vendor(request, {"canvas": name})
            check(f"{request} answers for canvas {name!r}", r.get("success") is True, dump(r))
    else:
        check("canvas-scoped requests", None, "get_canvas listed no canvas")

    r = await obs.vendor("get_dock_modes")
    check("get_dock_modes answers", r.get("success") is True, dump(r))
    r = await obs.vendor("get_docks")
    check("get_docks reports success:true", r.get("success") is True, dump(r))
    r = await obs.vendor("get_live_scenes")
    check("get_live_scenes answers", r.get("success") is True, dump(r))
    return outputs


async def test_error_paths(obs):
    for request in ("start_output", "stop_output"):
        r = await obs.vendor(request, {})
        check(f"{request} without a name reports name_not_set", is_error(r, "name_not_set"), dump(r))
        r = await obs.vendor(request, {"output": MISSING})
        check(f"{request} with an unknown name reports output_not_found", is_error(r, "output_not_found"), dump(r))

    r = await obs.vendor("get_stats", {"output": MISSING})
    check("get_stats with an unknown name reports output_not_found", is_error(r, "output_not_found"), dump(r))

    r = await obs.vendor("current_scene", {"canvas": MISSING})
    check("current_scene with an unknown canvas says why", bool(r.get("error")), dump(r))

    r = await obs.vendor("switch_scene", {"scene": MISSING, "canvas": MISSING})
    check("switch_scene with an unknown canvas says so",
          r.get("success") is False and r.get("error") == "'canvas' not found", dump(r))
    r = await obs.vendor("switch_scene", {"scene": MISSING})
    check("switch_scene without a canvas fails when no canvas has the scene", r.get("success") is False, dump(r))
    canvases = (await obs.vendor("get_canvas")).get("canvas") or []
    extra = next((c for c in canvases if c.get("type") == "extra"), None)
    if extra:
        r = await obs.vendor("switch_scene", {"scene": MISSING, "canvas": extra["name"]})
        check(f"switch_scene with an unknown scene on {extra['name']!r} says so",
              r.get("success") is False and r.get("error") == "'scene' not found", dump(r))
    else:
        check("switch_scene with an unknown scene on a real canvas", None, "no extra canvas to try")

    for request, data in ERROR_PATHS:
        r = await obs.vendor(request, data)
        check(f"{request} {json.dumps(data)} fails cleanly", r.get("success") is False, dump(r))


async def test_get_stats(obs):
    r = await obs.vendor("get_stats")
    stats = r.get("stats")
    ok = r.get("success") is True and isinstance(stats, list) \
        and all(STAT_FIELDS <= set(s) and s["bitrate_kbps"] == 0 for s in stats)
    check("get_stats returns the contract fields, with bitrate_kbps 0 on a pull", ok, dump(r))


async def test_logging(obs):
    saved = await obs.vendor("get_logging")
    ok = saved.get("success") is True and all(k in saved for k in LOGGING_FIELDS)
    check("get_logging returns every field", ok, dump(saved))
    if not ok:
        return
    current = {k: saved[k] for k in LOGGING_FIELDS}
    check("logging is at the documented defaults", True if current == LOGGING_DEFAULTS else None,
          f"current={current}")

    try:
        wanted = {"log_events": not saved["log_events"], "stats_events": not saved["stats_events"],
                  "stats_interval_ms": 1500, "level": "warning"}
        r = await obs.vendor("set_logging", wanted)
        got = await obs.vendor("get_logging")
        check("set_logging applies every field and get_logging agrees",
              r.get("success") is True and all(r.get(k) == v and got.get(k) == v for k, v in wanted.items()),
              f"set={dump(r)} get={dump(got)}")

        before = await obs.vendor("get_logging")
        r = await obs.vendor("set_logging", {"level": "shouting", "stats_interval_ms": 9000})
        after = await obs.vendor("get_logging")
        check("an invalid level reports invalid_level and applies nothing",
              is_error(r, "invalid_level") and before == after, f"reply={dump(r)} after={dump(after)}")

        r = await obs.vendor("set_logging", {"stats_interval_ms": 1})
        check("stats_interval_ms is held at the 250 minimum", r.get("stats_interval_ms") == 250, dump(r))
        r = await obs.vendor("set_logging", {"stats_interval_ms": 999999})
        check("stats_interval_ms is held at the 60000 maximum", r.get("stats_interval_ms") == 60000, dump(r))
    finally:
        r = await obs.vendor("set_logging", current)
        check("logging settings restored", all(r.get(k) == v for k, v in current.items()), dump(r))


async def test_concurrency(obs, rounds=50):
    """Pipelines requests that hop to the UI thread, to shake out threading bugs."""
    requests = ("get_outputs", "get_canvas", "get_docks", "get_dock_modes", "get_stats", "status")
    futures = [await obs.vendor_send(requests[i % len(requests)]) for i in range(rounds * len(requests))]
    try:
        replies = await asyncio.wait_for(asyncio.gather(*futures), REQUEST_TIMEOUT * 6)
    except asyncio.TimeoutError:
        check("pipelined requests all answer", False, f"timed out waiting for {len(futures)} replies")
        return
    bad = [r for r in map(Obs.unwrap, replies) if r.get("success") is not True]
    check(f"{len(futures)} pipelined requests all succeed", not bad,
          dump(bad[:3]) if bad else "no failures, no dock_unavailable")


async def wait_state(obs, name, active):
    return await obs.wait_event("output_state_changed",
                                lambda d: d.get("name") == name and d.get("active") is active)


async def test_live(obs, name):
    r = await obs.vendor("get_stats", {"output": name})
    check("get_stats on a stopped output returns an empty list",
          r.get("success") is True and r.get("stats") == [], dump(r))
    r = await obs.vendor("stop_output", {"output": name})
    check("stop_output on a stopped output reports output_not_running", is_error(r, "output_not_running"), dump(r))

    r = await obs.vendor("start_output", {"output": name})
    started = r.get("success") is True
    check("start_output is accepted", started, dump(r))
    if not started:
        if r.get("error_code") == "start_failed":
            print("       (an output that depends on the main stream fails this way while it is down)")
        return

    r = await obs.vendor("start_output", {"output": name})
    check("an immediate second start_output is refused",
          is_error(r, "already_starting_or_active"), dump(r))

    state = await wait_state(obs, name, True)
    check("output_state_changed reports the output active", state is not None, dump(state))
    legacy = await obs.wait_event("start_output", lambda d: d.get("output") == name, timeout=2)
    check("the existing start_output event still carries `output`", legacy is not None, dump(legacy))

    outputs = await obs.vendor("get_outputs")
    item = next((o for o in outputs.get("outputs", []) if o["name"] == name), {})
    check("get_outputs shows the output active", item.get("active") is True, dump(item))

    first = await obs.vendor("get_stats", {"output": name})
    await asyncio.sleep(3)
    second = await obs.vendor("get_stats", {"output": name})
    a, b = (first.get("stats") or [{}])[0], (second.get("stats") or [{}])[0]
    check("get_stats shows the output moving data",
          STAT_FIELDS <= set(b) and b.get("active") is True and b["total_bytes"] > a.get("total_bytes", 0),
          f"bytes {a.get('total_bytes')} -> {b.get('total_bytes')}")

    stats = await obs.wait_event("output_stats", lambda d: d.get("name") == name and d.get("bitrate_kbps", 0) > 0)
    check("output_stats arrives with a real bitrate",
          stats is not None and STAT_FIELDS <= set(stats), dump(stats))

    logs = [e.get("eventData") or {} for e in obs.events if e.get("eventType") == "log"]
    check("log events carry level, where, message and ts_ms",
          bool(logs) and all({"level", "where", "message", "ts_ms"} <= set(entry) for entry in logs),
          f"{len(logs)} events, first={dump(logs[:1])}")
    leaked = leaked_fields(obs.events)
    check("no secret field in any vendor event", not leaked, f"present: {leaked}")

    r = await obs.vendor("stop_output", {"output": name})
    check("stop_output is accepted", r.get("success") is True, dump(r))
    state = await wait_state(obs, name, False)
    check("output_state_changed reports the stop with a stop_code",
          state is not None and "stop_code" in state, dump(state))
    legacy = await obs.wait_event("stop_output", lambda d: d.get("output") == name, timeout=2)
    check("the stop_output event carries the output name", legacy is not None, dump(legacy))

    r = await obs.vendor("start_output", {"output": name})
    check("the output starts again after a stop", r.get("success") is True, dump(r))
    if r.get("success") is True:
        check("it reaches active again", await wait_state(obs, name, True) is not None, "")
        await obs.vendor("stop_output", {"output": name})
        check("and stops again", await wait_state(obs, name, False) is not None, "")


async def run(args):
    try:
        obs = await connect(args.port, args.password)
    except OSError as e:
        print(f"could not reach obs-websocket on port {args.port}: {e}")
        return 2
    print("connected to obs-websocket\n")

    outputs = await test_read_requests(obs)
    if outputs is None:
        print("\nThe vendor did not respond -- is the plugin loaded?")
        return 2

    await test_error_paths(obs)
    await test_get_stats(obs)
    await test_logging(obs)
    await test_concurrency(obs)

    if args.live:
        target = next((o for o in outputs.get("outputs", []) if o["name"] == args.output), None)
        if target is None:
            check(f"output {args.output!r} exists", False, dump(outputs))
        elif target["active"]:
            check(f"output {args.output!r} is stopped before the live run", False,
                  "already active; refusing to touch a stream this test did not start")
        else:
            saved = await obs.vendor("get_logging")
            await obs.vendor("set_logging", {"log_events": True, "stats_events": True,
                                             "stats_interval_ms": 500, "level": "debug"})
            try:
                await test_live(obs, args.output)
            finally:
                final = await obs.vendor("get_outputs")
                if any(o["name"] == args.output and o["active"] for o in final.get("outputs", [])):
                    await obs.vendor("stop_output", {"output": args.output})
                    print(f"stopped {args.output!r} on the way out")
                await obs.vendor("set_logging", {k: saved[k] for k in LOGGING_FIELDS if k in saved})
    else:
        check("start/stop and events on a real output", None, "needs --live --output NAME")

    print()
    failed = [n for n, ok in RESULTS if ok is False]
    print(f"{sum(1 for _, ok in RESULTS if ok is True)} passed, {len(failed)} failed, "
          f"{sum(1 for _, ok in RESULTS if ok is None)} skipped")
    for n in failed:
        print(f"  FAILED: {n}")
    print(f"never sent: {', '.join(sorted(ALL_REQUESTS - obs.called))}")
    print("state-changing requests above ran on their error path only")
    await obs.ws.close()
    return 1 if failed else 0


def main():
    ap = argparse.ArgumentParser(description="End-to-end test of the aitum-stream-suite vendor.")
    ap.add_argument("--password", default=os.environ.get("OBS_WS_PASSWORD", ""))
    ap.add_argument("--port", type=int, default=4455)
    ap.add_argument("--live", action="store_true", help="start and stop --output as a real stream")
    ap.add_argument("--output", default=None, help="name of the output to use with --live")
    args = ap.parse_args()
    if args.live and not args.output:
        ap.error("--live needs --output NAME: the test only streams to an output you chose")
    return asyncio.run(run(args))


if __name__ == "__main__":
    sys.exit(main())
