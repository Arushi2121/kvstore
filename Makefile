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
CFLAGS = -std=c11 -Wall -Wextra -g -Isrc -Itests
ASAN = -fsanitize=address -fno-omit-frame-pointer
BUILD = build

test: smoke store
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

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: test smoke store clean
