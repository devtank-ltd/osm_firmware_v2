tests_CFLAGS:=-Os -I$(OSM_DIR)/core/include -I$(OSM_DIR)/sensors/include --coverage
tests_LDFLAGS:= -lgcov

tests_DIR:=$(OSM_DIR)/tests
tests_BUILD_DIR:=$(BUILD_DIR)/tests

tests_CFLAGS+=-I$(tests_DIR)

PROGRAMS_MKS = $(shell find $(tests_DIR) -maxdepth 2 -mindepth 2 -name "*.mk" -printf "%f\n")

TESTS_ELFS = $(PROGRAMS_MKS:%.mk=$(tests_BUILD_DIR)/%.elf)

tests: $(TESTS_ELFS)
	echo "$<"
	for test in $(TESTS_ELFS); do echo "====== $$test ==== "; ./$$test; done
	echo "====== DONE ==== "



define tests_PROGRAM_template
  $(1)_OBJS=$$($(1)_SOURCES:%.c=$(BUILD_DIR)/%.o)
  $$($(1)_OBJS): $$($(1)_SOURCES)
	@mkdir -p "$$(@D)"
	$(CC) -c $(tests_CFLAGS) $$($(1)_CFLAGS) $$< -o $$@
  $(tests_BUILD_DIR)/$(1).elf: $$($(1)_OBJS)
	@mkdir -p "$$(@D)"
	$(CC) $$($(1)_OBJS) $$(tests_LDFLAGS) -o $$@
endef

include $(tests_DIR)/ring/ring_test.mk
