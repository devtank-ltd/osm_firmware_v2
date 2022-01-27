CFLAGS:=-Os -I../ --coverage
LDFLAGS:= -lgcov

BUILD_DIR := build


$(BUILD_DIR)/%.o: %.c
	mkdir -p "$(@D)"
	$(CC) -c $(CFLAGS) $< -o $@

define PROGRAM_template
  include $(1)
  $(2)_OBJS=$$($(2)_SOURCES:%.c=$(BUILD_DIR)/%.o)
  $(BUILD_DIR)/$(2).elf: $$($(2)_OBJS)
	$(CC) $$($(2)_OBJS) $$(LDFLAGS) -o $$@
endef

PROGRAMS_MKS = $(shell find . -maxdepth 1 -name "*.mk" -printf "%f\n")

TESTS = $(PROGRAMS_MKS:%.mk=$(BUILD_DIR)/%.elf)

default: $(TESTS)

# To see what it's doing, just replace "eval" with "info"
$(foreach file_mk,$(PROGRAMS_MKS),$(eval $(call PROGRAM_template,$(file_mk),$(file_mk:%.mk=%))))


clean:
	rm -rf $(BUILD_DIR)

.PHONY: coverage
coverage: $(TESTS)
	lcov --zerocounters -d $(BUILD_DIR)
	lcov --capture --initial -d $(BUILD_DIR) --output-file coverage.info
	for test in $(TESTS); do echo "====== $$test ==== "; ./$$test; done
	echo "====== DONE ==== "
	lcov --capture -d $(BUILD_DIR) --output-file coverage.info
	mkdir -p coverage
	cd coverage && genhtml ../coverage.info
	sensible-browser coverage/index.html
