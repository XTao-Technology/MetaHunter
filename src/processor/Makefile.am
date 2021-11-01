SUBDIRS= standard scanner

if IRODS
SUBDIRS+=irods
endif

indent:
	for d in $(SUBDIRS); do 	\
		make -C $$d indent ;
