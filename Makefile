XEN_ROOT = /root/xen-unstable.hg
include $(XEN_ROOT)/tools/Rules.mk

INTRANODE_OBJS = intranode_com.o

CYCLES_PER_SEC := $(shell cat /proc/cpuinfo |grep cpu\ MHz|cut -d\: -f2|tail -n 1|awk \{print\ $$\1\ \*\ 1000000\ \} )

CFLAGS += -I../include -I. -DCYCLES_PER_SEC=${CYCLES_PER_SEC}

.PHONY: all
all: communication

communication: $(INTRANODE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(INTRANODE_OBJS) $(LDLIBS_libxenvchan) $(APPEND_LDFLAGS) -lm

.PHONY: clean
clean:
	$(RM) -f *.o *.opic *.so* *.a communication $(DEPS)

distclean: clean

-include $(DEPS)
