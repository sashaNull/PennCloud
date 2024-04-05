TARGETS = client

all: $(TARGETS)

client: client.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
