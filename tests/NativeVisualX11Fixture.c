/* Test-only event observer. Never used to supply game evidence. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (!display) return 2;
    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 850, 700, 0, 0, 0);
    XStoreName(display, window, "Native visual input test fixture");
    unsigned long pid = (unsigned long)getpid();
    XChangeProperty(display, window, XInternAtom(display, "_NET_WM_PID", False), XA_CARDINAL,
                    32, PropModeReplace, (unsigned char *)&pid, 1);
    XSelectInput(display, window, KeyPressMask | KeyReleaseMask);
    /* Only this private test display: observe physical edges, not autorepeat. */
    XAutoRepeatOff(display);
    XMapWindow(display, window); XFlush(display);
    printf("%lu\n", window); fflush(stdout);
    for (;;) {
        XEvent event; XNextEvent(display, &event);
        if (event.type == KeyPress || event.type == KeyRelease) {
            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
            printf("%s %.9f %s\n", event.type == KeyPress ? "down" : "up", now.tv_sec + now.tv_nsec/1e9,
                   XKeysymToString(XLookupKeysym(&event.xkey, 0)));
            fflush(stdout);
            if (event.type == KeyPress && XLookupKeysym(&event.xkey, 0) == XK_Escape) {
#ifdef TEST_LEAK
                if (fork() == 0) { signal(SIGTERM, SIG_IGN); for (;;) pause(); }
#endif
                XCloseDisplay(display);
                return 0;
            }
        }
    }
}
