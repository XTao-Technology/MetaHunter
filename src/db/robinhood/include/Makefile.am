SUFFIXES = .def

rbhsrctop = $(RBH)/src

TYPEGEN=$(RBH)/scripts/type_gen.pl

.def.h:
	$(TYPEGEN) $< $*.h

nodist_noinst_HEADERS=$(rbhsrctop)/include/tmp_fs_mgr_types.def

all: tmp_fs_mgr_types.h

tmp_fs_mgr_types.h:
	$(TYPEGEN) $(rbhsrctop)/include/tmp_fs_mgr_types.def tmp_fs_mgr_types.h

CLEANFILES = tmp_fs_mgr_types.h

indent:
	$(top_srcdir)/scripts/indent.sh
