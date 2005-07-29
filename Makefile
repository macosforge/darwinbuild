SUBDIRS= \
	darwinbuild \
	darwinxref \
	darwinroot \
	darwintrace

.PHONY: all clean install uninstall

all clean install uninstall:
	@$(foreach DIR,$(SUBDIRS), \
		echo "*** Making $@ in $(DIR) ***" ; \
		make -C $(DIR) $@ ;)
