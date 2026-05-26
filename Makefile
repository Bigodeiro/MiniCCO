CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

TARGET = main
PYTHON = python3
PLOT_SCRIPT = plot.py
OUTPUT_FILE = arvore.txt
OUTPUT_PNG =  arvore.png

# Argumentos fixos
ARG1 = 3.0
ARG2 = 4.0

.PHONY: all run plot clean

all: run plot

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp

run: $(TARGET)
	./$(TARGET) $(ARG1) $(ARG2)

plot: run
	$(PYTHON) $(PLOT_SCRIPT) $(OUTPUT_FILE) $(OUTPUT_PNG)

clean:
	rm -f $(TARGET) $(OUTPUT_FILE)