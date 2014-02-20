# Copyright 2013 Henrik Mühe and Florian Funke

# This file is part of CampersCoreBurner.

# CampersCoreBurner is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# CampersCoreBurner is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.

# You should have received a copy of the GNU Affero General Public License
# along with CampersCoreBurner.  If not, see <http://www.gnu.org/licenses/>.


SRC_O=$(addprefix bin/,$(filter-out %tester.o,$(filter-out %driver.o,$(filter-out %main.o,$(patsubst %.cpp,%.o,$(wildcard src/**/*.cpp src/*.cpp))))))
LIB_O=libs/tbb/tbb41_20130116oss/build/*_release/*.o
TEST_O=$(addprefix bin/,$(patsubst %.cpp,%.o,$(wildcard tests/*.cpp)))

TBB_URL="http://threadingbuildingblocks.org/sites/default/files/software_releases/source/tbb41_20130116oss_src.tgz"
TCMALLOC_URL="https://gperftools.googlecode.com/files/gperftools-2.0.tar.gz"
LOCKLESSMALLOC_URL="http://locklessinc.com/downloads/lockless_allocator_src.tgz"

-include config.local

SHELL := /bin/bash
CC  ?= gcc
CXX ?= g++
OPT ?= -O3
REMOTEHOST?=deskkemper04.informatik.tu-muenchen.de
DRIVER ?= driver
SSEFLAGS ?= -msse4.2
CFLAGS=-std=c++11 $(OPT) $(SSEFLAGS) -fPIC -Wall -g -I. -I./include -Ilibs/gtest/include -Ilibs -fno-omit-frame-pointer -Ilibs/tbb/include
CXXFLAGS=$(CFLAGS)
LDFLAGS=-lpthread -Lbin
ifneq ($(shell uname),Darwin)
LDFLAGS:=$(LDFLAGS) -Wl,-rpath,'$$ORIGIN/bin' -L/usr/lib -ldl -Wl,--build-id
endif
LIB_LDFLAGS:=
ifneq ($(shell uname),Darwin)
ifeq ($(shell echo $$LEADERBOARD),1)
LIB_O:=$(LIB_O) libs/lockless_allocator/libllalloc.a
#LIB_O:=$(LIB_O) libs/tcmalloc/lib/libtcmalloc_minimal.a
else
LIB_O:=$(LIB_O) libs/lockless_allocator/libllalloc.a
endif
LIB_LDFLAGS:=$(LIB_LDFLAGS) -Wl,--no-as-needed -lrt
else
LIB_O:=$(LIB_O) libs/tcmalloc/lib/libtcmalloc_minimal.a
endif

ifeq ($(shell echo $$LEADERBOARD),1)
CXXFLAGS:=$(CXXFLAGS) -D LEADERBOARD
endif

PROGRAMS=driver tester
LIBRARY=core
DEPTARGETS=tbb


all: deps
	$(MAKE) $(PROGRAMS)

CHECKDIR=@mkdir -p $(dir $@)
DEPTRACKING=-MD -MF $(@:.o=.d)
define DEPFIXUP
	@mv -f $(@:.o=.d) $(@:.o=.d).tmp
	@sed -e 's|.*:|$@:|' < $(@:.o=.d).tmp > $(@:.o=.d)
	@sed -e 's/.*://' -e 's/\\$$//' < $(@:.o=.d).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(@:.o=.d)
	@rm -f $(@:.o=.d).tmp
endef

-include bin/*.d bin/**/*.d bin/**/**/*.d $(addprefix bin/,$(SRC_O:.o=.d))

bin/%.o: %.cpp
	$(CHECKDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $(DEPTRACKING) $<
	$(DEPFIXUP)

bin/%.o: %.c
	$(CHECKDIR)
	$(CC) $(filter-out -std=c++11,$(CXXFLAGS)) -c -o $@ $(DEPTRACKING) $<
	$(DEPFIXUP)

deps:
	@mkdir -p bin
	@touch bin/.compiler; \
	if [ "`cat bin/.compiler`" != "$(CXX)" ]; then \
	echo "$(CXX)" > bin/.compiler; \
	$(MAKE) clean; \
	$(MAKE) $(DEPTARGETS); \
	fi ;\

bin/libcore.so: $(SRC_O) $(LIB_O)
	$(CXX) $(CXXFLAGS) -shared -o bin/lib$(LIBRARY).so $(filter %.o,$^) $(filter %.a,$^) $(LDFLAGS) $(LIB_LDFLAGS)

leaderboard:
	$(MAKE) clean
	LEADERBOARD=1 $(MAKE)
	rm bin/libcore.so
	LEADERBOARD=1 $(MAKE) bin/libcore.so
	strip bin/libcore.so
	cp bin/libcore.so .

bin/gtest-all.o:
	$(CHECKDIR)
	$(CXX) -Ilibs/gtest/include -Ilibs/gtest -c libs/gtest/src/gtest-all.cc -o $@
	ar -rv bin/libgtest.a bin/gtest-all.o

driver: bin/libcore.so bin/src/driver.o
	$(CXX) $(CXXFLAGS) -o driver $(filter %.o,$^) $(LDFLAGS) -lcore

tester: bin/libcore.so bin/src/tester.o bin/gtest-all.o $(TEST_O)
	$(CXX) $(CXXFLAGS) -o tester $(filter %.o,$^) $(LDFLAGS) -lcore

.force:

tbb:
	rm -rf libs/tbb
	mkdir libs/tbb
	cd libs/tbb ; wget -O archive_tbb41.tar.gz $(TBB_URL)
	cd libs/tbb ; tar zxf archive_tbb41*
	cd libs/tbb ; perl -p -e "s/-O2/-O3/g" -i tbb41*/build/linux.gcc.inc
	if grep -q "clang" bin/.compiler; then \
	cd libs/tbb/tbb41* && make -j4 tbb compiler=clang CXXFLAGS="-stdlib=libc++ -std=c++11" LDFLAGS="-stdlib=libc++"; \
	else \
	cd libs/tbb/tbb41* && make -j 4 tbb; \
	fi
	rm -f libs/tbb/tbb41_20130116oss/build/linux_intel64_gcc_cc4.7_libc2.15_kernel3.5.0_release/itt_notify_malloc*
	find libs/tbb -name 'libtbb*.*' -exec mv {} libs/tbb \;
	ln -s `pwd`/libs/tbb/tbb41*/include/ libs/tbb/include

libs/tcmalloc/lib/libtcmalloc_minimal.a:
	$(CHECKDIR)
	rm -rf libs/tcmalloc
	rm -rf libs/gperf*
	cd libs ; wget -O archive_gperf.tar.gz $(TCMALLOC_URL)
	cd libs ; tar zxf archive_gperf*
	cd libs/gperf* ; ./configure --prefix=`pwd`/../tcmalloc --with-pic --enable-minimal
	cd libs/gperf* ; make -j 4
	cd libs/gperf* ; make install
	rm -rf libs/gperf*

llalloc libs/lockless_allocator/libllalloc.a:
	rm -rf libs/lockless*
	cd libs/ ; wget -O archive_lockless.tar.gz $(LOCKLESSMALLOC_URL)
	cd libs/ ; tar zxf archive_lockless*
	cd libs/lockless* ; make

speed:
	@for i in `seq 1 25`; do ./$(DRIVER) ; done | grep Time | sed 's/^.*=//g' | sed 's/\[.*$$//g' | sort -n | awk 'BEGIN{c=0;sum=0;} /^[^#]/{a[c++]=$$1;sum+=$$1;} END{ave=sum/c; if((c%2)==1){median=a[int(c/2)];} else{median=(a[c/2]+a[c/2-1])/2;} print "MIN: ",a[0],"MAX: ",a[c-1],"AVG: ",int(ave),"MEAN: ",int(median) }'

longspeed: bin/long
	@for i in `seq 1 10`; do ./$(DRIVER) bin/long ; done | grep Time | cut -d '=' -f 2 | cut -d '[' -f 1 | sort -n | awk 'BEGIN{c=0;sum=0;} /^[^#]/{a[c++]=$$1;sum+=$$1;} END{ave=sum/c; if((c%2)==1){median=a[int(c/2)];} else{median=(a[c/2]+a[c/2-1])/2;} print "MIN: ",a[0],"MAX: ",a[c-1],"AVG: ",int(ave),"MEAN: ",int(median) }'

bigspeed: bin/big_test.txt
	@for i in `seq 1 10`; do ./$(DRIVER) bin/big_test.txt ; done | grep Time | cut -d '=' -f 2 | cut -d '[' -f 1 | sort -n | awk 'BEGIN{c=0;sum=0;} /^[^#]/{a[c++]=$$1;sum+=$$1;} END{ave=sum/c; if((c%2)==1){median=a[int(c/2)];} else{median=(a[c/2]+a[c/2-1])/2;} print "MIN: ",a[0],"MAX: ",a[c-1],"AVG: ",int(ave),"MEAN: ",int(median) }'

interspeed: test_data/inter_test.txt
	@for i in `seq 1 10`; do ./$(DRIVER) test_data/inter_test.txt ; done | grep Time | cut -d '=' -f 2 | cut -d '[' -f 1 | sort -n | awk 'BEGIN{c=0;sum=0;} /^[^#]/{a[c++]=$$1;sum+=$$1;} END{ave=sum/c; if((c%2)==1){median=a[int(c/2)];} else{median=(a[c/2]+a[c/2-1])/2;} print "MIN: ",a[0],"MAX: ",a[c-1],"AVG: ",int(ave),"MEAN: ",int(median) }'

interspeed20: test_data/inter_test.txt
	@for i in `seq 1 20`; do ./$(DRIVER) test_data/inter_test.txt ; done | grep Time | cut -d '=' -f 2 | cut -d '[' -f 1 | sort -n | awk 'BEGIN{c=0;sum=0;} /^[^#]/{a[c++]=$$1;sum+=$$1;} END{ave=sum/c; if((c%2)==1){median=a[int(c/2)];} else{median=(a[c/2]+a[c/2-1])/2;} print "MIN: ",a[0],"MAX: ",a[c-1],"AVG: ",int(ave),"MEAN: ",int(median) }'

interspeed50: test_data/inter_test.txt
	@for i in `seq 1 50`; do ./$(DRIVER) test_data/inter_test.txt ; done | grep Time | cut -d '=' -f 2 | cut -d '[' -f 1 | sort -n | awk 'BEGIN{c=0;sum=0;} /^[^#]/{a[c++]=$$1;sum+=$$1;} END{ave=sum/c; if((c%2)==1){median=a[int(c/2)];} else{median=(a[c/2]+a[c/2-1])/2;} print "MIN: ",a[0],"MAX: ",a[c-1],"AVG: ",int(ave),"MEAN: ",int(median) }'

clean:
	find . -name '._*' -exec rm {} \;
	find . -name '*conflicted*' -exec rm {} \;
	find . -name '*.orig' -exec rm {} \;
	find . -name '*.rej' -exec rm {} \;
	rm -f perf.data*
	rm -f $(PROGRAMS)
	rm -f testgen resultgen
	find bin -name '*.so' -exec rm -f {} \;
	find bin -name '*.o' -exec rm -f {} \;
	find bin -name '*.a' -exec rm -f {} \;
	find . -name '*.unison.tmp' -exec rm -f {} \;
	find . -name '*.dSYM' -exec rm -rf {} \;
	rm -rf ./r00*
	rm -f impl.tar.gz
	rm -f libcore.so

remote:
	fswatch . "unison . ssh://$(REMOTEHOST)/zoig -prefer . -ignore 'Name .git' -ignore 'Name *.o' -ignore 'Name *.so' -ignore 'Name driver'  -ignore 'Name tester' -ignore 'Name testgen'  -ignore 'Name resultgen' -ignore 'Name libs' -ignore 'Name nfa' -ignore 'Name *.a' -batch -ignore 'Name bin' -ignore 'Name perf.*' -ignore 'Name files' -ignore 'Name .DS_Store' -ignore 'Name ._*' -ignore 'Name fuzzytester'"

stats:
	@cloc --exclude-dir=bin --exclude-dir=libs --exclude-lang=D .
	@cat test_data/small_test.txt | grep "^s " | cut -d ' ' -f 6- | tr ' ' '\n' | sort | echo "Words: "`wc -l`
	@cat test_data/small_test.txt | grep "^s " | cut -d ' ' -f 6- | tr ' ' '\n' | sort | uniq | echo "Unique Words: "`wc -l`
	@cat test_data/small_test.txt | grep -E "^s [0-9]+ 2" | cut -d ' ' -f 6- | tr ' ' '\n' | sort | uniq | echo "Unique Words in ed: "`wc -l`
	@cat test_data/small_test.txt | grep -E "^s [0-9]+ 2" | cut -d ' ' -f 6- | tr ' ' '\n' | sort | uniq
	@cat test_data/small_test.txt | grep -E "^s [0-9]+ 2" | cut -d ' ' -f 4 | sort | uniq -c

testgen: utils/testgen.cpp include/random.hpp include/metrics.hpp
	$(CXX) $(CXXFLAGS) utils/testgen.cpp -o testgen

fuzzytester: utils/fuzzytester.cpp include/core.h include/metrics.hpp bin/libcore.so
	$(CXX) $(CXXFLAGS) utils/fuzzytester.cpp -o fuzzytester $(LDFLAGS) -lcore

resultgen: utils/resultgen.cpp bin/libcore.so
	$(CXX) $(CXXFLAGS) utils/resultgen.cpp -o resultgen $(LDFLAGS) -lcore

bin/big_test.txt:
	mkdir -p bin
	cd bin ; wget http://www-db.in.tum.de/~muehe/big_test.zip
	cd bin ; unzip big_test.zip

files:
	mkdir -p files
	cd files ; if [ ! -f archive_tbb41.tar.gz ]; then wget -O archive_tbb41.tar.gz $(TBB_URL) ; fi
	cd files ; if [ ! -f archive_lockless.tar.gz ]; then wget -O archive_lockless.tar.gz $(LOCKLESSMALLOC_URL) ; fi

submission: files
	rm -rf bin/test_impl
	mkdir -p bin/test_impl
	# libs
	mkdir -p bin/test_impl/libs
	cd bin/test_impl/libs ; cp ../../../files/archive_tbb41.tar.gz .
	cd bin/test_impl/libs ; tar zxf archive_tbb41*
	cd bin/test_impl/libs/tbb*; bzcat ../../../../patches/tbb.patch.bz2 | patch -s -p1
	cd bin/test_impl/libs/tbb*; cat ../../../../patches/tbb2.patch | patch -s -p1
	cd bin/test_impl/libs/tbb*; cat ../../../../patches/tbb3.patch | patch -s -p1
	cd bin/test_impl/libs/ ; cp ../../../files/archive_lockless.tar.gz .
	cd bin/test_impl/libs/ ; tar zxf archive_lockless*
	cd bin/test_impl/libs/lockless* ; bzcat ../../../../patches/llalloc.patch.bz2 | patch -s -p1
	cd bin/test_impl/libs ; rm -f *.tar.gz *.tar.bz2
	# src
	mkdir -p bin/test_impl/src ; \
	mkdir -p bin/test_impl/include ; \
	cp -rf src/* bin/test_impl/src ; \
	rm -f bin/test_impl/src/tester.cpp bin/test_impl/src/driver.cpp ; \
	cp -rf include/* bin/test_impl/include ; \
	cd bin/test_impl ; \
	SRC=`find . -type f -name '*.cpp' -or -name '*.c'` ; \
	HEAD=`find . -type f -name '*.hpp' -or -name '*.h'` ; \
	DIRS=`find . -type d | sed 's#^#-I#g' | xargs` ; \
	echo -e "-march=native\n" > flags ; \
	cp ../../COPYING . ; \
	cp ../../README . ; \
	perl -p -e "s%^#.*/\*\* LBREMOVE \*\*/.*\n$$%%g" -i src/core.cpp ; \
	echo g++-4.7 `cat flags` -O3 -fPIC -Wall -std=c++11 -fopenmp -g $$DIRS -shared -pthread -lpthread -o libcore.so $$SRC
	# g++-4.7 `cat flags` -O3 -fPIC -Wall -std=c++11 -fopenmp -g $$DIRS -shared -pthread -lpthread -o libcore.so $$SRC
	# package
	cd bin/test_impl ; tar cfz ../../impl.tar.gz *


.PRECIOUS: bin/%.o
.PHONY: tbb llalloc submission files leaderboard
