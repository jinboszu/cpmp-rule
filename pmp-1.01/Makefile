#
#  $Id: Makefile,v 1.4 2016/11/29 13:49:40 tanaka Exp tanaka $
#  $Revision: 1.4 $
#  $Date: 2016/11/29 13:49:40 $
#  $Author: tanaka $
#
.PHONY: all strip clean depend

ARCH      := $(shell uname -m)
OBJS       = main.o heuristics.o print.o problem.o solution.o solve.o timer.o
SRCS      := $(OBJS:.o=.c)

TARGET     = pmp

CC         = gcc
MAKEDEP    = gcc -MM

CFLAGS     = -Wall -g -O3 -march=corei7 #-fomit-frame-pointer 
LIBS       = 

override DEFS += -I. # -DDEBUG -DUSE_CLOCK

all:: $(TARGET)

.c.o:
	$(CC) $(CFLAGS) $(DEFS) -c $<

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFS) -o $@ $(OBJS) $(LIBS)

strip:: $(TARGET)
	@strip $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) *~ *.bak #*

depend:
	@sed -i -e "/^# START/,/# END/d" Makefile
	@echo "# START" >> Makefile
	@$(MAKEDEP) $(DEFS) $(SRCS) >> Makefile
	@echo "# END" >> Makefile


# START
main.o: main.c define.h print.h problem.h solution.h timer.h solve.h
heuristics.o: heuristics.c define.h heuristics.h problem.h solution.h \
 print.h timer.h
print.o: print.c define.h print.h problem.h solution.h timer.h
problem.o: problem.c define.h problem.h
solution.o: solution.c define.h solution.h problem.h
solve.o: solve.c define.h print.h problem.h solution.h timer.h solve.h
timer.o: timer.c define.h timer.h problem.h
# END
