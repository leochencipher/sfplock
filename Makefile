# sfplock - simple fingerprint screen locker
# See LICENSE file for copyright and license details.

include config.mk

SRC = sfplock.c
OBJ = ${SRC:.c=.o}
all: options sfplock

options:
	@echo sfplock build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

sfplock: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f sfplock ${OBJ} sfplock-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p sfplock-${VERSION}
	@cp -R LICENSE Makefile README config.mk ${SRC} sfplock-${VERSION}
	@tar -cf sfplock-${VERSION}.tar sfplock-${VERSION}
	@gzip sfplock-${VERSION}.tar
	@rm -rf sfplock-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f sfplock ${DESTDIR}${PREFIX}/bin

	@chmod 755 ${DESTDIR}${PREFIX}/bin/sfplock
	@chmod u+s ${DESTDIR}${PREFIX}/bin/sfplock

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/sfplock
.PHONY: all options clean dist install uninstall
