
SUFFIXES = .def

TYPEGEN=$(top_srcdir)/scripts/type_gen.pl

.def.h:
	$(TYPEGEN) $< $*.h

noinst_HEADERS=xlist.h mem.h logging.h locking.h rb.h rbthash.h hashfn.h \
	filesystem.h database.h processor.h cfg-parser.h cJSON.h common.h \
	defaults.h hunter.h mattr.h thread-pool.h


#CLEANFILES = 

indent:
	$(top_srcdir)/scripts/indent.sh
