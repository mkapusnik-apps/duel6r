"""Execute the exact production staging/publication block under allocation failure.

This is a scoped exception-safety probe, not thread recovery or crypto evidence.
Live common-channel SDK ownership cases live in SessionTransportTests.cpp. No production
seam, replacement container allocator, or copied publication algorithm is used.
"""
import pathlib
import subprocess
import sys
import tempfile


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


root = pathlib.Path(sys.argv[1])
compiler = sys.argv[2]
transport = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else root / "source/network/SessionTransport.cpp"
text = transport.read_text()
start = text.index("                std::vector<std::uint8_t> payload(payloadSize);")
end = text.index("                if (payloadSize > 0 && !readExact", start)
block = text[start:end]
trust = (root / "source/network/NetworkTrustPolicy.cpp").read_text()
erase = function(trust, "void secureErase(void *target")
erase += "\n" + function(trust, "void secureEraseMemory(void *target")
program = r'''
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <new>
#include "source/network/SessionTransport.h"
namespace Duel6::Network::Trust { ERASE }
using namespace Duel6::Network;
using Clock = std::chrono::steady_clock;
static bool armed = false, injected = false, freed = false, wiped = false;
static void *watched = nullptr;
static constexpr std::size_t payloadSize = 48;
void *operator new(std::size_t size) {
    if (armed) { armed = false; injected = true; throw std::bad_alloc(); }
    if (void *value = std::malloc(size)) return value;
    throw std::bad_alloc();
}
void release(void *value) noexcept {
    if (value && value == watched) {
        const auto *bytes = static_cast<const unsigned char *>(value);
        wiped = std::all_of(bytes, bytes + payloadSize, [](unsigned char b) { return b == 0; });
        freed = true; watched = nullptr;
    }
    std::free(value);
}
void operator delete(void *value) noexcept { release(value); }
void operator delete(void *value, std::size_t) noexcept { release(value); }
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main() {
  try {
    for (bool failing : {true, false}) {
      std::deque<TransportFrame> input;
      // libstdc++ default allocator: force the next push to allocate a real node.
      // If this layout changes, the injected=false qualification fails closed.
      const auto initialFrames = 512 / sizeof(TransportFrame) - 1;
      const auto stamp = TransportTimePoint{} + std::chrono::seconds(123);
      for (std::size_t i = 0; i < initialFrames; ++i) input.push_back({{}, stamp});
      std::size_t inputBytes = 0;
      std::mutex inputMutex;
      InboundTransportDependencies inbound;
      std::function<TransportTimePoint()> now = [stamp] { return stamp; };
      constexpr unsigned kind = 0, ApplicationFrame = 0;
      freed = wiped = injected = false;
      inbound.beforeApplicationPublish = [&](TransportTimePoint receivedAt) {
        require(receivedAt == stamp, "publication timestamp changed");
        armed = failing;
      };
      bool caught = false;
      try {
        BLOCK
        std::fill(payload.begin(), payload.end(), 0xa7);
        watched = payload.data();
        deliver();
        require(delivered && payload.empty(), "successful move did not transfer ownership");
      } catch (const std::bad_alloc &) { caught = true; }
      armed = false;
      if (failing) {
        std::cout << "OBSERVATION allocation-exception=" << (injected && caught)
                  << " freed=" << freed << " wiped=" << wiped
                  << " input-bytes=" << inputBytes << " queue-delta=" << (input.size() - initialFrames) << '\n';
        require(injected && caught, "actual deque allocation exception not executed");
        require(freed && wiped, "discarded credential not wiped before actual free");
        require(inputBytes == 0 && input.size() == initialFrames, "exception changed accounting/queue");
        require(std::all_of(input.begin(), input.end(), [stamp](const auto &f) {
          return f.payload.empty() && f.receivedAt == stamp;
        }), "exception changed existing frames/timestamps");
        std::cout << "PASS actual deque allocation throw: wipe-before-free, accounting/queue/timestamps unchanged\n";
      } else {
        require(!caught && !freed && inputBytes == payloadSize && input.size() == initialFrames + 1,
                "successful transfer accounting/ownership changed");
        const auto &frame = input.back();
        require(frame.receivedAt == stamp && frame.payload.size() == payloadSize
          && std::all_of(frame.payload.begin(), frame.payload.end(), [](auto b) { return b == 0xa7; }),
          "successful transfer changed bytes/timestamp");
        Trust::secureEraseMemory(input.back().payload.data(), input.back().payload.size());
        input.pop_back();
        require(freed && wiped, "queue-owned storage not wiped before release");
        std::cout << "PASS successful transfer: bytes/timestamp unchanged, accounting exactly once\n";
      }
    }
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
'''.replace("ERASE", erase).replace("BLOCK", block)
with tempfile.TemporaryDirectory(prefix="d6r-staging-allocation-") as directory:
    source = pathlib.Path(directory) / "probe.cpp"
    binary = pathlib.Path(directory) / "probe"
    source.write_text(program)
    subprocess.run([compiler, "-std=c++17", "-O2", "-pthread", "-I", str(root), str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
