/* Test-only event observer. Never used to supply game evidence. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <time.h>
int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (!display) return 2;
    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 850, 700, 0, 0, 0);
    XStoreName(display, window, "Native visual input test fixture");
    XSelectInput(display, window, KeyPressMask | KeyReleaseMask);
    XMapWindow(display, window); XFlush(display);
    printf("%lu\n", window); fflush(stdout);
    for (;;) {
        XEvent event; XNextEvent(display, &event);
        if (event.type == KeyPress || event.type == KeyRelease) {
            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
            printf("%s %.9f\n", event.type == KeyPress ? "down" : "up", now.tv_sec + now.tv_nsec/1e9);
            fflush(stdout);
        }
    }
}
