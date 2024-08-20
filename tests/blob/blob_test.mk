blob_test_DIR:=$(tests_DIR)/blob

blob_test_CFLAGS:=-I$(OSM_DIR)/comms/include -I$(blob_test_DIR)

blob_test_SOURCES:= \
  $(OSM_DIR)/protocols/src/hexblob.c \
  $(blob_test_DIR)/blob_empty_send.c

blob_test_OBJS:=$(blob_test_SOURCES:%.c=$(OSM_BUILD_DIR)/tests/%.o)

$(blob_test_OBJS): $(OSM_BUILD_DIR)/tests/%.o: $(OSM_DIR)/%.c
	@mkdir -p "$(@D)"
	$(CC) -c $(tests_CFLAGS) $(blob_test_CFLAGS) $< -o $@

$(tests_OSM_BUILD_DIR)/tests/blob/bloblib.o: $(blob_test_OBJS)
	@mkdir -p $(@D)
	ld -r $^ -o $@

$(tests_OSM_BUILD_DIR)/tests/blob/bloblib.so: $(tests_OSM_BUILD_DIR)/tests/blob/bloblib.o
	@mkdir -p $(@D)
	$(CC) $^ $(tests_LDFLAGS) -shared -o $@

$(tests_OSM_BUILD_DIR)/tests/blob/blob_tests.py: $(blob_test_DIR)/blob_tests.py
	@mkdir -p $(@D)
	cp $< $@
	chmod +x $@

$(tests_OSM_BUILD_DIR)/tests/blob/debug.js: $(OSM_DIR)/lorawan_protocol/debug.js
	@mkdir -p $(@D)
	cp $< $@

$(tests_OSM_BUILD_DIR)/tests/blob/cs_protocol.js: $(OSM_DIR)/lorawan_protocol/cs_protocol.js
	@mkdir -p $(@D)
	cp $< $@

build/tests/blob_test.elf: $(tests_OSM_BUILD_DIR)/tests/blob/blob_tests.py $(tests_OSM_BUILD_DIR)/tests/blob/bloblib.so $(tests_OSM_BUILD_DIR)/tests/blob/debug.js $(tests_OSM_BUILD_DIR)/tests/blob/cs_protocol.js
	echo "#!/bin/sh \n\
	python3 $(shell realpath $(tests_OSM_BUILD_DIR)/tests/blob/blob_tests.py) $(shell realpath $(tests_OSM_BUILD_DIR)/tests/blob/bloblib.so)" > $@
	chmod +x $@
