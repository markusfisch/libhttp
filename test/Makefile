BIN=http
OBJECTS=main.o
LIBS=-L.. -lhttp
FLAGS=-O2 -I.. -Wall -Wextra

.c.o: $(OBJECTS)
	$(CC) -c $< -o $@ $(FLAGS)

$(BIN): $(OBJECTS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f *.o $(BIN)
