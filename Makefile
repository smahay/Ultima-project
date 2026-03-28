CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread
LIBS = -lncurses
TARGET = ultima

SRCS = Ultima.cpp Sched.cpp Sema.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)