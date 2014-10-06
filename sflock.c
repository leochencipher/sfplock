/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
    int screen;
    Window root, win;
    Pixmap pmap;
    unsigned long colors[2];
} Lock;

static Lock **locks;
static int nscreens;
static Bool running = True;

static void
die(const char *errstr, ...) {
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static int auth() {
    FILE * fp;
    char buf[80];
    int rc = 0;
    fp = popen("/usr/bin/fprintd-verify","r");
    do {
        fgets(buf,sizeof(buf),fp);
        if (strstr(buf,"verify-match")!=NULL) {
            rc = 1;
            break;
        }
        else if (strstr(buf,"verify-no-match")!=NULL) {
            sleep(3);
            break;
        }
    } while(!feof(fp));
    pclose(fp);
    return rc;
}

static void
readfinger(Display *dpy)
{
    int screen;
    running = True;
    while(running) {
        running = !auth();
        for(screen = 0; screen < nscreens; screen++)
        {
            XRaiseWindow(dpy, locks[screen]->win);
        }
    }
}

static void
unlockscreen(Display *dpy, Lock *lock) {
    if(dpy == NULL || lock == NULL)
        return;

    XUngrabPointer(dpy, CurrentTime);
    XFreeColors(dpy, DefaultColormap(dpy, lock->screen), lock->colors, 2, 0);
    XFreePixmap(dpy, lock->pmap);
    XDestroyWindow(dpy, lock->win);
    free(lock);
}

static Lock *
lockscreen(Display *dpy, int screen) {
    char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
    unsigned int len;
    Lock *lock;
    XColor color, dummy;
    XSetWindowAttributes wa;
    Cursor invisible;

    if(dpy == NULL || screen < 0)
        return NULL;

    lock = malloc(sizeof(Lock));
    if(lock == NULL)
        return NULL;

    lock->screen = screen;

    lock->root = RootWindow(dpy, lock->screen);

    /* init */
    wa.override_redirect = 1;
    wa.background_pixel = BlackPixel(dpy, lock->screen);
    lock->win = XCreateWindow(dpy, lock->root, 0, 0, DisplayWidth(dpy, lock->screen), DisplayHeight(dpy, lock->screen),
            0, DefaultDepth(dpy, lock->screen), CopyFromParent,
            DefaultVisual(dpy, lock->screen), CWOverrideRedirect | CWBackPixel, &wa);
    XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen), COLOR2, &color, &dummy);
    lock->colors[1] = color.pixel;
    XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen), COLOR1, &color, &dummy);
    lock->colors[0] = color.pixel;
    lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
    invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap, &color, &color, 0, 0);
    XDefineCursor(dpy, lock->win, invisible);
    XMapRaised(dpy, lock->win);
    for(len = 1000; len; len--) {
        if(XGrabPointer(dpy, lock->root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
            GrabModeAsync, GrabModeAsync, None, invisible, CurrentTime) == GrabSuccess)
            break;
        usleep(1000);
    }
    if(running && (len > 0)) {
        for(len = 1000; len; len--) {
            if(XGrabKeyboard(dpy, lock->root, True, GrabModeAsync, GrabModeAsync, CurrentTime)
                == GrabSuccess)
                break;
            usleep(1000);
        }
    }

    running &= (len > 0);
    if(!running) {
        unlockscreen(dpy, lock);
        lock = NULL;
    }
    else 
        XSelectInput(dpy, lock->root, SubstructureNotifyMask);

    return lock;
}

int
main(int argc, char **argv) {
    Display *dpy;
    int screen;

    if((argc == 2) && !strcmp("-v", argv[1]))
        die("sflock-%s, rains31@gmail.com\nbased on slock-1.1 © 2006-2012 Anselm R Garbe\n", VERSION);

    if(!(dpy = XOpenDisplay(0)))
        die("sflock: cannot open display\n");
    /* Get the number of screens in display "dpy" and blank them all. */
    nscreens = ScreenCount(dpy);
    locks = malloc(sizeof(Lock *) * nscreens);
    if(locks == NULL)
        die("sflock: malloc: %s\n", strerror(errno));
    int nlocks = 0;
    for(screen = 0; screen < nscreens; screen++) {
        if ( (locks[screen] = lockscreen(dpy, screen)) != NULL)
            nlocks++;
    }
    XSync(dpy, False);
    /* Did we actually manage to lock something? */
    if (nlocks == 0) { // nothing to protect
        free(locks);
        XCloseDisplay(dpy);
        return 1;
    }
    /* Everything is now blank. Now wait for the correct finger... */
    readfinger(dpy);

    /* verify ok, unlock everything and quit. */
    for(screen = 0; screen < nscreens; screen++)
        unlockscreen(dpy, locks[screen]);
    free(locks);
    XCloseDisplay(dpy);
    return 0;
}