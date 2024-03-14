.PHONY: all clean

all: server client
server: server.c
	gcc -Wall -o server http.c server.c ./utils/base64.c
client: client.c
	gcc -Wall -o client http.c client.c ./utils/base64.c
clean:
	@rm -rf server client 
