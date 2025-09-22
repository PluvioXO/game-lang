CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2
DEBUG_FLAGS = -g -O0 -DDEBUG
SRCDIR = src
BUILDDIR = build
BINDIR = bin
TARGET = gamelang

# Find all source files
SOURCES = $(shell find $(SRCDIR) -name "*.cpp")
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)

# Default target
all: $(BINDIR)/$(TARGET)

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(BINDIR)/$(TARGET)

# Main executable
$(BINDIR)/$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) $(OBJECTS) -o $@

# Object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

# Create directories
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR) $(BINDIR)

# Install (optional)
install: $(BINDIR)/$(TARGET)
	cp $(BINDIR)/$(TARGET) /usr/local/bin/

# Run the interpreter
run: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET)

# Run with a specific file
run-file: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET) $(FILE)

.PHONY: all debug clean install run run-file