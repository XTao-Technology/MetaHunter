
rpm_dir=rpmbuild
MH_TAR=metahunter-$(VERSION).tar.gz
build_opt=

if ROBINHOOD
    build_opt+= --with robinhood	
endif
if IRODS
    build_opt+= --with irods
endif


rpms: prep rpm

prep:
	$(MAKE) -C ../ clean;
	$(MAKE) -C ../ dist;
	-mkdir -p $(rpm_dir)/BUILD
	-mkdir -p $(rpm_dir)/SPECS
	-mkdir -p $(rpm_dir)/RPMS
	-mkdir -p $(rpm_dir)/SRPMS
	-mkdir -p $(rpm_dir)/SOURCES
	-rm -rf $(rpm_dir)/SOURCES/*

	cp ../*.tar.gz ./rpmbuild/SOURCES/
	cp ../metahunter.spec $(rpm_dir)/SPECS

rpm:
	echo $build_opt
	echo "%_topdir `pwd`/rpmbuild" >> /`whoami`/.rpmmacros
	echo "%_libdir /usr/lib" >> /`whoami`/.rpmmacros
	rpmbuild $(build_opt) --define "rbhsrc $(RBH)" -bb rpmbuild/SPECS/metahunter.spec
	mv rpmbuild/RPMS/* ./

clean-local:
	rm -rf rpmbuild
	rm -rf *.rpm
