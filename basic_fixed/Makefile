CFLAGS:=-Os -MD -MP
LDFLAGS:=
OBJS:=main.o basic_fixed.o
DEBS = $(patsubst %.o, %.d, $(OBJS))
PROGRAM:=basic_fixed

$(PROGRAM): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS) $(PROGRAM) $(DEBS)

-include $(DEBS)
