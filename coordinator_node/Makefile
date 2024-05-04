TARGETS = coordinator

all: $(TARGETS)

coordinator: coordinator.cpp ../utils/utils.cpp helper.cpp
	g++-10 $^ -std=c++20 -lpthread -lssl -lcrypto -g -o $@

clean::
	rm -fv $(TARGETS) *~