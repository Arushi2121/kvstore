# Makefile — the build recipe book for kvstore.
#
# Targets:
#   make test       -> build & run ALL test binaries (smoke + store)
#   make smoke      -> build & run just the smoke test
#   make store      -> build & run just the store test
#   make clean      -> delete build output
#
# NOTE: command lines under each target MUST start with a real TAB, not spaces.

CC ?= clang
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -g -Isrc -Itests
ASAN = -fsanitize=address -fno-omit-frame-pointer
BUILD = build

test: smoke store heap clock
	@echo ""
	@echo "=== All test suites passed ==="

smoke: $(BUILD)/test_smoke
	@echo "--- smoke test (ASan ON) ---"
	./$(BUILD)/test_smoke

$(BUILD)/test_smoke: tests/test_smoke.c tests/test_harness.h | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) tests/test_smoke.c -o $@

store: $(BUILD)/test_store
	@echo "--- store test (ASan ON) ---"
	./$(BUILD)/test_store

$(BUILD)/test_store: $(BUILD)/store.o $(BUILD)/test_store.o | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) $(BUILD)/store.o $(BUILD)/test_store.o -o $@

$(BUILD)/store.o: src/store.c src/store.h | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) -c src/store.c -o $@

$(BUILD)/test_store.o: tests/test_store.c tests/test_harness.h tests/oracle.h src/store.h | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) -c tests/test_store.c -o $@

heap: $(BUILD)/test_heap
	@echo "--- heap test (ASan ON) ---"
	./$(BUILD)/test_heap

$(BUILD)/test_heap: $(BUILD)/heap.o $(BUILD)/test_heap.o | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) $(BUILD)/heap.o $(BUILD)/test_heap.o -o $@

$(BUILD)/heap.o: src/heap.c src/heap.h | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) -c src/heap.c -o $@

$(BUILD)/test_heap.o: tests/test_heap.c tests/test_harness.h src/heap.h | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) -c tests/test_heap.c -o $@

clock: $(BUILD)/test_clock
	@echo "--- clock test (ASan ON) ---"
	./$(BUILD)/test_clock

$(BUILD)/test_clock: $(BUILD)/clock.o $(BUILD)/test_clock.o | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) $(BUILD)/clock.o $(BUILD)/test_clock.o -o $@

$(BUILD)/clock.o: src/clock.c src/clock.h | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) -c src/clock.c -o $@

$(BUILD)/test_clock.o: tests/test_clock.c tests/test_harness.h src/clock.h | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) -c tests/test_clock.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: test smoke store heap clock clean
