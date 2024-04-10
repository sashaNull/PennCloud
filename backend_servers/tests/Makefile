TARGETS = client

all: $(TARGETS)

client: client.cpp ../../utils/utils.cpp
	g++ $^ -lpthread -g -o $@

clean::
	rm -fv $(TARGETS) *~
