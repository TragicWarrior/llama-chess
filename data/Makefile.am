EXTRA_DIST = config.example nag.data agony.data cc.data

install-data-local:
	mkdir -m 0755 -p ${datadir}/cboard || exit 1

	for file in ${EXTRA_DIST}; do \
	    test -f $$file && cp -f $$file ${datadir}/cboard; \
	    chmod 0644 ${datadir}/cboard/$$file; \
	done
