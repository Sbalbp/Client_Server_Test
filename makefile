SRC = src
BIN = bin

all: $(BIN) $(BIN)/server $(BIN)/client

$(BIN):
	mkdir -p $(BIN)

$(BIN)/server: $(SRC)/server.c
	gcc -o $(BIN)/server $(SRC)/server.c

$(BIN)/client: $(SRC)/client.c
	gcc -o $(BIN)/client $(SRC)/client.c
