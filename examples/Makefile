# OCaml-Mad examples.
#
# Copyright 2003 by the Savonet team.
#
# $Id$

all clean:
	$(MAKE) -f Makefile.opus2wav $@
	OCAMLRUNPARAM=b ./opus2wav test.opus test.wav

distclean: clean
	rm -rf autom4te.cache config.log config.status autom4te.cache
	rm -f Makefile.opus2wav
