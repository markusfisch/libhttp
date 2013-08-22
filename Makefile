LIBNAME=libhttp
OBJECTS=http.o
FLAGS=-O2

.c.o:
	$(CC) -c $< -o $@ $(FLAGS)

$(LIBNAME).a: $(OBJECTS)
	$(AR) rcs $@ $^

$(LIBNAME).so: $(OBJECTS)
	$(CC) -shared -o $@ $^

clean:
	rm -f *.o $(LIBNAME).*
