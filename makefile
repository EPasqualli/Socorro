vinac: vinac.o arquivo.o membros.o lz.o 
	gcc -o vinac vinac.o arquivo.o membros.o lz.o -Wall

vinac.o: vinac.c membros.h 
	gcc -c vinac.c -Wall

arquivo.o: arquivo.c arquivo.h
	gcc -c arquivo.c -Wall

membros.o: membros.c membros.h
	gcc -c membros.c -Wall

lz.o: lz.c lz.h
	gcc -c lz.c -Wall

clean:
	rm -f *.o vinac  # Limpa vinac