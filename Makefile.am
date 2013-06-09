EXTRA_DIST = KnownBugs ChangeLog debian osdep
SUBDIRS = libchess src doc data
ACLOCAL_AMFLAGS = -I m4

changelog:
	git log --no-color > ChangeLog || exit 1

deb: changelog
	dpkg-buildpackage -B -uc -rfakeroot && dh_clean

version: changelog
	$(MAKE) $(AM_MAKEFLAGS) distcheck
	$(MAKE) $(AM_MAKEFLAGS) deb

distcheck-hook:	changelog
