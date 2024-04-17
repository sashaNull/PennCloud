TARGETS = coordinator

all: $(TARGETS)

coordinator: coordinator.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~