EXTRA_DIST = config.example
dist_man6_MANS = cboard.man

dist-hook:
	for m in $(dist_man6_MANS); do \
	    sed -e 's/^\.Dd.*/\.Dd '"`date '+%B %d, %Y'`"'/' $(distdir)/$$m \
	        >$(distdir)/$$m.tmp \
		&& mv -f $(distdir)/$$m.tmp $(distdir)/$$m; \
	done;
