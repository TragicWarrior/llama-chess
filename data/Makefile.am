EXTRA_DIST = config.example nag.data agony.data cc.data

install-data-hook:
	$(INSTALL) -d -m 0755 $(datadir)/cboard

	for file in ${EXTRA_DIST}; do \
	    $(INSTALL) -m 0644 $(srcdir)/$$file $(datadir)/cboard; \
	done

uninstall:
	rm -rf $(datadir)/cboard
