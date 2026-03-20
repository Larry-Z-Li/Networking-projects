CXX = g++
CXXFLAGS = -Wall -g

# Define the programs and their source files
PROGRAM1 = wSender
PROGRAM1_SRCS = wSender.cpp

PROGRAM2 = wReceiver
PROGRAM2_SRCS = wReceiver.cpp

# Build rules
all: $(PROGRAM1) $(PROGRAM2)

$(PROGRAM1): $(PROGRAM1_SRCS)
	$(CXX) $(CXXFLAGS) $(PROGRAM1_SRCS) -o $@

$(PROGRAM2): $(PROGRAM2_SRCS)
	$(CXX) $(CXXFLAGS) $(PROGRAM2_SRCS) -o $@

clean:
	rm -f $(PROGRAM1) $(PROGRAM2) *.o
