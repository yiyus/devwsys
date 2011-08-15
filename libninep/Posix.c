#define	__EXTENSIONS__
#define	_BSD_COMPAT
#include <lib9.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<signal.h>
#include	<pwd.h>
#include 	<sys/types.h>
#include	<sys/time.h>
#include	<sys/stat.h>
#include	<sys/socket.h>
#include	<sys/un.h>

#include <ninep.h>
#include "ninepserver.h"
#include "ninepaux.h"

enum {
	MAX_CACHE = 32,
};

typedef struct Fdset Fdset;
typedef struct addrinfo addrinfo;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_un sockaddr_un;
typedef struct sockaddr_in sockaddr_in;

struct Fdset
{
	fd_set infds, outfds, excfds, r_infds, r_outfds, r_excfds;
};

int
ninepinitsocket(void)
{
	return 0;
}

void
ninependsocket(void)
{
}

void
ninepclosesocket(int fd)
{
	close(fd);
}

static
char*
getport(char *addr) {
	char *s;

	s = strchr(addr, '!');
	if(s == nil) {
		werrstr("no port provided");
		return nil;
	}

	*s++ = '\0';
	if(*s == '\0') {
		werrstr("invalid port number");
		return nil;
	}
	return s;
}

static
int
sockunix(char *address, sockaddr_un *sa, socklen_t *salen) {
	int fd;

	memset(sa, 0, sizeof *sa);

	sa->sun_family = AF_UNIX;
	strncpy(sa->sun_path, address, sizeof sa->sun_path);
	*salen = SUN_LEN(sa);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0)
		return -1;
	return fd;
}

static
addrinfo*
alookup(char *host, int announce) {
	addrinfo hints, *ret;
	char *port;
	int err;

	/* Truncates host at '!' */
	port = getport(host);
	if(port == nil)
		return nil;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if(announce) {
		hints.ai_flags = AI_PASSIVE;
		if(!strcmp(host, "*"))
			host = nil;
	}

	err = getaddrinfo(host, port, &hints, &ret);
	if(err) {
		werrstr("getaddrinfo: %s", gai_strerror(err));
		return nil;
	}
	return ret;
}

static int
aisocket(addrinfo *ai) {
	return socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
}

static
int
announcetcp(char *host) {
	addrinfo *ai, *aip;
	int fd;

	aip = alookup(host, 1);
	if(aip == nil)
		return -1;

	/* Probably don't need to loop */
	SET(fd);
	for(ai = aip; ai; ai = ai->ai_next) {
		fd = aisocket(ai);
		if(fd == -1)
			continue;

		if(bind(fd, ai->ai_addr, ai->ai_addrlen) < 0)
			goto fail;

		if(listen(fd, MAX_CACHE) < 0)
			goto fail;
		break;
	fail:
		close(fd);
		fd = -1;
	}

	freeaddrinfo(aip);
	return fd;
}

static
int
announceunix(char *file) {
	const int yes = 1;
	sockaddr_un sa;
	socklen_t salen;
	int fd;

	signal(SIGPIPE, SIG_IGN);

	fd = sockunix(file, &sa, &salen);
	if(fd == -1)
		return fd;

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof yes) < 0)
		goto fail;

	unlink(file);
	if(bind(fd, (sockaddr*)&sa, salen) < 0)
		goto fail;

	chmod(file, S_IRWXU);
	if(listen(fd, MAX_CACHE) < 0)
		goto fail;

	return fd;

fail:
	close(fd);
	return -1;
}

int
ninepannounce(Ninepserver *server, char *address)
{
	char *addr, *type;
	int ret;

	USED(server);
	ret = -1;
	type = strdup(address);

	if(type == nil)
		sysfatal("no mem");
	addr = strchr(type, '!');
	if(addr == nil)
		werrstr("no address type defined");
	else {
		*addr++ = '\0';
		if(strcmp(type, "tcp") == 0)
			ret = announcetcp(addr);
		else if(strcmp(type, "unix") == 0)
			ret = announceunix(addr);
		else
			werrstr("unsupported address type");
	}

	free(type);
	return ret;
}

int
ninepaccept(Ninepserver *server)
{
	struct sockaddr_in sin;
	int len, s;

	len = sizeof(sin);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	s = accept(server->connfd, (struct sockaddr *)&sin, &len);
	if(s < 0){
		if(errno != EINTR)
			fprint(2, "error in accept: %s\n", strerror(errno));
	}
	return s;
}

void
ninepinitwait(Ninepserver *server)
{
	Fdset *fs;

	server->priv = fs = malloc(sizeof(Fdset));
	FD_ZERO(&fs->infds);
	FD_ZERO(&fs->outfds);
	FD_ZERO(&fs->excfds);
	FD_SET(server->connfd, &fs->infds);
}

int
ninepnewcall(Ninepserver *server)
{
	Fdset *fs;

	fs = server->priv;
	return FD_ISSET(server->connfd, &fs->r_infds);
}

void
ninepnewclient(Ninepserver *server, int s)
{
	Fdset *fs;

	fs = server->priv;
	FD_SET(s, &fs->infds);
}

void
ninepfreeclient(Ninepserver *server, int s)
{
	Fdset *fs;

	fs = server->priv;
	FD_CLR(s, &fs->infds);
}

int
ninepnewmsg(Ninepserver *server, int s)
{
	Fdset *fs;

	fs = server->priv;
	return FD_ISSET(s, &fs->r_infds) || FD_ISSET(s, &fs->r_excfds);
}

char*
ninepwaitmsg(Ninepserver *server)
{
	struct timeval seltime;
	int nfds;
	Fdset *fs;

	fs = server->priv;
	fs->r_infds = fs->infds;
	fs->r_outfds = fs->outfds;
	fs->r_excfds = fs->excfds;
	seltime.tv_sec = 10;
	seltime.tv_usec = 0L;
	nfds = select(sizeof(fd_set)*8, &fs->r_infds, &fs->r_outfds, &fs->r_excfds, &seltime);
	if(nfds < 0 && errno != EINTR)
		return"error in select";
	return nil;
}

int
nineprecv(Ninepserver *server, int fd, char *buf, int n, int m)
{
	return recv(fd, buf, n, m);
}

int
ninepsend(Ninepserver *server, int fd, char *buf, int n, int m)
{
	return send(fd, buf, n, m);
}

void
ninepexit(int n)
{
	exit(n);
}

static
char*
username(void) {
	static char *user;
	struct passwd *pw;

	if(user == nil) {
		pw = getpwuid(getuid());
		if(pw)
			user = strdup(pw->pw_name);
	}
	if(user == nil)
		user = "none";
	return user;
}

static
char*
displayns(void) {
	char *path, *disp;
	struct stat st;

	disp = getenv("DISPLAY");
	if(disp == nil || disp[0] == '\0')
		sysfatal("$DISPLAY is unset");

	disp = strdup(disp);
	if(disp == nil)
		sysfatal("no mem");
	path = &disp[strlen(disp) - 2];
	if(path > disp && !strcmp(path, ".0"))
		*path = '\0';

	path = smprint("/tmp/ns.%s.%s", username(), disp);
	free(disp);

	if(mkdir(path, 0700) == -1 && errno != EEXIST)
		sysfatal("can't create path %s\n", path);
	else if(stat(path, &st))
		sysfatal("can't stat namespace path %s", path);
	else if(getuid() != st.st_uid)
		sysfatal("namespace path %s exists but is not owned by you", path);
	else if((st.st_mode & 077) && chmod(path, st.st_mode & ~077))
		sysfatal("namespace path %s exists, but has wrong permissions", path);
	else
		return path;
	free(path);
	return nil;
}

char*
ninepnamespace(void)
{
	char *ns;

	/*
	 * Check if there's a namespace as defined by p9p.
	 */
	ns = getenv("NAMESPACE");
	if(ns == nil)
		ns = displayns();
	if(mkdir(ns, 0700) == -1 && errno != EEXIST) {
		fprint(2, "mkdir: %s", ns);
		return nil;
	}
	return smprint("unix!%s", ns);
}
