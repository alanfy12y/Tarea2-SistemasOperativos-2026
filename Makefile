CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Iinclude

SRCS := src/tipos.cpp src/monitor_lectores.cpp src/monitor_escritores.cpp src/verificador.cpp
HDRS := include/tipos.h include/control_rw.h include/monitor_lectores.h include/monitor_escritores.h include/verificador.h

.PHONY: all test-monitores clean

all: test-monitores

test-monitores: build/test_monitores
	./build/test_monitores

build/test_monitores: $(SRCS) $(HDRS) tests/test_monitores.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRCS) tests/test_monitores.cpp -o $@

clean:
	rm -rf build
