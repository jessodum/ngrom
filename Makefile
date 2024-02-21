PROGRAM=ngrom

CXX=g++
RM=rm -f

SRCFILES= \
   ngrom.cpp

OBJFILES=$(subst .cpp,.o,$(SRCFILES))

INCLUDE_DIRS= \
 -I /usr/include/qt5 \
 -I /usr/include/qt5/QtCore

DEFINES= \
 -DQT_NO_DEBUG \
 -DQT_CORE_LIB

# Used by GNU make built-in rule for $(OBJFILES); i.e., %.o: %.cpp
CPPFLAGS= \
 $(INCLUDE_DIRS) \
 -O2 -Wall -Wextra -D_REENTRANT -fPIC \
 $(DEFINES) \


LDFLAGS= \


LDLIBS= \
 -L/usr/lib64 \
  -lQt5Core \
 -lpthread

# First target is default
default: all

# "Phony" targets are rules that don't create a file of the target name.
.PHONY: default all clean

all: $(PROGRAM)

# $(OBJFILES): # This is a GNU make built-in rule.

$(PROGRAM): $(OBJFILES)
	@echo
	@echo ///////// Building $@ /////////
	$(CXX) $(LDFLAGS) -o $@ $(OBJFILES) $(LDLIBS)
	@echo
	@echo $@ finished

clean:
	$(RM) $(OBJFILES)
	$(RM) $(PROGRAM)

