TARGETS = loadbalancer

all: $(TARGETS)

loadbalancer: loadbalancer.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~