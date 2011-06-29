#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>

Image *col;
Image *black;

int chatty9p = 1;

void
main(void) {
	int i;
	ulong type = ARGB32;

	// newwindow("-r 0 0 800 400");
	if(initdraw(nil, nil, "tri") < 0)
		exits("initdraw");

	black = allocimage(display, Rect(0, 0, Dx(screen->r), Dy(screen->r)), ARGB32, 0, DBlack);

	for(i = 0; i < 0xff; i++) {
		col = allocimage(display, Rect(0,0,1,1), type, 1, setalpha(DWhite, 0xff-i));
		draw(black, Rect(0,i,200,i+1), col, nil, ZP);
		freeimage(col);

		col = allocimage(display, Rect(0,0,1,1), type, 1, setalpha(DRed, 0xff-i));
		draw(black, Rect(200,i,400,i+1), col, nil, ZP);
		freeimage(col);
		col = allocimage(display, Rect(0,0,1,1), type, 1, setalpha(DGreen, 0xff-i));
		draw(black, Rect(400,i,600,i+1), col, nil, ZP);
		freeimage(col);
		col = allocimage(display, Rect(0,0,1,1), type, 1, setalpha(DBlue, 0xff-i));
		draw(black, Rect(600,i,800,i+1), col, nil, ZP);
		freeimage(col);	
	}
	col = allocimage(display, Rect(0, 0, 1, 1), type, 1, DBlack);
	draw(black, Rect(0, 260, 800, 300), col, nil, ZP);
	draw(screen, screen->r, black, nil, ZP);

	draw(screen, Rect(0, 360, 800, 400), col, nil, ZP);


	flushimage(display, 1);
	sleep(10000);
	closedisplay(display);
}
