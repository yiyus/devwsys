typedef struct Mousebuf Mousebuf;
typedef struct Tagbuf Tagbuf;

struct  Mouse
{
	int		buttons;	/* bit array: LMR=124 */
	Point	xy;
	ulong	msec;
};

struct Mousebuf
{
	Mouse m[32];
	int ri;
	int wi;
	int stall;
	int resized;
};

struct Tagbuf
{
	int t[32];
	void *r[32];
	int ri;
	int wi;
};

void addmouse(int, Mouse);
void matchmouse(int);

