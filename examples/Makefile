all:
	$(MAKE) -f Makefile.opus2wav $@

test:
	OCAMLRUNPARAM=b ./opus2wav test.opus test.wav

clean:
	rm -rf autom4te.cache config.log config.status autom4te.cache
	$(MAKE) -f Makefile.opus2wav clean

distclean: clean
	$(MAKE) -f Makefile.opus2wav distclean
	rm -f Makefile.opus2wav
