pop3:
	gcc -Wall -c util.c -o util.o -m32 -lws_32 -liphlpapi -DDEBUG -g

	gcc -Wall -c mailserver.c -o mailserver.o -m32 -lws2_32 -liphlpapi -DDEBUG -g
	gcc -Wall mailserver.o util.o -o mailserver -m32 -lws2_32 -liphlpapi -DDEBUG -g

	gcc -Wall -c husers.c -o husers.o -m32 -lws2_32 -liphlpapi -DDEBUG -g
	gcc -Wall husers.o util.o -o husers -m32 -lws2_32 -liphlpapi -DDEBUG -g
	del *.o