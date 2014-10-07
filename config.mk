# sfplock version
VERSION = 1.0

# Customize below to fit your system

# paths
PREFIX = /usr/local
FPRINTD_PREFIX=/usr

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCS = -I. -I/usr/include -I${X11INC} -I/usr/local/include

LIBS = -L/usr/lib -L/usr/local/lib -lc -L${X11LIB} -lX11 -lXext

# flags
# On *BSD remove -DHAVE_SHADOW_H from CPPFLAGS and add -DHAVE_BSD_AUTH
CPPFLAGS = -DFPRINTD_PREFIX=\"${FPRINTD_PREFIX}\" -DVERSION=\"${VERSION}\" -DHAVE_SHADOW_H  -DCOLOR1=\"black\" -DCOLOR2=\"\#005577\"
CFLAGS = -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS = -s ${LIBS}


# compiler and linker
CC = cc

# Default MODE=4755 and GROUP=root
MODE=4755
GROUP=root
