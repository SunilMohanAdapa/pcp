#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif
ifeq ($(shell [ -d /usr/gnu/bin ] && echo 0),0)
PATH	= /usr/gun/bin:$(shell . $(PCP_DIR)/etc/pcp.env; echo $$PATH)
else
PATH	= $(shell . $(PCP_DIR)/etc/pcp.env; echo $$PATH)
endif
include $(PCP_INC_DIR)/builddefs

SUBDIRS = src-oss
ifeq ($(shell [ -d src ] && echo 0),0)
SUBDIRS += src
endif
SUBDIRS += pmdas debian

TESTS	= $(shell sed -n -e '/^[0-9]/s/[ 	].*//p' <group)

default: localconfig new remake check qa_hosts $(OTHERS) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

setup: $(SUBDIRS) pconf cisco
	$(SUBDIRS_MAKERULE)

LDIRT += 051.work 134.full.* \
         *.bak *.bad *.core *.full *.raw *.o core a.out core.* \
	 *.log eek* urk* so_locations tmp.* gmon.out oss.qa.tar.gz \
	 *.full.ok *.new rc_cron_check.clean \
	  make.out qa_hosts localconfig localconfig.h check.time
	# these ones are links to the real files created when the associated
	# test is run
LDIRT += $(shell [ -f .gitignore ] && grep '\.out$$' .gitignore)
	# from QA 441
LDIRT += big1.*

# 051 depends on this rule being here
051.work/die.001: 051.setup
	chmod u+x 051.setup
	./051.setup

qa_hosts:	qa_hosts.master mk.qa_hosts
	PATH=$(PATH); ./mk.qa_hosts

localconfig:
	PATH=$(PATH); ./mk.localconfig

pconf:	pconf.tar
	tar xf pconf.tar
	touch pconf

cisco:	cisco.tar
	tar xf cisco.tar
	touch cisco

SCRIPTS = mk.localconfig new check recheck remake mk.qa_hosts findmetric \
	  fix-rc getpmcdhosts group-stats mk.variant changeversion \
	  check-gitignore check-group chk.setup sanity.coverage show-me \
	  xlate_2_new_pmns Makepkgs

COMMON = common common.check common.config common.filter common.install.cisco \
	 common.pcpweb common.product common.rc common.setup

OTHERS = group qa_hosts.master COPYING README pconf.tar \
	 cisco.tar VERSION.pcpqa valgrind-suppress

DOTOUTFILES = $(shell git ls-tree --name-only HEAD | grep '^[0-9]' | grep -v '^[0-9][0-9][0-9]$$' | grep -v '^[0-9][0-9][0-9][0-9]$$')

LSRCFILES = $(SCRIPTS) $(COMMON) $(TESTS) $(DOTOUTFILES) $(OTHERS)

install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/qa
	$(INSTALL) -m 755 $(SCRIPTS) $(PCP_VAR_DIR)/qa
	$(INSTALL) -m 644 GNUmakefile $(COMMON) $(OTHERS) $(PCP_VAR_DIR)/qa
	$(INSTALL) -m 755 $(TESTS) $(PCP_VAR_DIR)/qa
	$(INSTALL) -m 644 $(DOTOUTFILES) $(PCP_VAR_DIR)/qa
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
