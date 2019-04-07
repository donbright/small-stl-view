/* Fake version of VST for basic graphics layout testing of "stl.ino"
 * This implements moveto() and lineto() as SVG file output.
 * For exompilation + usage instructions please see stl.ino */

#include <cstdio>
#include <cstring>
#include <cinttypes>

int rx_points=0;
int num_points=0;
int timeSet=0;

int timeStatus(
)
{
	printf("timeStatus()\n");
	return 0;
}

void draw_string(
	const char * c,
	int x,
	int y,
	int b)
{
	printf("draw_string(%s,%i,%i,%i)\n",c,x,y,b);
}

void moveto(
	int x,
	int y
)
{
	printf(" M %i %i \n", x, y );
}

void lineto(
	int x,
	int y
)
{
	printf(" L %i %i \n", x, y );
}

#include "stl.ino"

int main(
)
{
//	int w = 640;
//	int h = 480;
	int w = 500; //FIXME doent work
	int h = 800;
	printf("<svg width='%i' height='%i' xmlns='http://www.w3.org/2000/svg' version='1.1'>\n",w,h);
	printf(" <!-- border -->\n");
	printf(" <path fill='none' stroke='black' d='M 0 0 L %i %i L %i %i L %i %i L 0 0'/>\n",0,h,w,h,w,0);
	box2d screen;
	screen.min.x = int_to_fix( 0 );
	screen.min.y = int_to_fix( 0 );
	screen.max.x = int_to_fix( w );
	screen.max.y = int_to_fix( h );
	stl_setup_triangles3d( triangles, screen );
	printf("  <path fill='none' stroke='black' fill-opacity='0.45' d='");
	stl_draw_triangles3d( triangles, screen );
	printf("'  />\n");
	printf("</svg>\n");
	return 0;
}


