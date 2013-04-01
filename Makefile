XEN_ROOT = /root/xen-unstable.hg
include $(XEN_ROOT)/tools/Rules.mk

INTRANODE_OBJS = intranode_com.o

CFLAGS += -I../include -I.

.PHONY: all
all: communication

communication: $(INTRANODE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(INTRANODE_OBJS) $(LDLIBS_libxenvchan) $(APPEND_LDFLAGS)

.PHONY: clean
clean:
	$(RM) -f *.o *.opic *.so* *.a communication $(DEPS)

distclean: clean

-include $(DEPS)
