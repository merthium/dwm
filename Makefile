# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c dwm.c util.c
OBJ = ${SRC:.c=.o}

all: options dwm

options:
	@echo dwm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

dwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

test: tests/bar-labels tests/tray-order
	./tests/bar-labels
	./tests/tray-order

tests/bar-labels: tests/bar-labels.c dwm.c drw.c util.c config.h config.mk drw.h util.h push.c
	${CC} ${CFLAGS} -Wno-unused-function -o $@ tests/bar-labels.c drw.c util.c ${LDFLAGS}

tests/tray-order: tests/tray-order.c dwm.c drw.c util.c config.h config.mk drw.h util.h push.c
	${CC} ${CFLAGS} -Wno-unused-function -o $@ tests/tray-order.c drw.c util.c ${LDFLAGS}

tests/grab-release: tests/grab-release.c dwm.c drw.c util.c config.h config.mk drw.h util.h push.c
	${CC} ${CFLAGS} -Wno-unused-function -o $@ tests/grab-release.c drw.c util.c ${LDFLAGS}

tests/tray-stacking: tests/tray-stacking.c dwm.c drw.c util.c config.h config.mk drw.h util.h push.c
	${CC} ${CFLAGS} -Wno-unused-function -o $@ tests/tray-stacking.c drw.c util.c ${LDFLAGS}

# Use a separate X server and explicitly set DWM_ISOLATED_X_TEST=1.
test-x11: tests/grab-release tests/tray-stacking tests/swallow
	./tests/grab-release
	./tests/tray-stacking
	./tests/swallow
	./tests/swallow --sanitized-test

tests/swallow: tests/swallow.c dwm.c drw.c util.c config.h config.mk drw.h util.h push.c
	${CC} ${CFLAGS} -Wno-unused-function -o $@ tests/swallow.c drw.c util.c ${LDFLAGS}

clean:
	rm -f tests/swallow
	rm -f tests/tray-stacking
	rm -f tests/tray-order
	rm -f tests/bar-labels
	rm -f tests/grab-release
	rm -f dwm ${OBJ} dwm-${VERSION}.tar.gz

dist: clean
	mkdir -p dwm-${VERSION}
	cp -R LICENSE gpl-v3.txt mit-x-consortium.txt Makefile README.md config.def.h config.mk\
		dwm.1 drw.h util.h ${SRC} push.c present.h transient.c tests dwm-${VERSION}
	tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	gzip dwm-${VERSION}.tar
	rm -rf dwm-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm\
		${DESTDIR}${MANPREFIX}/man1/dwm.1

.PHONY: all options test test-x11 clean dist install uninstall
