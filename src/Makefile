#Copyright (C) 2020 D. Michael Agun
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

.PHONY:all debug
all: concat concat-debug
#all: test-val test-stack
debug: concat-debug

HEADER_FILES=vm.h val.h helpers.h valhash.h parser.h vmstate.h ops.h $(wildcard ops_*.h) $(wildcard val_*.h) $(wildcard vm_*.h)
SOURCE_FILES=vm.c val.c helpers.c valhash.c parser.c vmstate.c ops.c $(wildcard ops_*.c) $(wildcard val_*.c) $(wildcard vm_*.c)

LIBFLAGS=-lm -lpthread
CFLAGS=
RELEASEFLAGS=
#RELEASEFLAGS += -O3
#RELEASEFLAGS += -Os
DEBUGFLAGS=-g
DEBUGFLAGS += -DINTERRUPT_ON_FATAL
DEBUGFLAGS += -DINTERRUPT_ON_THROW
DEBUGFLAGS += -DDEBUG_CHECKS
#DEBUGFLAGS += -DVM_DEBUG

concat: concat.c $(HEADER_FILES) $(SOURCE_FILES)
	gcc $(CFLAGS) $(RELEASEFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)

concat-debug: concat.c $(HEADER_FILES) $(SOURCE_FILES)
	gcc $(CFLAGS) $(DEBUGFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)

test-val: test-val.c $(HEADER_FILES) $(SOURCE_FILES)
	gcc $(CFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS) -g

test-stack: test-stack.c $(HEADER_FILES) $(SOURCE_FILES)
	gcc $(CFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS) -g

#TODO: improve the tests for more useful output
.PHONY: test test-debug
test: concat
	sh -c 'for test in ../tests/*.cat; do ./concat < $$test | diff -q $$test.out -; done; echo "All tests finished."'
test-debug: concat-debug
	sh -c 'for test in ../tests/*.cat; do ./concat-debug < $$test | diff -q $$test.out -; done; echo "All tests finished."'

.PHONY:clean
clean:
	rm -f concat concat-debug test-val test-stack
