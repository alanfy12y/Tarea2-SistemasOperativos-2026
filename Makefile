CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Iinclude

.PHONY: all test-monitores clean

all: test-monitores

test-monitores: build/test_monitores
	./build/test_monitores

build/test_monitores: src/tipos.cpp src/monitor_lectores.cpp src/monitor_escritores.cpp tests/test_monitores.cpp include/tipos.h include/control_rw.h include/monitor_lectores.h include/monitor_escritores.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) src/tipos.cpp src/monitor_lectores.cpp src/monitor_escritores.cpp tests/test_monitores.cpp -o $@

clean:
	rm -rf build
