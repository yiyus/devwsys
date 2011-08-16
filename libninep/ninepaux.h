int ninepinitsocket(void);
void ninependsocket(void);
void ninepclosesocket(int);
int ninepannounce(Ninepserver*, char *);
void ninepinitwait(Ninepserver*);
int ninepnewcall(Ninepserver*);
int ninepnewmsg(Ninepserver*, int);
int ninepaccept(Ninepserver *server);
void ninepnewclient(Ninepserver*, int);
void ninepfreeclient(Ninepserver*, int);
char* ninepwaitmsg(Ninepserver*);
int nineprecv(Ninepserver*, int, char*, int, int);
int ninepsend(Ninepserver*, int, char*, int, int);
void ninepexit(int);
char* ninepuser(void);

