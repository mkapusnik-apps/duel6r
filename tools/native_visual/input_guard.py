"""One bounded input transaction on an exclusively owned private X display."""
import sys
import threading

from .common import require

MAX_INPUT_SECONDS = 2.0
RELEASE_RESERVE = .1


def hold_input(client, down, up, value, modifiers, hold, deadline, clock, sleep, timing):
    # Focus is established before any press, with its own recipe-bounded wait.
    client.prepare_input(min(deadline, clock() + .5))
    started = clock()
    release_at = min(started + hold, started + MAX_INPUT_SECONDS - RELEASE_RESERVE)
    finish_by = min(deadline, release_at + RELEASE_RESERVE)
    require(release_at < finish_by, "no budget for native input release")
    state = {"started": started, "release_target": release_at, "deadline": finish_by,
             "release_started": None, "release_finished": None, "release_errors": []}
    release_now, ready = threading.Event(), threading.Event()
    releases = [(up, value)] + [("keyup", key) for key in reversed(modifiers)]

    def guard():
        ready.set()
        release_now.wait(max(0, release_at - clock()))
        state["release_started"] = clock()
        try:
            # Independent of press acknowledgements. All releases are dispatched
            # before waiting for any of their acknowledgements.
            state["release_errors"] = client.release_all(releases, deadline=finish_by)
        except Exception as error:
            state["release_errors"] = [str(error)]
        finally:
            state["release_finished"] = clock()

    thread = threading.Thread(target=guard, name="native-input-release", daemon=True)
    thread.start()
    try:
        require(ready.wait(max(0, finish_by - clock())), "release guard startup deadline")
        for action, key in [("keydown", key) for key in modifiers] + [(down, value)]:
            require(clock() < release_at, "native press deadline expired")
            client.input(action, key, deadline=release_at)
            require(clock() < release_at, "native press acknowledgement exceeded deadline")
        sleep(max(0, release_at - clock()))
    finally:
        active_error = sys.exc_info()[0] is not None
        release_now.set()
        thread.join(max(0, finish_by - clock()))
        timing.update(state)
        overrun = (thread.is_alive() or state["release_finished"] is None
                   or state["release_finished"] > finish_by or clock() > finish_by)
        timing["overrun"] = overrun
        if not active_error:
            require(not overrun, "native input/release deadline exceeded")
            require(not state["release_errors"], "native input release failed")
