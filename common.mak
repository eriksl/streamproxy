CWARNINGS		= -Wall -Wextra -Wshadow -Wundef -Wformat=2 -Winit-self -Wunused -Werror -Wpointer-arith -Wcast-qual -Wmultichar
CPPWARNINGS		= $(CWARNINGS)

CFLAGS			+= $(CWARNINGS)
CPPFLAGS		+= $(CPPWARNINGS)

ifneq ($(DEBUG), on)
CFLAGS			+= -O2
LDFLAGS			+= -s
else
CFLAGS			+= -O0 -g
LDFLAGS			+= -g
endif

ifeq ($(TARGET), x86_64)
	CC			= gcc
	CPP			= g++
	CFLAGS		+= -DTARGET=x86_64 -DTARGET_X86_64=1
endif

ifeq ($(TARGET), i386)
	CC			= gcc
	CPP			= g++
	CFLAGS		+= -m32 -DTARGET=i386 -DTARGET_I386=1
	LDFLAGS		+= -m32
endif

ifeq ($(TARGET), mipsel21)
	TOOLPATH	=	/nfs/src/openpli-2.1/build-dm8000/tmp/sysroots/x86_64-linux/usr/mipsel/bin
	CC			=	$(TOOLPATH)/mipsel-oe-linux-gcc
	CPP			=	$(TOOLPATH)/mipsel-oe-linux-g++
	CFLAGS		+= -DTARGET=mipsel21 -DTARGET_MIPSEL21=1
	CFLAGS		+= -I/usr/src/libmicrohttpd/mipsel21/usr/include
	LDFLAGS		+= -L/usr/src/libmicrohttpd/mipsel21/usr/lib
endif

ifeq ($(TARGET), mipsel)
	TOOLPATH	=	/nfs/src/openpli/build/tmp/sysroots/x86_64-linux/usr/bin/mips32el-oe-linux
	CC			=	$(TOOLPATH)/mipsel-oe-linux-gcc
	CPP			=	$(TOOLPATH)/mipsel-oe-linux-g++
	CFLAGS		+= -DTARGET=mipsel -DTARGET_MIPSEL=1
	CFLAGS		+= -I/usr/src/libmicrohttpd/mipsel/usr/include
	LDFLAGS		+= -L/usr/src/libmicrohttpd/mipsel/usr/lib
endif

.PHONY:			all depend clean pristine install rpm dpkg

DATE			=	`date '+%Y%m%d%H%M'`
VERSION			=	daily-$(DATE)

RPM				=	`pwd`/rpm
RPMTARBALL		=	$(RPM)/$(PROGRAM).tar
RPMPKGDIR		=	$(RPM)/$(TARGET)
RPMDEBUGDIR		=	$(RPMPKGDIR)/debug
RPMBUILDDIR		=	$(RPM)/build
RPMTMP			=	$(RPM)/tmp
RPMSPECIN		=	$(RPM)/$(PROGRAM).spec
RPMSPECOUTDIR	=	$(RPMBUILDDIR)/rpm
RPMSPECOUT		=	$(RPMSPECOUTDIR)/$(PROGRAM).spec

DPKG			=	`pwd`/dpkg
DPKGTARBALL 	=	$(DPKG)/$(PROGRAM).tar
DPKGDEBIANDIR	=	$(DPKG)/DEBIAN
DPKGCONFIGURE	=	$(DPKG)/configure.dpkg
DPKGPKGDIR		=	$(DPKG)/$(TARGET)
DPKGBUILDDIR	=	$(DPKG)/build
DPKGDESTDIR		=	$(DPKGBUILDDIR)/dpkg/root
DPKGDEBIANDESTD	=	$(DPKGDESTDIR)/DEBIAN
DPKGCHANGELOG	=	$(DPKGDEBIANDESTD)/changelog
DPKGCONTROL		=	$(DPKGDEBIANDESTD)/control

DEPS			=	$(patsubst %.o,.%.d, $(OBJS))

all:		depend $(PROGRAM)

ifeq ($(DEPS), "")
else
-include .deps
endif

depend:		.deps

.deps:		$(DEPS)
			@echo "DEPEND ALL"
			@cat $^ /dev/null > $@

.%.d:		%.c
			@echo "DEPEND c $<"
			@$(CPP) $(CFLAGS) $(CCFLAGS) -M $^ -o $@

.%.d:		%.cpp
			@echo "DEPEND c++ $<"
			@$(CPP) $(CFLAGS) $(CPPFLAGS) -M $^ -o $@

%.o:		%.c
			@echo "CC $< -> $@"
			@$(CPP) $(CFLAGS) $(CCFLAGS) -c $< -o $@

%.o:		%.cpp
			@echo "CPP $< -> $@"
			@$(CPP) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(PROGRAM):	$(OBJS)
			@echo "LD $@"
			@$(CPP) $(LDFLAGS) $^ $(LDLIBS) -o $@ 

install:	$(PROGRAM)
			@echo "INSTALL $(PROGRAM) -> $(DESTDIR)/bin"
			@mkdir -p $(DESTDIR)/bin
			@cp $(PROGRAM) $(DESTDIR)/bin
			@-chown root:root $(DESTDIR)/bin/$(PROGRAM)
			@-chmod 755 $(DESTDIR)/bin/$(PROGRAM)

clean:
			@echo "CLEAN"
			rm -f $(PROGRAM) $(OBJS) $(DEPS) .deps 2> /dev/null || true

pristine:	
			@echo "PRISTINE"
			git clean -f -d -q

rpm:
#
			@echo "PREPARE $(VERSION)"
			@-rm -rf $(RPMPKGDIR) $(RPMBUILDDIR) $(RPMTMP) 2> /dev/null || true
			@mkdir -p $(RPMPKGDIR) $(RPMBUILDDIR) $(RPMTMP) $(RPMSPECOUTDIR)
			@sed $(RPMSPECIN) -e "s/%define dateversion.*/%define dateversion $(DATE)/" > $(RPMSPECOUT)
#
			@echo "TAR $(RPMTARBALL)"
			@-rm -f $(RPMTARBALL) 2> /dev/null
			tar cf $(RPMTARBALL) -C .. --exclude rpm --exclude .git $(PROGRAM)
#
			@echo "CREATE RPM $(RPMSPECOUT)"
			@rpmbuild -bb $(RPMSPECOUT)
#
			@-rm -rf $(RPMDEBUGDIR) 2> /dev/null || true
			@-mkdir -p $(RPMDEBUGDIR) 2> /dev/null || true
			@-mv rpm/$(TARGET)/$(PROGRAM)-debuginfo-*.rpm rpm/$(TARGET)/debug 2> /dev/null || true
			@-rm -rf $(RPMBUILDDIR) $(RPMTMP)

dpkg:
#
			@echo "PREPARE $(VERSION)"
			@-rm -f $(DPKGPKGDIR) $(DPKGBUILDDIR) $(DPKGDESTDIR) 2> /dev/null || true
			@mkdir -p $(DPKGPKGDIR) $(DPKGBUILDDIR)
#
			@echo "TAR $(DPKGTARBALL)"
			@-rm -f $(DPKGTARBALL) 2> /dev/null
			@git archive -o $(DPKGTARBALL) HEAD
			@tar xf $(DPKGTARBALL) -C $(DPKGBUILDDIR)
#
			@echo "DUP $(DPKGDEBIANDIR)"
			@mkdir -p $(DPKGDEBIANDESTD)
			@cp $(DPKGDEBIANDIR)/* $(DPKGDEBIANDESTD)
#
			@echo "CHANGELOG $(DPKGCHANGELOG)"
			@-rm -f $(DPKGCHANGELOG) 2> /dev/null
			@echo "$(PROGRAM) ($(VERSION)) stable; urgency=low" > $(DPKGCHANGELOG)
			@echo >> $(DPKGCHANGELOG)
			@git log | head -100 >> $(DPKGCHANGELOG)
			@sed --in-place $(DPKGCONTROL) -e "s/^Version:.*/Version: `date "+%Y%m%d"`/"
			@sed --in-place $(DPKGCONTROL) -e "s/^Architecture:.*/Architecture: $(TARGET)/"
#
			@echo "CONFIGURE $(DPKGCONFIGURE)"
			@(cd $(DPKGBUILDDIR); $(DPKGCONFIGURE))
#
			@echo "BUILD $(DPKGBUILDDIR)"
			@$(MAKE) -C $(DPKGBUILDDIR) all
#
			@echo "CREATE DEB"
			@fakeroot /bin/sh -c "\
				$(MAKE) -C $(DPKGBUILDDIR) DESTDIR=$(DPKGDESTDIR) install; \
				dpkg --build $(DPKGDESTDIR) $(DPKGPKGDIR)"

rpminstall:
			@echo "INSTALL"
			@sudo rpm -Uvh --force rpm/$(TARGET)/*.rpm
