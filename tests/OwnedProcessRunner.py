#!/usr/bin/env python3
"""Linux test-only supervisor. Run in a dedicated process with no other children.

Subreaping retains descendants even when wrappers exit or services create new
sessions. Signal pidfds, never an unanchored PID/PGID. Success requires waitpid
ECHILD, not merely a dead leader, zombies, or an elapsed grace period.
"""
import argparse
import ctypes
import errno
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
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
            except (FileNotFoundError, ProcessLookupError):
                # An enumerated task may exit before its children file is read.
                continue
            for child in direct:
                if child not in found:
                    found.add(child)
                    pending.append(child)
    return found


def observed_disappearance(record):
    """Evidence of absence, not proof of the syscall's kernel path or cleanup."""
    return (record is not None and record["errno"] == errno.EINVAL
            and record["target_pid"] > 0 and record["flags"] == 0
            and record["owned_at_census"] is True
            and record["owned_at_recheck"] is False and record["identity"] == {}
            and record["observation_errors"] == [
                {"source": "status", "errno": errno.ENOENT},
                {"source": "stat", "errno": errno.ENOENT}])


def pidfd_failure_context(pid, flags, error, owned_at_census, *, observation="after-failed-syscall"):
    """Collect fail-closed observations independently of best-effort emission."""
    try:
        record = {"event": "pidfd-open-failure", "target_pid": pid, "flags": flags,
                  "errno": error.errno, "owned_at_census": owned_at_census,
                  "anchored": False, "observation": observation,
                  "owned_at_recheck": None, "identity": {}, "observation_errors": []}
        try:
            record["owned_at_recheck"] = pid in descendants(os.getpid())
        except OSError as observation_error:
            record["observation_errors"].append({"source": "ancestry", "errno": observation_error.errno})
        for source in ("status", "stat"):
            try:
                text = Path(f"/proc/{pid}/{source}").read_text()
                if source == "status":
                    fields = dict(line.split(":", 1) for line in text.splitlines() if ":" in line)
                    record["identity"][source] = {key: int(fields[key]) for key in ("Pid", "Tgid", "PPid")}
                    record["identity"][source]["state"] = fields["State"].split()[0]
                else:
                    fields = text.rsplit(")", 1)[1].split()
                    record["identity"][source] = {"pid": int(text.split(" ", 1)[0]),
                        "state": fields[0], "ppid": int(fields[1]), "start_ticks": int(fields[19])}
            except (OSError, ValueError, IndexError, KeyError) as observation_error:
                record["observation_errors"].append({"source": source, "errno": getattr(observation_error, "errno", None)})
        record["classification"] = "provisional-disappearance" if observed_disappearance(record) else "unclassified"
    except BaseException:
        return None  # Collection uncertainty cannot authorize a disappearance.
    try:
        print(json.dumps(record, sort_keys=True), file=sys.stderr, flush=True)
    except BaseException:
        pass  # Output failure must not replace the original error or observations.
    return record


def retain_pidfd_failure(failures, pid, error):
    if not any(target == pid and failure.errno == error.errno for target, failure in failures):
        failures.append((pid, error))


def signal_tree(sig, errors=None, disappearances=None):
    # Open the handle first, then reconfirm ancestry. PID reuse after that point
    # cannot redirect the signal to a new, unrelated process.
    failures = [] if errors is None else errors
    owned = descendants(os.getpid())
    for pid in owned:
        try:
            fd = os.pidfd_open(pid, 0)
        except ProcessLookupError:
            continue
        except OSError as error:
            # Observe EVERY failure before deduplication: a later absence must
            # never erase an earlier live/unreadable error, or hide the reverse.
            context = pidfd_failure_context(pid, 0, error, pid in owned)
            if errors is not None and disappearances is not None and observed_disappearance(context):
                retain_pidfd_failure(disappearances, pid, error)
            else:
                retain_pidfd_failure(failures, pid, error)
            # No signal without a handle. Other descendants still get cleanup.
            continue
        try:
            if pid in descendants(os.getpid()):
                signal.pidfd_send_signal(fd, sig)
        except ProcessLookupError:
            pass
        finally:
            os.close(fd)
    if errors is None and failures:
        raise failures[0][1]


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
    diagnostic_errors = []
    cleanup_errors = []
    disappearances = []

    def record(event):
        record = dict(result, event=event, elapsed=round(time.monotonic() - started, 3),
                      timestamp=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
        # Neither sink is a prerequisite for terminating/reaping the owned tree.
        # Try both independently and defer reporting failure until after cleanup.
        payload = json.dumps(record, sort_keys=True)
        try:
            log.parent.mkdir(parents=True, exist_ok=True)
            with log.open("a") as output:
                output.write(payload + "\n")
        except OSError as error:
            diagnostic_errors.append(f"{event}: log: {error}")
        try:
            print(payload, flush=True)
        except OSError as error:
            diagnostic_errors.append(f"{event}: stdout: {error}")

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
            try:
                result["remaining"] = sorted(descendants(os.getpid()))
                result["process_state"] = {}
                for pid in result["remaining"]:
                    try:
                        # State/parent/group/session only; never argv or environment.
                        result["process_state"][pid] = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()[:4]
                    except (FileNotFoundError, ProcessLookupError):
                        pass
                record("cleanup-begin")
            finally:
                # Even an unexpected diagnostic/snapshot exception must pass
                # through this boundary before propagating to the caller.
                for sig, grace in ((signal.SIGTERM, 1.0), (signal.SIGKILL, 3.0)):
                    deadline = time.monotonic() + grace
                    while True:
                        signal_tree(sig, cleanup_errors, disappearances)
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
                if process is not None and process.pid in result["reaped"]:
                    process.returncode = result["reaped"][process.pid]
                try:
                    result["remaining"] = sorted(descendants(os.getpid()))
                except Exception as error:
                    if not disappearances:
                        raise
                    result["remaining"] = None  # Unknown is not an empty tree.
                    result["cleanup_observation_errors"] = [
                        {"source": "final-ancestry", "errno": getattr(error, "errno", None)}]
                result["cleanup"] = result["cleanup"] and result["remaining"] == []
                if disappearances:
                    result["pidfd_disappearances"] = []
                    for pid, error in disappearances:
                        context = pidfd_failure_context(pid, 0, error, True, observation="final-cleanup")
                        verified = result["cleanup"] and observed_disappearance(context)
                        result["pidfd_disappearances"].append({"target_pid": pid, "errno": error.errno,
                            "classification": "verified-disappearance" if verified else "unverified-disappearance"})
                        if not verified:
                            retain_pidfd_failure(cleanup_errors, pid, error)
                if cleanup_errors:
                    result["cleanup_errors"] = [{"target_pid": pid, "errno": error.errno}
                                                for pid, error in cleanup_errors]
                record("end")
        finally:
            for sig, handler in handlers.items():
                signal.signal(sig, handler)
    if cleanup_errors:
        raise cleanup_errors[0][1]  # ECHILD alone cannot clear an unclassified failure.
    if diagnostic_errors:
        raise RuntimeError(f"test supervisor diagnostics failed after cleanup={result['cleanup']}: "
                           + "; ".join(diagnostic_errors))
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
