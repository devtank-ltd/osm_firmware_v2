tests_CFLAGS:=-Os -I$(OSM_DIR)/core/include -I$(OSM_DIR)/sensors/include --coverage
tests_CFLAGS+=-Wall -Werror -pedantic
tests_LDFLAGS:= -lgcov

tests_DIR:=$(OSM_DIR)/tests
tests_OSM_BUILD_DIR:=$(OSM_BUILD_DIR)/tests

tests_CFLAGS+=-I$(tests_DIR)

PROGRAMS_MKS = $(shell find $(tests_DIR) -maxdepth 2 -mindepth 2 -name "*.mk" -printf "%f\n")

TESTS_ELFS = $(PROGRAMS_MKS:%.mk=$(tests_OSM_BUILD_DIR)/%.elf)

.PHONY: tests

tests: $(TESTS_ELFS)
	@for test in $(TESTS_ELFS); do echo "====== $$test ==== "; ./$$test; done
	@echo "====== DONE ==== "

define tests_PROGRAM_template
  $(1)_OBJS=$$($(1)_SOURCES:%.c=$(OSM_BUILD_DIR)/tests/%.o)
  $$($(1)_OBJS): $$(OSM_BUILD_DIR)/tests/%.o: $$(OSM_DIR)/%.c
	@mkdir -p "$$(@D)"
	$(CC) -c $(tests_CFLAGS) $$($(1)_CFLAGS) $$< -o $$@
  $(tests_OSM_BUILD_DIR)/$(1).elf: $$($(1)_OBJS)
	@mkdir -p "$$(@D)"
	$(CC) $$($(1)_OBJS) $$(tests_LDFLAGS) -o $$@
endef

$(foreach file_mk,$(shell find $(tests_DIR) -maxdepth 2 -mindepth 2 -name "*.mk"),$(eval include $(file_mk)))
