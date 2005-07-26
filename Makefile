SUBDIRS= \
	darwinbuild \
	darwinxref \
	darwintrace

.PHONY: all clean install uninstall

all clean install uninstall:
	@$(foreach DIR,$(SUBDIRS),make -C $(DIR) $@ ;)
