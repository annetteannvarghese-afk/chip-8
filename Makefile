CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
# On Windows/MinGW, SDL2 needs a specific link order:
#   -lmingw32   → Windows startup code
#   -lSDL2main  → Provides WinMain which calls our main()
#   -lSDL2      → The actual SDL2 library
#   -mwindows   → Tells linker this is a GUI app (not console)
LDFLAGS = -lmingw32 -lSDL2main -lSDL2 -mwindows
TARGET = chip8
SOURCES = src/main.cpp src/chip8.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean