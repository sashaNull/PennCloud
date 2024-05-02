TARGETS = client

all: $(TARGETS)

client: client.cpp ../../utils/utils.cpp ../utils.cpp
	g++-10 $^ -std=c++20 -lpthread -lssl -lcrypto -g -o $@

fronetendclient: frontendclient.cpp ../../utils/utils.cpp ../utils.cpp
	g++-10 $^ -std=c++20 -lpthread -lssl -lcrypto -g -o $@

clean::
	rm -fv $(TARGETS) *~
