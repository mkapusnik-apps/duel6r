#!/usr/bin/env python3
"""Linux test-only supervisor. Run in a dedicated process with no other children.

Subreaping retains descendants even when wrappers exit or services create new
sessions. Signal pidfds, never an unanchored PID/PGID. Success requires waitpid
ECHILD, not merely a dead leader, zombies, or an elapsed grace period.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def children():
    return [int(pid) for pid in Path(f"/proc/self/task/{os.getpid()}/children").read_text().split()]


def descendants(pid):
    found = set()
    pending = [pid]
    while pending:
        parent = pending.pop()
        try:
            tasks = list(Path(f"/proc/{parent}/task").iterdir())
        except FileNotFoundError:
            continue
        for task in tasks:
            try:
                direct = [int(value) for value in (task / "children").read_text().split()]
            except FileNotFoundError:
                continue
            for child in direct:
                if child not in found:
                    found.add(child)
                    pending.append(child)
    return found


def signal_tree(sig):
    # Open the handle first, then reconfirm ancestry. PID reuse after that point
    # cannot redirect the signal to a new, unrelated process.
    for pid in descendants(os.getpid()):
        try:
            fd = os.pidfd_open(pid)
        except ProcessLookupError:
            continue
        try:
            if pid in descendants(os.getpid()):
                signal.pidfd_send_signal(fd, sig)
        except ProcessLookupError:
            pass
        finally:
            os.close(fd)


def reap():
    statuses = {}
    while True:
        try:
            pid, status = os.waitpid(-1, os.WNOHANG)
        except ChildProcessError:
            return True, statuses
        if pid == 0:
            return False, statuses
        statuses[pid] = os.waitstatus_to_exitcode(status)


def run_owned(command, timeout, log, *, cwd=None, env=None, stdout=None, stderr=None):
    if children():
        raise RuntimeError("test supervisor must not own unrelated children")
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        raise OSError(ctypes.get_errno(), "cannot enable test subreaper")
    started = time.monotonic()
    cancelled = []
    handlers = {}
    for sig in (signal.SIGINT, signal.SIGTERM):
        handlers[sig] = signal.signal(sig, lambda signum, frame: cancelled.append(signum))
    process = None
    result = {"phase": "launch", "exit": None, "cleanup": False, "reaped": {}}
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)

    def record(event):
        record = dict(result, event=event, elapsed=round(time.monotonic() - started, 3),
                      timestamp=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
        with log.open("a") as output:
            output.write(json.dumps(record, sort_keys=True) + "\n")
        print(json.dumps(record, sort_keys=True), flush=True)

    try:
        record("begin")
        process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stdout, stderr=stderr,
                                   start_new_session=True)
        result["phase"] = "running"
        while not cancelled and time.monotonic() - started < timeout:
            # Observe without reaping; cleanup still owns every descendant.
            exit_info = os.waitid(os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
            if exit_info is not None:
                result["exit"] = (exit_info.si_status if exit_info.si_code == os.CLD_EXITED
                                  else -exit_info.si_status)
                result["phase"] = "exited"
                break
            time.sleep(0.02)
        else:
            result["phase"] = "cancelled" if cancelled else "timeout"
    except BaseException:
        result["phase"] = "exception"
        raise
    finally:
        try:
            result["remaining"] = sorted(descendants(os.getpid()))
            result["process_state"] = {}
            for pid in result["remaining"]:
                try:
                    # State/parent/group/session only; never argv or environment.
                    result["process_state"][pid] = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()[:4]
                except FileNotFoundError:
                    pass
            record("cleanup-begin")
            for sig, grace in ((signal.SIGTERM, 1.0), (signal.SIGKILL, 3.0)):
                deadline = time.monotonic() + grace
                while True:
                    signal_tree(sig)
                    complete, statuses = reap()
                    result["reaped"].update(statuses)
                    if complete:
                        result["cleanup"] = True
                        break
                    if time.monotonic() >= deadline:
                        break
                    time.sleep(0.02)
                if result["cleanup"]:
                    break
                result["forced"] = True
            result["remaining"] = sorted(descendants(os.getpid()))
            if process is not None and process.pid in result["reaped"]:
                process.returncode = result["reaped"][process.pid]
            record("end")
        finally:
            for sig, handler in handlers.items():
                signal.signal(sig, handler)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    result = run_owned(command, args.timeout, args.log)
    if not result["cleanup"]:
        return 125
    if result["phase"] != "exited":
        return 124
    return result["exit"] if result["exit"] >= 0 else 128 - result["exit"]


if __name__ == "__main__":
    raise SystemExit(main())
