BUILD_DIR ?= build
CMAKE ?= cmake

.PHONY: all test demos clean

all:
	$(CMAKE) -S . -B $(BUILD_DIR)
	$(CMAKE) --build $(BUILD_DIR) -j

test: all
	cd $(BUILD_DIR) && ctest --output-on-failure

demos: all
	$(BUILD_DIR)/axi4_smoke_demo
	$(BUILD_DIR)/axi3_smoke_demo

clean:
	rm -rf $(BUILD_DIR)
