BIN=bt

$(BIN): bt.c
	gcc -Wall -ggdb bt.c -o $(BIN)

clean:
	rm -f $(BIN)
