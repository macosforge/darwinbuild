
all: darwinbuild-all \
	darwinxref-all \
	darwintrace-all

darwinbuild-all:
	@make -C darwinbuild all

darwinxref-all:
	@make -C darwinxref all

darwintrace-all:
	@make -C darwintrace all

install: darwinbuild-install \
	darwinxref-install \
	darwintrace-install

darwinbuild-install:
	@make -C darwinbuild install

darwinxref-install:
	@make -C darwinxref install

darwintrace-install:
	@make -C darwintrace install

uninstall: darwinbuild-uninstall \
	darwinxref-uninstall \
	darwintrace-uninstall

darwinbuild-uninstall:
	@make -C darwinbuild uninstall

darwinxref-uninstall:
	@make -C darwinxref uninstall

darwintrace-uninstall:
	@make -C darwintrace uninstall

