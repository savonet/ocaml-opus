all:
	$(MAKE) -f Makefile.opus2wav $@

test: all
	if [ ! -e test.opus ] ; then ffmpeg -f lavfi -i "sine=frequency=440:duration=5" -ac 2 test.opus ; fi
	OCAMLRUNPARAM=b ./opus2wav test.opus test.wav

clean:
	rm -rf autom4te.cache config.log config.status autom4te.cache
	$(MAKE) -f Makefile.opus2wav clean

distclean: clean
	$(MAKE) -f Makefile.opus2wav distclean
	rm -f Makefile.opus2wav
