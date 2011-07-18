#define fatal(...) sysfatal("devwsys: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprint(2, "devwsys: " __VA_ARGS__)

/* Devdraw */
Client* drawnewclient(Draw*);
Memimage* drawinstall(Client*, int, Memimage*, DScreen*);
void drawfree(Client*);
int drawattach(Window*, char*);
DImage* drawlookup(Client*, int, int);
DName* drawlookupname(int, char*);
int readdrawctl(char*, Client*);
int readrefresh(char*, long, Client*);
int drawmesg(Client*, void*, int);
void drawreplacescreenimage(Draw*, Memimage*);

/* Keyboard */
void writekbd(Window*, int);

/* Mouse */
void writemouse(Window*, Mouse, int);
int cursorwrite(Window*, char*, int);

/* Window */
void deletewin(Window*);
Window* newwin(char*, char*);
int parsewinsize(char*, Rectangle*, int*);
void setlabel(Window*, char*);

/* Wctl */
int wctlmesg(Window*, char*, int, char*);

/* Ixp */
void ixppwrite(void*, char*);
int ixpserve(char*);

/* X connection */
int xinit(void);
int xfd(void);
void xnextevent(void);
void xclose(void);

/* X window */
Memimage* xallocmemimage(Window*, Rectangle, ulong, int);
int xattach(Window*, char*);
void xdeletewin(Window*);
void xflushmemscreen(Window*, Rectangle);
void xfreememimage(Memimage*);
int xupdatelabel(Window*);
void xtopwindow(Window*);
void xresizewindow(Window*, Rectangle);
void xsetcursor(Window*);
char* xgetsnarf(Window*);
void xputsnarf(Window*, char*);
