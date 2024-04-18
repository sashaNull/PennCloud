TARGETS = coordinator

all: $(TARGETS)

coordinator: coordinator.cpp helper.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~