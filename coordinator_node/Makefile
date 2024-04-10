TARGETS = coordinator

all: $(TARGETS)

server: coordinator.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~