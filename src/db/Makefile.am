SUBDIRS=

if ROBINHOOD
SUBDIRS+=robinhood
endif

indent:
	for d in $(SUBDIRS); do 	\
		make -C $$d indent ; 	\
	done
