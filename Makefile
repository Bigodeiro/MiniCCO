CXX = g++
CXXFLAGS = -Wall -O3
TARGET = cco
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) arvore.txt
