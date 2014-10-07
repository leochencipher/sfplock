/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
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

static int fprint_verify(char * user) {
    FILE * fp;
    char buf[80];
    int ret = 0;
    int uid = -1;
    char cmd [] = FPRINTD_PREFIX"/bin/fprintd-verify ";
    uid = getuid();
    strcat(cmd, user);
    setuid(0); //set uid 0 to get permission of reading everyone's fingerprint.
    fp = popen(cmd, "r");
    setuid(uid); //set uid back to normal user.
    syslog(LOG_INFO, "cmd:%s", cmd);
    while(!feof(fp)) {
        fgets(buf,sizeof(buf),fp);
        syslog(LOG_INFO, "%s", buf);
        if (strstr(buf,"verify-match")!=NULL) {
            ret = 0;
            break;
        }
        else if (strstr(buf,"verify-no-match")!=NULL) {
            ret = 1;
            sleep(3);
            break;
        }
        else if (strstr(buf,"failed to claim device")!=NULL) {
            ret = 2;
            sleep(3);
            break;
        }
    }
    ret |= pclose(fp);
    return ret;
}

static void
wait_fingerprint(Display *dpy, char * user)
{
    int screen;
    running = True;
    while(running) {
        running = fprint_verify(user);
        if(running){
            sleep(1);//prevent calling fprintd-verify too fast when fingerprint device doesn't work.
        }
        for(screen = 0; screen < nscreens; screen++) {
            XRaiseWindow(dpy, locks[screen]->win);
        }
    }
}
static void
ensure_fprint_exists(char * user) {
    FILE * fp;
    char buf[100];
    char cmd [] = FPRINTD_PREFIX"/bin/fprintd-list ";
    strcat(cmd, user);
    fp = popen(cmd, "r");
    while(!feof(fp)) {
        fgets(buf, sizeof(buf), fp);
        fprintf(stderr, "%s", buf);
        if (strstr(buf,"Fingerprints for user")!=NULL) {
            pclose(fp);
            return;
        }
    }
    pclose(fp);
    die("sfplock: fingerprint not found for user '%s'.\n", user);
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
    int nlocks = 0;
    int uid = -1;
    struct passwd *pwd;
    char *user = NULL;

    if((argc == 2) && !strcmp("-v", argv[1]))
        die("sfplock-%s © 2012 Rains<rains31(at)gmail.com>\nbased on slock-1.1 © 2006-2012 Anselm R Garbe\n", VERSION);

    if(!(dpy = XOpenDisplay(0)))
        die("sfplock: cannot open display\n");

    /* Get the number of screens in display "dpy" and blank them all. */
    nscreens = ScreenCount(dpy);
    locks = malloc(sizeof(Lock *) * nscreens);

    if(locks == NULL)
        die("sfplock: malloc: %s\n", strerror(errno));

    uid = getuid();
    pwd = getpwuid(uid);
    user = pwd->pw_name;

    ensure_fprint_exists(user);

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
    openlog("sfplock", LOG_PID, LOG_USER);
    /* Everything is now blank. Now wait for the correct finger... */
    wait_fingerprint(dpy, user);
    closelog();
    /* verify ok, unlock everything and quit. */
    for(screen = 0; screen < nscreens; screen++)
        unlockscreen(dpy, locks[screen]);
    free(locks);
    XCloseDisplay(dpy);
    return 0;
}
