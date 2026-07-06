CC ?= clang
CFLAGS = -std=c11 -Wall -Wextra -g -Isrc -Itests
ASAN = -fsanitize=address -fno-omit-frame-pointer
BUILD = build

test: $(BUILD)/test_smoke
	@echo "Running tests (AddressSanitizer ON)..."
	./$(BUILD)/test_smoke

$(BUILD)/test_smoke: tests/test_smoke.c tests/test_harness.h | $(BUILD)
	$(CC) $(CFLAGS) $(ASAN) tests/test_smoke.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: test clean
