FLAGS:=$(shell pkg-config --cflags --libs libdrm)
FLAGS+=-Wall -O0 -g
FLAGS+=-D_FILE_OFFSET_BITS=64

CXX=g++
CXX_FLAGS=$(shell pkg-config --cflags --libs libdrm) -Wall -O0 -g
CXX_INCLUDES=
CXX_LIBS=
CXX_SRCS=plus/Config.cpp plus/Exception.cpp plus/plus.cpp
CXX_OBJS=$(CXX_SRCS:.cpp=.o)
CXX_EXEC=modeset-plus


.PHONY: depend clean

all:	$(CXX_EXEC)
		@echo C++ binary has been combipled
		gcc -o modeset modeset.c $(FLAGS)
		gcc -o modeset-double-buffered modeset-double-buffered.c $(FLAGS)
		gcc -o modeset-vsync modeset-vsync.c $(FLAGS)

$(CXX_EXEC): $(CXX_OBJS) 
		$(CXX) $(CXX_FLAGS) $(CXX_INCLUDES) -o $(CXX_EXEC) $(CXX_OBJS) $(CXX_FLAGS) $(CXX_LIBS)

# this is a suffix replacement rule for building .o's from .cpp's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.cpp.o:
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) $(CXX_OBJS)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
