include ../../Makefile.include TAGLIB-VERSION ../../download-files.include
DEPS = ../../Makefile.include Makefile TAGLIB-VERSION ../../download-files.include \
                                       001-cmake-pdb-debug.patch

LIBDYLIB=$(PLATFORM)/build/$(LIBNAME)/$(BYPRODUCT)
all: .installed-$(PLATFORM)

$(PLATFORM): $(DEPS) | $(TARBALLS_LOCATION)/$(ARCHIVE).$(HASH_TYPE)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); rm -rf build; mkdir -p build
	cd $(PLATFORM); patch -p1 -i ../001-cmake-pdb-debug.patch
	if(CPU STREQUAL armeabi-v7a)
		CMAKE_FLAGS := -DCMAKE_CXX_FLAGS="-D_FILE_OFFSET_BITS=64"
	else
		CMAKE_FLAGS :=
	endif
	cd $(PLATFORM)/build; $(CMAKE) $(CMAKE_FLAGS) -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_BINDINGS=OFF -DCMAKE_BUILD_TYPE=Debug ..

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)/build
	touch $@

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM)/build install
	touch $@

clean:
	$(MAKE) -C $(PLATFORM)/build clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
