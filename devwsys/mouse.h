typedef struct Mousebuf Mousebuf;

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

void addmouse(int, Mouse, int);
void matchmouse(int);
