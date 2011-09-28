EXTRA_DIST = KnownBugs ChangeLog debian osdep
SUBDIRS = libchess src doc data
ACLOCAL_AMFLAGS = -I m4

changelog:
	git-log --no-color > ChangeLog || exit 1

version: changelog
	$(MAKE) $(AM_MAKEFLAGS) distcheck
