ring_test_DIR:=$(tests_DIR)/ring

ring_test_CFLAGS:=-I$(ring_test_DIR)

ring_test_SOURCES:= \
	$(OSM_DIR)/core/src/ring.c \
	$(ring_test_DIR)/ring_test.c

$(eval $(call tests_PROGRAM_template,ring_test))
