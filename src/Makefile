.PHONY:all debug
all: concat concat-debug
#all: test-val test-stack
debug: concat-debug

# to build concat and concat-debug:
#
# $ make
#
# to build release only:
#
# $ make concat
#
# to build concat with address sanitizer
#
# $ make concat-asan
#
# to run all tests scripts with address sanitizer
#
# $ make test-asan


HEADER_FILES=vm.h val.h helpers.h parser.h opcodes.h $(wildcard val_*.h) $(wildcard vm_*.h)
SOURCE_FILES=vm.c val.c helpers.c parser.c opcodes.c $(wildcard val_*.c) $(wildcard vm_*.c)

CC=gcc
LIBFLAGS=-lm -lpthread
CFLAGS=

RELEASEFLAGS=
RELEASEFLAGS += -DEBUG_FILENAME
#RELEASEFLAGS += -O3
#RELEASEFLAGS += -Os

DEBUGFLAGS=-g
DEBUGFLAGS += -DEBUG_FILENAME
DEBUGFLAGS += -DINTERRUPT_ON_FATAL
DEBUGFLAGS += -DINTERRUPT_ON_THROW
DEBUGFLAGS += -DDEBUG_VAL
DEBUGFLAGS += -DDEBUG_VAL_EVAL
DEBUGFLAGS += -DVM_DEBUG_ERR
DEBUGFLAGS += -DDEBUG_CHECKS
#DEBUGFLAGS += -DVM_DEBUG
#DEBUGFLAGS += -DVM_DEBUG_STEP
#DEBUGFLAGS += -DSPRINTF_CHECK_RLEN

#DEBUGFLAGS += -fsanitize=address -static-libasan

# not fully implemented
#DEBUGFLAGS += -DVAL_POINTER_CHECKS

# compile with address sanitizer (not compatible with gdb/valigrind use)
ASANFLAGS += -fsanitize=address -static-libasan

concat: concat.c $(HEADER_FILES) $(SOURCE_FILES)
	$(CC) $(CFLAGS) $(RELEASEFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)

concat-debug: concat.c $(HEADER_FILES) $(SOURCE_FILES)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)

concat-asan: concat.c $(HEADER_FILES) $(SOURCE_FILES)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(ASANFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)


#test - run all the *.cat files in the tests directory, and compare output to the corresponding .out files
#test-debug - do the same with the debug build
.PHONY: test test-debug test-asan
test: concat
	sh -c 'for test in ../tests/*.cat; do ./concat < $$test | diff -q $$test.out -; done; echo "All tests finished."'

test-debug: concat-debug
	sh -c 'for test in ../tests/*.cat; do ./concat-debug < $$test | diff -q $$test.out -; done; echo "All tests finished."'

test-asan: concat-asan
	sh -c 'for test in ../tests/*.cat; do ./concat-asan < $$test | diff -q $$test.out -; done; echo "All tests finished."'

#test_val: test_val.c $(HEADER_FILES) $(SOURCE_FILES)
#	$(CC) $(CFLAGS) $(RELEASEFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)

#test_val-debug: test_val.c $(HEADER_FILES) $(SOURCE_FILES)
#	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o $@ $< $(SOURCE_FILES) $(LIBFLAGS)

.PHONY:clean
clean:
	rm -f concat concat-debug concat-asan test_val test_val-debug
