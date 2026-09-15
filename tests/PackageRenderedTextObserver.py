"""GDB test support: assert allowlisted text drawn by the packaged Linux client.

Linux x86-64/libstdc++ C++11 ABI only. This observes Font::print calls in the
unaltered executable; it is not an accessibility API or a pixel/visual oracle.
No screenshot, arbitrary UI text, packet, credential, or memory dump is saved.
The driver requests text predicates; only predicate indexes and frame counts
leave the debugger. Software breakpoints affect timing: never use this for
performance, startup-deadline, reconnect-deadline, or race acceptance.
"""

import json
import os
from pathlib import Path
import struct

import gdb


request_path = Path(os.environ["D6R_TEXT_REQUEST"])
result_path = Path(os.environ["D6R_TEXT_RESULT"])
request = {"id": None, "predicates": []}
matched = set()
frames = 0
stamp = None


def refresh():
    global request, matched, stamp
    current = request_path.stat().st_mtime_ns
    if current != stamp:
        request = json.loads(request_path.read_text())
        matched = set()
        stamp = current


def publish(value):
    temporary = result_path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value))
    temporary.replace(result_path)


class Text(gdb.Breakpoint):
    string_register = "$r8"
    coordinates = True

    def stop(self):
        stage = "refresh"
        try:
            refresh()
            # SysV AMD64: this, x, y, color reference, std::string reference.
            stage = "string-register"
            address = int(gdb.parse_and_eval(self.string_register))
            memory = gdb.selected_inferior()
            stage = "string-layout"
            pointer, length = struct.unpack("<QQ", memory.read_memory(address, 16))
            if length == 0 or length > 2048:
                return False
            stage = "string-text"
            text = bytes(memory.read_memory(pointer, length)).decode("utf-8", errors="strict")
            stage = "coordinates"
            x = int(gdb.parse_and_eval("$esi")) if self.coordinates else None
            y = int(gdb.parse_and_eval("$edx")) if self.coordinates else None
            for index, predicate in enumerate(request["predicates"]):
                hit = (text == predicate["text"] if "text" in predicate
                       else predicate["contains"] in text)
                if hit and ("x" not in predicate or predicate["x"] == x) \
                        and ("y" not in predicate or predicate["y"] == y):
                    matched.add(index)
        except Exception as error:
            publish({"id": request.get("id"), "error": "rendered-text observer failed: " + stage + ":" + type(error).__name__})
            gdb.execute("quit 2")
        return False


class Frame(gdb.Breakpoint):
    def stop(self):
        global frames, matched
        refresh()
        frames += 1
        if request["predicates"] and len(matched) == len(request["predicates"]):
            publish({"id": request["id"], "matched": sorted(matched), "frame": frames})
        matched = set()
        return False


class FloatText(Text):
    # Floating point coordinates consume SSE registers, not integer registers.
    string_register = "$rdx"
    coordinates = False


gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set print thread-events off")
gdb.execute("set breakpoint pending on")
def exited(event):
    # GDB itself can exit zero after a nonzero inferior exit; preserve the latter.
    result_path.with_suffix(".exit").write_text(json.dumps({"exit_code": getattr(event, "exit_code", None)}))


gdb.events.exited.connect(exited)
# Address breakpoint must stop before the optimized function prologue changes r8.
Text("*_ZNK5Duel64Font5printEiiRKNS_5ColorERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE", internal=True)
FloatText("*_ZNK5Duel64Font5printEfffRKNS_5ColorERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEf", internal=True)
Frame("SDL_GL_SwapWindow", internal=True)
gdb.execute("run")
