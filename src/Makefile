.PHONY:all debug
all: concat concat-debug
#all: test-val test-stack
debug: concat-debug

HEADER_FILES=vm.h val.h helpers.h parser.h opcodes.h $(wildcard val_*.h) $(wildcard vm_*.h)
SOURCE_FILES=vm.c val.c helpers.c parser.c opcodes.c $(wildcard val_*.c) $(wildcard vm_*.c)

LIBFLAGS=-lm -lpthread
CFLAGS=
RELEASEFLAGS=
#RELEASEFLAGS += -O3
#RELEASEFLAGS += -Os
DEBUGFLAGS=-g
DEBUGFLAGS += -DINTERRUPT_ON_FATAL
DEBUGFLAGS += -DINTERRUPT_ON_THROW
DEBUGFLAGS += -DDEBUG_CHECKS
DEBUGFLAGS += -DVM_DEBUG_ERR
#DEBUGFLAGS += -DVM_DEBUG_STEP
#DEBUGFLAGS += -DSPRINTF_CHECK_RLEN
#DEBUGFLAGS += -DVM_DEBUG

concat: concat.c $(HEADER_FILES) $(SOURCE_FILES)
	gcc $(CFLAGS) $(RELEASEFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)

concat-debug: concat.c $(HEADER_FILES) $(SOURCE_FILES)
	gcc $(CFLAGS) $(DEBUGFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)

#TODO: friendlier test output
.PHONY: test test-debug
test: concat
	sh -c 'for test in ../tests/*.cat; do ./concat < $$test | diff -q $$test.out -; done; echo "All tests finished."'
test-debug: concat-debug
	sh -c 'for test in ../tests/*.cat; do ./concat-debug < $$test | diff -q $$test.out -; done; echo "All tests finished."'

.PHONY: test_vm test_val
test_vm: 
	gcc -o test_vm test_vm.c vm.c vm_*.c val.c val_*.c helpers.c parser.c $(LIBFLAGS) $(DEBUGFLAGS)

test_val: 
	gcc -o test_val test_val.c vm.c vm_*.c val.c val_*.c helpers.c parser.c $(LIBFLAGS) $(DEBUGFLAGS)

.PHONY:clean
clean:
	rm -f concat concat-debug test_vm test_val
