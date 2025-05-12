vinac: vinac.o diretorio.o membros.o inserir.o mover.o remover.o extrair.o listar.o lz.o
	gcc -o vinac vinac.o diretorio.o membros.o inserir.o mover.o remover.o extrair.o listar.o lz.o -Wall

vinac.o: vinac.c membros.h
	gcc -c vinac.c -Wall

diretorio.o: diretorio.c diretorio.h
	gcc -c diretorio.c -Wall

membros.o: membros.c membros.h
	gcc -c membros.c -Wall

inserir.o: inserir.c membros.h
	gcc -c inserir.c -Wall

mover.o: mover.c membros.h
	gcc -c mover.c -Wall

remover.o: remover.c membros.h
	gcc -c remover.c -Wall

extrair.o: extrair.c membros.h
	gcc -c extrair.c -Wall

listar.o: listar.c membros.h
	gcc -c listar.c -Wall

lz.o: lz.c lz.h
	gcc -c lz.c -Wall

clean:
	rm -f *.o vinac
