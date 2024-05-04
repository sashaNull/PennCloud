TARGETS = server

# Add -lssl and -lcrypto to link against OpenSSL libraries
LDFLAGS = -lpthread -lssl -lcrypto -lresolv -g

all: $(TARGETS)

server: server.cpp ../utils/utils.cpp ./backend_communication.cpp ./client_communication.cpp ./webmail.cpp ./admin.cpp
	g++ $^ $(LDFLAGS) -o $@

clean::
	rm -fv $(TARGETS) *~