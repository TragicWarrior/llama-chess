EXTRA_DIST = KnownBugs ChangeLog debian osdep
SUBDIRS = libchess src doc data test

changelog:
	git-log --no-color > ChangeLog || exit 1

version: changelog
	$(MAKE) $(AM_MAKEFLAGS) distcheck
