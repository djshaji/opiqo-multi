LILV=`pkg-config --cflags --libs lilv-0`
all: test

test: test.cpp
	@echo "Building tests..."
	c++ -o test test.cpp $(LILV) -g
