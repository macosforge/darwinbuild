
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
