/** file
 * STL (3d file format) viewer for vector displays.
 * For a small micro controller.
 *
 * How to use:
 *
 *  Copy and paste your ASCII stl file to the
 *  stl_data string at the end of this file. :)
 *
 *  Then set MAX_TRIANGLES to a number higher than
 *  the # of triangles in your STL
 *
 *  We use Q16.16 fixed-point ints. Each triangle will use 3*3*4 bytes. 
 *  So 1000 tris ~= 36kbytes
 *
 * Compiling:
 *
 *  To compile for microcontroller, open Arduino IDE
 *
 *  To compile a 'tester' version to test rendering on linux:
 *
 *  g++ -std=c++0x tester.cc && ./a.out > x.svg && firefox x.svg
 *
 * Design:
 *
 * Fixed point math, Q16.16 format, 16 bit for integer, 16 bit for fractions.
 * STL ASCII decimal converts to binary Q1616 3d data in an array of triangle3d.
 * Array of triangle3d ends with 'null triangle' (nine 0 coordinates)
 * Triangle3d data is 3d-scaled down to 'draw box', a 3d cube with sides
 *  about the lenth of the viewscreens minimum dimension. Centered at 0,0,0.
 * For each scaled-down 3d triangle:
 *  3d triangle is projected onto 2d plane, centered at 0,0.
 *  2d triangles are moved to screen, centered at screenwidth/2,screenheight/2.
 *  2d triangle data is drawn on screen with lineto() and moveto().
 *  calls to moveto() lineto() convert fixed-point Q16.16 to integer.
 * Logging can be turned on or off at #define LOGGING
 * Lots of &pass-by-reference, avoiding pointers
 * Let the compiler do work, don't worry too much about optimization
 * Minimal object orientation. goloang-ish structs + funcs
 * Overly wordy docs + ascii art are OK, because Brian Paul did it :)
 *
 * License: 
 * MIT license, as below. copyright don bright 2015, except
 * Division and multiplication functions are taken from here:
 * https://github.com/Yveaux/Arduino_fixpt/blob/master/fix16.c
 * Division and Multiplication functions have the following MIT license:

The MIT License (MIT)

Copyright (c) 2014 Flatmush@googlemail.com, i.pullens@emmission.nl

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#define MAX_TRIANGLES 10000

extern const char * stl_data;

#define LOGGING 1

#ifdef LOGGING
#define logf(...) do \
{ \
	printf("<!-- ");printf(__VA_ARGS__);printf(" -->\n"); \
} while(0)
#else
#define logf(...) {}
#endif

typedef int32_t fixed_t;

typedef struct {
	fixed_t x,y;
} point2d;

typedef struct {
	fixed_t x,y,z;
} point3d;

typedef point2d vector2d;

typedef point3d vector3d;

typedef struct {
	vector2d max, min;
} box2d;

typedef struct {
	vector3d max, min;
} box3d;

typedef struct {
	point2d points[3];
} triangle2d;

typedef struct {
	point3d points[3];
} triangle3d;

fixed_t addfix(
	fixed_t a,
	fixed_t b
)
{
	return (a+b);
}

fixed_t subfix(
	fixed_t a,
	fixed_t b
)
{
	return (a-b);
}

/* 16.16 fixed point division, see MIT license at top of file. Modified From
   https://github.com/Yveaux/Arduino_fixpt/blob/master/fix16.c  */
fixed_t divfix(
	fixed_t a,
	fixed_t b
)
{
	uint32_t remainder = (a >= 0) ? a : (-a);
	uint32_t divider = (b >= 0) ? b : (-b);
	uint32_t quotient = 0;
	uint32_t bit = 0x10000;
	
	while (divider < remainder)
	{
		divider <<= 1;
		bit <<= 1;
	}
	
	if (divider & 0x80000000)
	{
		if (remainder >= divider)
		{
			quotient |= bit;
			remainder -= divider;
		}
		divider >>= 1;
		bit >>= 1;
	}
	
	while (bit && remainder)
	{
		if (remainder >= divider)
		{
			quotient |= bit;
			remainder -= divider;
		}
		remainder <<= 1;
		bit >>= 1;
	}	 
			
	fixed_t result = quotient;

	if ((a ^ b) & 0x80000000) result = -result;
	
	return result;
}

/* multiplication, see MIT license at top of file. Modified from
   https://github.com/Yveaux/Arduino_fixpt/blob/master/fix16.c  */
fixed_t mulfix(
	fixed_t a,
	fixed_t b
)
{
	int32_t A = (a >> 16), C = (b >> 16);
	uint32_t B = (a & 0xFFFF), D = (b & 0xFFFF);
	
	int32_t AC = A*C;
	int32_t AD_CB = A*D + C*B;
	uint32_t BD = B*D;
	
	int32_t product_hi = AC + (AD_CB >> 16);
	
	// Handle carry from lower 32 bits to upper part of result.
	uint32_t ad_cb_temp = AD_CB << 16;
	uint32_t product_lo = BD + ad_cb_temp;
	if (product_lo < BD)
		product_hi++;
	
	return (product_hi << 16) | (product_lo >> 16);
}

fixed_t int_to_fix(
	int32_t a
)
{
	return a << 16;
}

int32_t fix_to_int(
	fixed_t a
)
{
	return a >> 16;
}

fixed_t min(
	fixed_t a, fixed_t b, fixed_t c
)
{
	fixed_t min = c;
	if (a<min) min = a;
	if (b<min) min = b;
	return min;
}

fixed_t mul_vector3d_fix(
	vector3d &v,
	fixed_t b
)
{
	v.x = mulfix( v.x, b );
	v.y = mulfix( v.y, b );
	v.z = mulfix( v.z, b );
}

fixed_t mul_vector2d_fix(
	vector2d &v,
	fixed_t b
)
{
	v.x = mulfix( v.x, b );
	v.y = mulfix( v.y, b );
}

void
project_point_isometric(
	point3d &p3d,
	point2d &p2d
)
{
	p2d.x = addfix( p3d.x, p3d.z >> 2 );
	p2d.y = addfix( p3d.y, p3d.z >> 3 );
}

bool
is_null_triangle3d(
	triangle3d &t
)
{
	bool result = true;
	for (uint8_t i=0;i<3;i++) {
		if (t.points[i].x!=0) { result = false; }
		else if (t.points[i].y!=0) { result = false; }
		else if (t.points[i].z!=0) { result = false; }
	}
	return result;
}

void
set_null_triangle3d(
	triangle3d &t
)
{
	for (uint8_t i=0;i<3;i++) {
		t.points[i].x=0;
		t.points[i].y=0;
		t.points[i].z=0;
	}
}

void
project_triangle(
	triangle3d &t3d,
	triangle2d &t2d
)
{
	for (uint8_t i=0;i<3;i++) {
		project_point_isometric( t3d.points[i], t2d.points[i] );
	}
}

void draw_triangle2d(
	triangle2d &t2d
)
{
	moveto( fix_to_int(t2d.points[0].x), fix_to_int(t2d.points[0].y) );
	lineto( fix_to_int(t2d.points[1].x), fix_to_int(t2d.points[1].y) );
	lineto( fix_to_int(t2d.points[2].x), fix_to_int(t2d.points[2].y) );
	lineto( fix_to_int(t2d.points[0].x), fix_to_int(t2d.points[0].y) );
}


void clip_triangle2d(
	triangle2d &knife,
	triangle2d &tofu
)
{
	
}

/* Convert ascii decimal string to fixed point 32 bit signed integer.
 * 
 * http://cs.furman.edu/digitaldomain/more/ch6/dec_frac_to_bin.htm
 * http://stackoverflow.com/questions/4987176/how-do-you-convert-a-fraction-to-binary
 * https://www.youtube.com/watch?v=REeaT2mWj6Y (Wildberger, Inconvenient Truths about Square Root of 2)
 *
 * Input pointer 'c' should point to first ascii decimal digit.
 * This returns pointer past end of digits.
 *
 * Example:
 *
 * input "23.54 "
 *      c-^
 *
 * 0+2->2
 * 2*10->20
 * 3+20->23
 * integer part = 23
 *
 * 0.54 = 54/100
 * 54/100 = 54 * 2**16 / 100 * 2**16
 * 54/100 = ( (54 * 2**16) / 100 ) / 2**16
 * 54/100 = ( (54 << 16) / 100 ) >> 16
 * 
 * 0+5->5
 * 5*10->50
 * 4+50->54
 * 54->binary 0b110110
 * 100->binary 0b1100100
 * 0b1100110<<16 -> 0b1101100000000000000000
 * 0b1101100000000000000000 / 0b1100100 -> 0b1000101000111101
 * 0b1000101000111101 >> 16 = 0.1000101000111101, or, in python:
 * > 1.0/2+1.0/2**5+1.0/2**7+1.0/2**11+1.0/2**12+1.0/2**13+1.0/2**14+1.0/2**16
 * > 0.5399932861328125, roughly 0.54
 * (recall, information is lost in base 10->base 2 conversion)
 * (but we are just drawing low resolution pictures, so it's ok)
 *
 * Now...this is fixed point, so skip the last step of >>16.
 * fractional_part = 0b1000101000111101
 * integer_part = bin(23) = 0b10111
 * Q16.16 fixed point number = 0000000000010111.1000101000111101
 * or in decimal, 23.5399932861328125
 *
 * return c:
 * input "23.54 "
 *           c-^
 */
const char *
ascii_decimal_to_fixed_t(
	const char *c,
	fixed_t &fixed
)
{
	logf("ascii decimal parse begin");
	uint32_t tmp = 0;
	uint32_t tmp2 = 0;
	uint32_t digitcounter = 0;
	uint32_t fracpart;
	uint32_t intpart;
	int32_t sign = 1;
	if (*c=='-') {
		sign = -1;
		c++;
	}
	while (*c!=' ' && *c!='\n' && *c!='.')
	{
		uint32_t decdigit = *c-48;
		logf(" chars: %.10s",c); 
		tmp *= 10;
		tmp += decdigit;
		logf(" tmp: %i",tmp);
		c++;
	}
	if (*c=='.') c++;
	uint32_t dividend=1;
	while (*c!=' ' && *c!='\n' && *c!='.' && *c!='e')
	{
		uint32_t decdigit = *c-48;
		logf(" c2: %.10s",c);
		tmp2 *= 10;
		tmp2 += decdigit;
		dividend *= 10;
		logf(" tmp2: %i",tmp2);
		logf(" dividend: %i",dividend);
		c++;
	}
	/* 6.23e-16 -> 0 */
	if (*c=='e') {
		tmp = 0;
		tmp2 = 0;
	}
	/* 12.704020 -> 12.7040 */
	while (tmp2>65535) {
		tmp2/=10;
		dividend/=10;
	}
	c++;
	logf(" final tmp: %i",tmp);
	logf(" final tmp2: %i",tmp2);
	intpart = tmp<<16;
	fracpart = ( (tmp2<<16) / dividend );
	fixed = intpart | fracpart;	
	fixed = sign * fixed;
	logf(" intpart: %u",intpart);
	logf(" fracpart: %u",fracpart);
	logf(" fixed: %i",fixed);
	logf("ascii decimal parse end");
	return c;
}

void
parse_stl(
	const char * stl_base,
	triangle3d triangles[]
)
{
	fixed_t x,y,z;
	uint32_t tri_index = 0;
	const char *s = stl_base;
	/* this will crash, hang if STL is not properly formatted */
	while (*s && !(tri_index>=MAX_TRIANGLES))
	{
		triangle3d * newtri = &(triangles[tri_index]);
		for (uint8_t i=0;i<3;i++) {
			point3d *p = &(newtri->points[i]);
			while (*s && strncmp(s,"vertex ",7)) s++;
			const char * debugs = s;
			if (!*s) { break; } else { s+=7; }
			s = ascii_decimal_to_fixed_t(s,p->x);
			s = ascii_decimal_to_fixed_t(s,p->y);
			s = ascii_decimal_to_fixed_t(s,p->z);
			logf("v: %.40s",debugs); 
			logf("x y z %i %i %i",p->x,p->y,p->z);
		}
		if (!is_null_triangle3d(triangles[tri_index])) tri_index++;
	}
	set_null_triangle3d( triangles[tri_index] );
}

/* extend the given 3d box so it contains the point p */
void
extend_box3d(
	box3d &box,
	point3d &p
)
{
	box.max.x = p.x>box.max.x ? p.x : box.max.x;
	box.max.y = p.y>box.max.y ? p.y : box.max.y;
	box.max.z = p.z>box.max.z ? p.z : box.max.z;
	box.min.x = p.x<box.min.x ? p.x : box.min.x;
	box.min.y = p.y<box.min.y ? p.y : box.min.y;
	box.min.z = p.z<box.min.z ? p.z : box.min.z;
}

void
find_bounding_box(
	triangle3d triangles[],
	box3d &bbox
)
{
	bbox.max = bbox.min = triangles[0].points[0];
	for (triangle3d *t=triangles;!is_null_triangle3d(*t);t++)
	{
		for (point3d *p=t->points; p - t->points < 3 ;p++)
		{
			extend_box3d( bbox, *p );
		}
	}
}

void
find_dimensions_box3d(
	box3d &box,
	vector3d &dimensions
)
{
	dimensions.x = subfix( box.max.x, box.min.x );
	dimensions.y = subfix( box.max.y, box.min.y );
	dimensions.z = subfix( box.max.z, box.min.z );
}

void
find_dimensions_box2d(
	box2d &box,
	vector2d &dimensions
)
{
	dimensions.x = subfix( box.max.x, box.min.x );
	dimensions.y = subfix( box.max.y, box.min.y );
}

void
find_center_box3d(
	box3d &box,
	vector3d &center
)
{
	vector3d dims;
	find_dimensions_box3d( box, dims ); 
	center.x = addfix( box.min.x, divfix( dims.x, int_to_fix( 2 ) ) );
	center.y = addfix( box.min.y, divfix( dims.y, int_to_fix( 2 ) ) );
	center.z = addfix( box.min.z, divfix( dims.z, int_to_fix( 2 ) ) );
	//logf("xdd ydd zdd %i %i %i",xdd,ydd,zdd);
	//logf("cx cy cz %i %i %i",center.x,center.y,center.z);
}

void
find_center_box2d(box2d &box, vector2d &center)
{
	vector2d dims;
	find_dimensions_box2d( box, dims ); 
	center.x = addfix( box.min.x, divfix( dims.x, int_to_fix( 2 ) ) );
	center.y = addfix( box.min.y, divfix( dims.y, int_to_fix( 2 ) ) );
}

void
translate_point3d(
	point3d &p,
	vector3d &translater
)
{
	p.x = addfix( p.x, translater.x );
	p.y = addfix( p.y, translater.y );
	p.z = addfix( p.z, translater.z );
}

void
translate_point2d(
	point2d &p,
	vector2d &translater
)
{
	p.x = addfix( p.x, translater.x );
	p.y = addfix( p.y, translater.y );
}


void
scale_point3d(
	vector3d &scaler,
	point3d &p
)
{
	p.x = mulfix( p.x, scaler.x );
	p.y = mulfix( p.y, scaler.y );
	p.z = mulfix( p.z, scaler.z );
}

void
translate_triangles3d(
	triangle3d triangles[],
	vector3d &translater
)
{
	for (triangle3d *t=triangles;!is_null_triangle3d(*t);t++) {
		for (point3d *p=t->points;p-t->points<3;p++) {
			translate_point3d( *p, translater );
		}
	}
}

/* find the ratio between the object bounding box and the
   draw box. assumes both boxes centered. assumes drawbox is a cube.   
   ____ 
   |  |           ____
   |  |    ->     |  |     -> scaler =   1/2
   |  |           |__|
   |__|       
                 ______
    ___         |      |   -> scaler =   3
    |_|    ->   |      |
                |______|

   result is divided by 1.618 to allow for rotation. How? First off,
   1.618 is approx the golden ratio, for aesthetics. Also, it works well because
   1+1/g.r. = g.r., which means 1/1.618 = 1.618 - 1 = 0.618
   So in Q16.16 0.618 decimal is, by python ((618 * 2**16)/1000), or 0x00009e35
*/
void
find_scaler(
	box3d &bbox,
	box3d &drawbox,
	vector3d &scaler
)
{
	vector3d dimsbb, dimsdb;
	find_dimensions_box3d( bbox, dimsbb );
	find_dimensions_box3d( drawbox, dimsdb );
	scaler.x = divfix( dimsdb.x, dimsbb.x );
	scaler.y = divfix( dimsdb.y, dimsbb.y );
	scaler.z = divfix( dimsdb.z, dimsbb.z );
	fixed_t minscale = scaler.z = min( scaler.x, scaler.y, scaler.z );
	scaler.x = scaler.y = scaler.z = mulfix( minscale, 0x00009e35 );
}

void
scale_triangles(
	triangle3d triangles[],
	vector3d scaler
)
{
	for (triangle3d *t=triangles;!is_null_triangle3d(*t);t++) {
		for (point3d *p=t->points;p-t->points<3;p++) {
			scale_point3d( scaler, *p );
		}
	}
}

/* This is a quick and dirty version of "back face culling". But we are
using a fun trick. You see, you don't need 3d math to do culling, if you
can guarantee your points are ordered properly (which your 3d program
hopefully did, or which Meshlab can help you with maybe), then you can
just look at the 'winding' or 'orientation' of the points as they are
projected into 2d. Imagine three points on a 2d plane, like a computer screen.


    A
    .     B 
         .
   .  
   C

If they are listed ABC then that's "Clockwise" from the perspective of the 
"Observer" or "Beholder" who is looking at them from above. If they are listed
CBA then it's "CounterClockwise" or "AntiClockwise". But.... if the Beholder
moves to the other side of the screen somehow, the CW and CCW will be switched.

Another way to think of this is that, if you have consistently ordered 
triangles, the ones that appear Clockwise will be facing 'away' 
from the Beholder, and the CounterClockwise ones will be facing 'towards' the 
beholder. But we are still talking about a 2d plane right? 

Ahh the trick is, that this doesn't matter. If you project the 3d points 
of, say, a Tetrahedron (4 sided pyramid) down from 3d into 2d, the 
Orientation of the triangles will still tell you whether they are facing 
"towards" you, the Beholder, or Away. Towards doesn't necessarily mean 
directly at you, it could be in your general direction. As long as you 
can see their front face, even at an angle, it's still considered 
frontward facing. The point is that there will be "backward facing" 
triangles, and "forward facing triangles". You don't want to draw the 
ones that are "backwards facing"... because it turns out you can't see 
them if the object is supposed to be solid!

Let's look at an example. ABCD is a Tetrahedron... but we can only see
one face, ABC. How does the program know? When we go to draw, the points
are listed like so. ABD (clockwise), CDB (clockwise), ADC (clockwise), and
CBA (CounterClockwise). 

    A
    .      
      .  .B
   .  D
   C

CBA is the only triangle we need to draw!!!

But how do we do the math on that? We use something called the "Wedge" 
function (and many many other names). Basically it works like this. 
Consider two 2d vectors, v1 and v2. (a vector can be imagined as a line 
segment with two endpoints, head and tail). Imagine both vectors with 
their 'tail' at origin 0,0, and the first with 'head' coordinates x1,y1, 
the second with coordinates x2,y2.

Wedge function is x1*y2 - x2*y1. The result of this function tells you whether
x1,y1 is "counterclockwise" or "clockwise" from the other.
              
        x1,y1 (v1)
       /
      /   
    0,0 ....... x2,y2  (v2)

 x1,y1 = 3,4  x2,y2 = 5,0
 x1y2 = 15, x2y1 = 20, wedge = 15-20 = -5, so v1 is 'counterclockwise' from v2.
 If you switch the order, you get this.

        x2,y2 (v2)
       /
      /   
    0,0 ....... x1,y1  (v1)

 x2,y2 = 3,4  x1,y1 = 5,0
 x1y2 = 20, x2y1 = 15, wedge = 20-15 = 5, so v1 is 'clockwise' from v2.

The only thing that matters when finding Orientation is the sign of wedge.
If you are wondering what the Magnitude of wedge means... why.. it's the 
area of the parallellogram formed by v1,v2 if you add them tail to head. 
"Signed Area of a Bivector". But that's another story for another time. 

        v2........v1+v2
       /         /
      /     5   /
    0,0.........v1

*/
bool
is_front_facing(
	triangle2d &triangle
)
{
	fixed_t x1 = subfix( triangle.points[0].x, triangle.points[1].x );
	fixed_t y1 = subfix( triangle.points[0].y, triangle.points[1].y );
	fixed_t x2 = subfix( triangle.points[1].x, triangle.points[2].x );
	fixed_t y2 = subfix( triangle.points[1].y, triangle.points[2].y );
	fixed_t wedge = mulfix( x1, y2 ) - mulfix( x2, y1 );
	if (wedge<0) return 0;
	return 1;
}

void
stl_draw_triangles3d(
	triangle3d triangles[],
	box2d &screen
)
{
	vector2d translater;
	find_center_box2d( screen, translater );
	for (triangle3d *t=triangles;!is_null_triangle3d(*t);t++) {
		triangle2d t2d;
		project_triangle( *t, t2d );
		if (is_front_facing( t2d )) {
			/*
			logf("trans %i %i \n", translater.x,translater.y );
			logf("p %i %i \n", t2d.points[0].x, t2d.points[0].y );
			logf("p %i %i \n", t2d.points[1].x, t2d.points[1].y );
			logf("p %i %i \n", t2d.points[2].x, t2d.points[2].y );
			*/
			translate_point2d( t2d.points[0], translater );
			translate_point2d( t2d.points[1], translater );
			translate_point2d( t2d.points[2], translater );
			/*
			logf("p %i %i \n", t2d.points[0].x, t2d.points[0].y );
			logf("p %i %i \n", t2d.points[1].x, t2d.points[1].y );
			logf("p %i %i \n", t2d.points[2].x, t2d.points[2].y );
			*/
			draw_triangle2d( t2d );
		}
	}
}

/* drawbox is a centered 3d cube, into which all the STL points will be scaled
   to fit before 2d projection. It has to be a little bit smaller than the
   screen dimensions to allow for rotation.

    stl 3d object
           ___
    ______/  /|         3d drawbox      2d screen
   /________/ /            ____         ______
   |       _|/            /___/|        |    |
   |      |/              |   ||        |    |
   |     |/               |___|/        |____| 
   |     |/                            
   |      |//|
   |________|/                        

 */
void
make_drawbox(
	box3d &drawbox,
	box2d &screen
)
{
	vector2d dims;
	find_dimensions_box2d( screen, dims );
	fixed_t mindim = min( dims.x, dims.y, dims.y );
	fixed_t half = divfix( mindim, int_to_fix( 2 ) );
	fixed_t neghalf = mulfix( half, int_to_fix( -1 ) );
	drawbox.max.x = drawbox.max.y = drawbox.max.z = half;
	drawbox.min.x = drawbox.min.y = drawbox.min.z = neghalf;
}

void
log_box3d(
	box3d &b
)
{
	logf("box3d: min %i %i %i ",b.min.x,b.min.y,b.min.z);
	logf("       max %i %i %i",b.max.x,b.max.y,b.max.z);
}

void
log_vector3d(
	vector3d &v
)
{
	logf("vector3d: x y z %i %i %i",v.x,v.y,v.z);
}

void
log_vector2d(
	vector2d &v
)
{
	logf("vector2d: x y %i %i",v.x,v.y);
}

void
log_point3d(
	point3d &p
)
{
	logf("point3d: x y z %i %i %i",p.x,p.y,p.z);
}

void
log_triangles3d(
	triangle3d triangles[]
)
{
	triangle3d *t;
	for (t=triangles;!is_null_triangle3d(*t);t++) {
		logf("triangle %lu",t-triangles);
		for (point3d *p=t->points;p-t->points<3;p++) {
			logf("%.12i %.12i %.12i", p->x, p->y, p->z );
		}
	}
	/* 3 points per triangle, 3 coords per point, 4 bytes per coord */
	logf("bytes for triangle3d data: %lu",(t-triangles)*3*3*4);
}

triangle3d triangles[MAX_TRIANGLES];

void
stl_setup_triangles3d(
	triangle3d triangles[],
	box2d &screen
)
{
	box3d bbox, drawbox;
	vector3d translater, scaler;
	parse_stl( stl_data, triangles );
	log_triangles3d( triangles );
	find_bounding_box( triangles, bbox );
	find_center_box3d( bbox, translater );	
	log_box3d( bbox );
	log_vector3d( translater );
	mul_vector3d_fix( translater, int_to_fix(-1) );
	log_vector3d( translater );
	translate_triangles3d( triangles, translater );
	translate_point3d( bbox.min, translater );
	translate_point3d( bbox.max, translater );
	logf("bounding box");
	log_box3d( bbox );
	make_drawbox( drawbox, screen );
	find_scaler( bbox, drawbox, scaler );
	logf("scaler");
	log_vector3d( scaler );
	logf("drawbox");
	log_box3d( drawbox );
	log_triangles3d( triangles );
	scale_triangles( triangles, scaler );
	logf("scaled:");
	log_triangles3d( triangles );
}

void
stl_draw(
	uint32_t width2d,
	uint32_t height2d
)
{
	rx_points = 0;
	if (timeStatus()!= timeSet) {
		draw_string("time invalid: unable to sync", 0, 0, 8);
	}

	box2d screen;
	screen.max.x = int_to_fix( width2d );
	screen.max.y = int_to_fix( height2d );
	screen.min.x = int_to_fix( 0 );
	screen.min.y = int_to_fix( 0 );

	stl_setup_triangles3d( triangles, screen );
	stl_draw_triangles3d( triangles, screen );

	num_points = rx_points;
	rx_points = 0;
}

const char * stl_data = R"(
solid OpenSCAD_Model
  facet normal -0.122647 0.577008 0.807478
    outer loop
      vertex -1.74858 16.6366 18.5786
      vertex -5.16932 15.9095 18.5786
      vertex -1.30661 12.4315 21.6506
    endloop
  endfacet
  facet normal -0.740888 -0.0778701 0.667099
    outer loop
      vertex -16.7283 0 18.5786
      vertex -20.2254 0 14.6946
      vertex -16.3627 -3.478 18.5786
    endloop
  endfacet
  facet normal -0.708508 0.230208 -0.667099
    outer loop
      vertex -16.3627 3.478 -18.5786
      vertex -19.7835 4.2051 -14.6946
      vertex -18.4768 8.22642 -14.6946
    endloop
  endfacet
  facet normal -0.644463 -0.580277 0.497942
    outer loop
      vertex -16.3627 -11.8882 14.6946
      vertex -18.4768 -13.4242 10.1684
      vertex -13.5335 -15.0304 14.6946
    endloop
  endfacet
  facet normal 0.43838 0.394719 0.807478
    outer loop
      vertex 11.1934 12.4315 18.5786
      vertex 10.1127 7.34731 21.6506
      vertex 13.5335 9.83263 18.5786
    endloop
  endfacet
  facet normal -0.861333 -0.49729 0.103962
    outer loop
      vertex -21.4306 -11.7212 4.23778
      vertex -19.7835 -14.3735 5.19779
      vertex -21.5615 -11.2938 5.19779
    endloop
  endfacet
  facet normal -0.861333 -0.497289 0.103966
    outer loop
      vertex -21.3795 -11.8882 3.86271
      vertex -19.7835 -14.3735 5.19779
      vertex -21.4306 -11.7212 4.23778
    endloop
  endfacet
  facet normal -0.861333 -0.497291 0.103961
    outer loop
      vertex -19.7835 -14.3735 5.19779
      vertex -21.3795 -11.8882 3.86271
      vertex -21.3767 -12.1806 2.48718
    endloop
  endfacet
  facet normal -0.739118 0.665505 -0.103962
    outer loop
      vertex -19.7835 14.3735 -5.19779
      vertex -20.2254 14.6946 0
      vertex -16.3627 18.1726 -5.19779
    endloop
  endfacet
  facet normal 0.437882 0.602693 0.667099
    outer loop
      vertex 10.1127 17.5157 14.6946
      vertex 8.36413 14.4871 18.5786
      vertex 13.5335 15.0304 14.6946
    endloop
  endfacet
  facet normal 0.510867 -0.294949 -0.807478
    outer loop
      vertex 13.5335 -9.83263 -18.5786
      vertex 11.4193 -5.08421 -21.6506
      vertex 15.282 -6.804 -18.5786
    endloop
  endfacet
  facet normal 0.559309 -0.769822 0.307485
    outer loop
      vertex 15.282 -16.9724 10.1684
      vertex 11.4193 -19.7788 10.1684
      vertex 16.3627 -18.1726 5.19779
    endloop
  endfacet
  facet normal 0.82407 0.475774 0.307486
    outer loop
      vertex 18.4768 13.4242 10.1684
      vertex 21.4547 10.5846 6.58134
      vertex 21.444 10.7525 6.35017
    endloop
  endfacet
  facet normal 0.824069 0.475777 0.307484
    outer loop
      vertex 18.4768 13.4242 10.1684
      vertex 21.5188 9.58077 7.96297
      vertex 21.4547 10.5846 6.58134
    endloop
  endfacet
  facet normal 0.824069 0.475776 0.307485
    outer loop
      vertex 21.5188 9.58077 7.96297
      vertex 18.4768 13.4242 10.1684
      vertex 20.8641 9.28931 10.1684
    endloop
  endfacet
  facet normal -0.510867 0.294949 -0.807478
    outer loop
      vertex -11.4193 5.08421 -21.6506
      vertex -13.5335 9.83263 -18.5786
      vertex -10.1127 7.34731 -21.6506
    endloop
  endfacet
  facet normal 0.352726 0.792236 -0.497942
    outer loop
      vertex 10.1127 17.5157 -14.6946
      vertex 6.25 19.2355 -14.6946
      vertex 11.4193 19.7788 -10.1684
    endloop
  endfacet
  facet normal 0 0.744969 -0.667099
    outer loop
      vertex 1.74858 16.6366 -18.5786
      vertex -2.11413 20.1146 -14.6946
      vertex 2.11413 20.1146 -14.6946
    endloop
  endfacet
  facet normal 0 0.744969 -0.667099
    outer loop
      vertex -2.11413 20.1146 -14.6946
      vertex 1.74858 16.6366 -18.5786
      vertex -1.74858 16.6366 -18.5786
    endloop
  endfacet
  facet normal 0.707142 -0.636713 -0.307485
    outer loop
      vertex 16.3627 -18.1726 -5.19779
      vertex 15.282 -16.9724 -10.1684
      vertex 18.4768 -13.4242 -10.1684
    endloop
  endfacet
  facet normal -0.740888 0.0778701 -0.667099
    outer loop
      vertex -16.7283 0 -18.5786
      vertex -20.2254 0 -14.6946
      vertex -16.3627 3.478 -18.5786
    endloop
  endfacet
  facet normal 0.824766 -0.267982 0.497942
    outer loop
      vertex 18.4768 -8.22642 14.6946
      vertex 21.5265 -7.25083 10.1684
      vertex 21.4855 -5.51743 11.1692
    endloop
  endfacet
  facet normal 0.824766 -0.267983 0.497941
    outer loop
      vertex 21.5265 -7.25083 10.1684
      vertex 18.4768 -8.22642 14.6946
      vertex 20.8641 -9.28931 10.1684
    endloop
  endfacet
  facet normal -0.740888 -0.0778701 -0.667099
    outer loop
      vertex -16.3627 -3.478 -18.5786
      vertex -20.2254 0 -14.6946
      vertex -16.7283 0 -18.5786
    endloop
  endfacet
  facet normal 0.561027 -0.182289 0.807478
    outer loop
      vertex 12.2268 -2.5989 21.6506
      vertex 11.4193 -5.08421 21.6506
      vertex 16.3627 -3.478 18.5786
    endloop
  endfacet
  facet normal 0.740888 -0.0778706 -0.667099
    outer loop
      vertex 19.7835 -4.2051 -14.6946
      vertex 16.3627 -3.478 -18.5786
      vertex 20.2254 0 -14.6946
    endloop
  endfacet
  facet normal -0.824069 -0.475776 -0.307485
    outer loop
      vertex -19.7835 -14.3735 -5.19779
      vertex -21.4827 -10.9914 -5.87692
      vertex -21.4394 -10.8253 -6.25
    endloop
  endfacet
  facet normal -0.824068 -0.475781 -0.307481
    outer loop
      vertex -18.4768 -13.4242 -10.1684
      vertex -21.4394 -10.8253 -6.25
      vertex -21.444 -10.7525 -6.35017
    endloop
  endfacet
  facet normal -0.824069 -0.475776 -0.307485
    outer loop
      vertex -19.7835 -14.3735 -5.19779
      vertex -21.4394 -10.8253 -6.25
      vertex -18.4768 -13.4242 -10.1684
    endloop
  endfacet
  facet normal -0.82407 -0.475776 -0.307484
    outer loop
      vertex -21.4827 -10.9914 -5.87692
      vertex -19.7835 -14.3735 -5.19779
      vertex -21.5615 -11.2938 -5.19779
    endloop
  endfacet
  facet normal 0.904981 0.294046 0.307484
    outer loop
      vertex 20.8641 9.28931 10.1684
      vertex 21.5141 7.34731 10.1127
      vertex 21.4936 8.42289 9.14426
    endloop
  endfacet
  facet normal 0.904981 0.294046 0.307483
    outer loop
      vertex 21.5141 7.34731 10.1127
      vertex 20.8641 9.28931 10.1684
      vertex 21.5265 7.25083 10.1684
    endloop
  endfacet
  facet normal 0.861333 0.49729 -0.103962
    outer loop
      vertex 21.3743 12.4315 -1.30661
      vertex 20.2254 14.6946 0
      vertex 21.532 12.4315 0
    endloop
  endfacet
  facet normal 0.861332 0.497291 -0.103963
    outer loop
      vertex 21.3767 12.1806 -2.48718
      vertex 20.2254 14.6946 0
      vertex 21.3743 12.4315 -1.30661
    endloop
  endfacet
  facet normal 0.861333 0.49729 -0.103962
    outer loop
      vertex 20.2254 14.6946 0
      vertex 21.3767 12.1806 -2.48718
      vertex 19.7835 14.3735 -5.19779
    endloop
  endfacet
  facet normal 0.180301 0.84826 -0.497942
    outer loop
      vertex 6.25 19.2355 -14.6946
      vertex 6.68623 21.6002 -10.5084
      vertex 7.04314 21.6765 -10.2491
    endloop
  endfacet
  facet normal 0.180303 0.84826 -0.497942
    outer loop
      vertex 6.68623 21.6002 -10.5084
      vertex 6.25 19.2355 -14.6946
      vertex 2.11413 20.1146 -14.6946
    endloop
  endfacet
  facet normal 0.645162 0.372485 0.667099
    outer loop
      vertex 18.4768 8.22642 14.6946
      vertex 13.5335 9.83263 18.5786
      vertex 15.282 6.804 18.5786
    endloop
  endfacet
  facet normal 0.352726 0.792236 -0.497942
    outer loop
      vertex 6.25 19.2355 -14.6946
      vertex 7.15415 21.6778 -10.1684
      vertex 11.4193 19.7788 -10.1684
    endloop
  endfacet
  facet normal 0.352724 0.792237 -0.497942
    outer loop
      vertex 7.15415 21.6778 -10.1684
      vertex 6.25 19.2355 -14.6946
      vertex 7.04314 21.6765 -10.2491
    endloop
  endfacet
  facet normal 0.708508 -0.230208 0.667099
    outer loop
      vertex 16.3627 -3.478 18.5786
      vertex 15.282 -6.804 18.5786
      vertex 18.4768 -8.22642 14.6946
    endloop
  endfacet
  facet normal 0.561027 0.182289 -0.807478
    outer loop
      vertex 16.3627 3.478 -18.5786
      vertex 11.4193 5.08421 -21.6506
      vertex 15.282 6.804 -18.5786
    endloop
  endfacet
  facet normal 0.86246 0.0906483 0.497941
    outer loop
      vertex 20.2254 0 14.6946
      vertex 21.4048 1.97347 12.2926
      vertex 19.7835 4.2051 14.6946
    endloop
  endfacet
  facet normal 0.86246 0.0906483 0.497941
    outer loop
      vertex 21.4048 1.97347 12.2926
      vertex 20.2254 0 14.6946
      vertex 21.4744 0.406491 12.4573
    endloop
  endfacet
  facet normal 0.86246 0.0906511 0.497941
    outer loop
      vertex 21.4744 0.406491 12.4573
      vertex 20.2254 0 14.6946
      vertex 21.4925 0 12.5
    endloop
  endfacet
  facet normal -0.346734 -0.477238 0.807478
    outer loop
      vertex -8.36413 -9.28931 21.6506
      vertex -11.1934 -12.4315 18.5786
      vertex -6.25 -10.8253 21.6506
    endloop
  endfacet
  facet normal -0.707142 0.636713 -0.307485
    outer loop
      vertex -18.4768 13.4242 -10.1684
      vertex -19.7835 14.3735 -5.19779
      vertex -16.3627 18.1726 -5.19779
    endloop
  endfacet
  facet normal -0.824069 -0.475779 0.307482
    outer loop
      vertex -21.4394 -10.8253 6.25
      vertex -18.4768 -13.4242 10.1684
      vertex -21.444 -10.7525 6.35017
    endloop
  endfacet
  facet normal -0.824069 -0.475776 0.307485
    outer loop
      vertex -19.7835 -14.3735 5.19779
      vertex -21.4394 -10.8253 6.25
      vertex -21.5615 -11.2938 5.19779
    endloop
  endfacet
  facet normal -0.824069 -0.475776 0.307485
    outer loop
      vertex -21.4394 -10.8253 6.25
      vertex -19.7835 -14.3735 5.19779
      vertex -18.4768 -13.4242 10.1684
    endloop
  endfacet
  facet normal 0.437882 0.602693 -0.667099
    outer loop
      vertex 11.1934 12.4315 -18.5786
      vertex 8.36413 14.4871 -18.5786
      vertex 13.5335 15.0304 -14.6946
    endloop
  endfacet
  facet normal 0.824069 -0.475777 -0.307484
    outer loop
      vertex 18.4768 -13.4242 -10.1684
      vertex 21.5188 -9.58077 -7.96296
      vertex 21.444 -10.7525 -6.35017
    endloop
  endfacet
  facet normal 0.824069 -0.475776 -0.307485
    outer loop
      vertex 21.5188 -9.58077 -7.96296
      vertex 18.4768 -13.4242 -10.1684
      vertex 20.8641 -9.28931 -10.1684
    endloop
  endfacet
  facet normal 0.824766 -0.267984 -0.497941
    outer loop
      vertex 21.4752 -5.08421 -11.4193
      vertex 19.7835 -4.2051 -14.6946
      vertex 21.5403 -4.57854 -11.5836
    endloop
  endfacet
  facet normal 0.824766 -0.267983 -0.497941
    outer loop
      vertex 18.4768 -8.22642 -14.6946
      vertex 21.4752 -5.08421 -11.4193
      vertex 21.4855 -5.51743 -11.1692
    endloop
  endfacet
  facet normal 0.824766 -0.267983 -0.497941
    outer loop
      vertex 21.4752 -5.08421 -11.4193
      vertex 18.4768 -8.22642 -14.6946
      vertex 19.7835 -4.2051 -14.6946
    endloop
  endfacet
  facet normal 0.751027 0.433605 0.497941
    outer loop
      vertex 18.4768 13.4242 10.1684
      vertex 18.4768 8.22642 14.6946
      vertex 20.8641 9.28931 10.1684
    endloop
  endfacet
  facet normal -0.751026 -0.433605 0.497942
    outer loop
      vertex -18.4768 -8.22642 14.6946
      vertex -18.4768 -13.4242 10.1684
      vertex -16.3627 -11.8882 14.6946
    endloop
  endfacet
  facet normal -0.904981 -0.294046 -0.307485
    outer loop
      vertex -20.8641 -9.28931 -10.1684
      vertex -21.4772 -9.28931 -8.36413
      vertex -21.4936 -8.42289 -9.14426
    endloop
  endfacet
  facet normal -0.90498 -0.294047 -0.307485
    outer loop
      vertex -21.4772 -9.28931 -8.36413
      vertex -20.8641 -9.28931 -10.1684
      vertex -21.5188 -9.58077 -7.96297
    endloop
  endfacet
  facet normal -0.303006 0.680563 -0.667099
    outer loop
      vertex -8.36413 14.4871 -18.5786
      vertex -10.1127 17.5157 -14.6946
      vertex -6.25 19.2355 -14.6946
    endloop
  endfacet
  facet normal 0.708508 -0.230208 -0.667099
    outer loop
      vertex 18.4768 -8.22642 -14.6946
      vertex 16.3627 -3.478 -18.5786
      vertex 19.7835 -4.2051 -14.6946
    endloop
  endfacet
  facet normal 0.5846 -0.804633 0.103962
    outer loop
      vertex 16.3627 -18.1726 5.19779
      vertex 12.5 -21.6506 0
      vertex 16.7283 -18.5786 0
    endloop
  endfacet
  facet normal -0.437882 0.602693 -0.667099
    outer loop
      vertex -8.36413 14.4871 -18.5786
      vertex -13.5335 15.0304 -14.6946
      vertex -10.1127 17.5157 -14.6946
    endloop
  endfacet
  facet normal -0.751026 0.433605 -0.497942
    outer loop
      vertex -16.3627 11.8882 -14.6946
      vertex -18.4768 8.22642 -14.6946
      vertex -18.4768 13.4242 -10.1684
    endloop
  endfacet
  facet normal -0.180303 0.84826 -0.497942
    outer loop
      vertex -6.25 19.2355 -14.6946
      vertex -6.68623 21.6002 -10.5084
      vertex -2.11413 20.1146 -14.6946
    endloop
  endfacet
  facet normal -0.180301 0.84826 -0.497942
    outer loop
      vertex -6.68623 21.6002 -10.5084
      vertex -6.25 19.2355 -14.6946
      vertex -7.04314 21.6765 -10.2491
    endloop
  endfacet
  facet normal 0.708508 -0.230208 0.667099
    outer loop
      vertex 19.7835 -4.2051 14.6946
      vertex 16.3627 -3.478 18.5786
      vertex 18.4768 -8.22642 14.6946
    endloop
  endfacet
  facet normal 0.707142 0.636713 -0.307485
    outer loop
      vertex 18.4768 13.4242 -10.1684
      vertex 16.3627 18.1726 -5.19779
      vertex 19.7835 14.3735 -5.19779
    endloop
  endfacet
  facet normal 0.509734 0.701588 0.497942
    outer loop
      vertex 11.4193 19.7788 10.1684
      vertex 10.1127 17.5157 14.6946
      vertex 13.5335 15.0304 14.6946
    endloop
  endfacet
  facet normal 0.740888 0.0778701 0.667099
    outer loop
      vertex 20.2254 0 14.6946
      vertex 16.3627 3.478 18.5786
      vertex 16.7283 0 18.5786
    endloop
  endfacet
  facet normal 0.739118 0.665505 0.103962
    outer loop
      vertex 20.2254 14.6946 0
      vertex 16.3627 18.1726 5.19779
      vertex 19.7835 14.3735 5.19779
    endloop
  endfacet
  facet normal 0.55362 0.498482 -0.667099
    outer loop
      vertex 13.5335 9.83263 -18.5786
      vertex 11.1934 12.4315 -18.5786
      vertex 13.5335 15.0304 -14.6946
    endloop
  endfacet
  facet normal 0.404532 0.908595 -0.103962
    outer loop
      vertex 12.2268 21.1775 -5.19779
      vertex 12.2268 21.4749 -2.5989
      vertex 12.5 21.6506 0
    endloop
  endfacet
  facet normal 0.404532 0.908595 -0.103962
    outer loop
      vertex 12.2268 21.1775 -5.19779
      vertex 11.4193 21.55 -5.08421
      vertex 12.2268 21.4749 -2.5989
    endloop
  endfacet
  facet normal 0.404532 0.908595 -0.103962
    outer loop
      vertex 11.4193 21.55 -5.08421
      vertex 12.2268 21.1775 -5.19779
      vertex 11.3537 21.5662 -5.19779
    endloop
  endfacet
  facet normal 0.387032 0.869287 0.307485
    outer loop
      vertex 11.4193 19.7788 10.1684
      vertex 10.1127 21.3585 7.34731
      vertex 9.63732 21.3834 7.8753
    endloop
  endfacet
  facet normal 0.387032 0.869287 0.307485
    outer loop
      vertex 12.2268 21.1775 5.19779
      vertex 10.1127 21.3585 7.34731
      vertex 11.4193 19.7788 10.1684
    endloop
  endfacet
  facet normal 0.387031 0.869287 0.307484
    outer loop
      vertex 10.1127 21.3585 7.34731
      vertex 12.2268 21.1775 5.19779
      vertex 11.3537 21.5662 5.19779
    endloop
  endfacet
  facet normal -0.43838 -0.394719 0.807478
    outer loop
      vertex -10.1127 -7.34731 21.6506
      vertex -13.5335 -9.83263 18.5786
      vertex -11.1934 -12.4315 18.5786
    endloop
  endfacet
  facet normal -0.751027 -0.433605 0.497941
    outer loop
      vertex -18.4768 -8.22642 14.6946
      vertex -20.8641 -9.28931 10.1684
      vertex -18.4768 -13.4242 10.1684
    endloop
  endfacet
  facet normal -0.510867 -0.294949 0.807478
    outer loop
      vertex -11.4193 -5.08421 21.6506
      vertex -13.5335 -9.83263 18.5786
      vertex -10.1127 -7.34731 21.6506
    endloop
  endfacet
  facet normal -0.824069 -0.475777 0.307484
    outer loop
      vertex -18.4768 -13.4242 10.1684
      vertex -21.5188 -9.58077 7.96296
      vertex -21.444 -10.7525 6.35017
    endloop
  endfacet
  facet normal -0.824069 -0.475776 0.307485
    outer loop
      vertex -21.5188 -9.58077 7.96296
      vertex -18.4768 -13.4242 10.1684
      vertex -20.8641 -9.28931 10.1684
    endloop
  endfacet
  facet normal -0.740888 0.0778706 0.667099
    outer loop
      vertex -19.7835 4.2051 14.6946
      vertex -20.2254 0 14.6946
      vertex -16.3627 3.478 18.5786
    endloop
  endfacet
  facet normal -0.824766 -0.267984 0.497941
    outer loop
      vertex -21.4752 -5.08421 11.4193
      vertex -19.7835 -4.2051 14.6946
      vertex -21.5403 -4.57854 11.5836
    endloop
  endfacet
  facet normal -0.824766 -0.267983 0.497941
    outer loop
      vertex -18.4768 -8.22642 14.6946
      vertex -21.4752 -5.08421 11.4193
      vertex -21.4855 -5.51743 11.1692
    endloop
  endfacet
  facet normal -0.824766 -0.267983 0.497941
    outer loop
      vertex -21.4752 -5.08421 11.4193
      vertex -18.4768 -8.22642 14.6946
      vertex -19.7835 -4.2051 14.6946
    endloop
  endfacet
  facet normal -0.122647 -0.577008 -0.807478
    outer loop
      vertex -1.74858 -16.6366 -18.5786
      vertex -5.16932 -15.9095 -18.5786
      vertex -1.30661 -12.4315 -21.6506
    endloop
  endfacet
  facet normal 0.824069 -0.475779 -0.307482
    outer loop
      vertex 21.4394 -10.8253 -6.25
      vertex 18.4768 -13.4242 -10.1684
      vertex 21.444 -10.7525 -6.35017
    endloop
  endfacet
  facet normal 0.824069 -0.475776 -0.307485
    outer loop
      vertex 19.7835 -14.3735 -5.19779
      vertex 21.4394 -10.8253 -6.25
      vertex 21.5615 -11.2938 -5.19779
    endloop
  endfacet
  facet normal 0.824069 -0.475776 -0.307485
    outer loop
      vertex 21.4394 -10.8253 -6.25
      vertex 19.7835 -14.3735 -5.19779
      vertex 18.4768 -13.4242 -10.1684
    endloop
  endfacet
  facet normal 0.645162 -0.372485 -0.667099
    outer loop
      vertex 16.3627 -11.8882 -14.6946
      vertex 13.5335 -9.83263 -18.5786
      vertex 18.4768 -8.22642 -14.6946
    endloop
  endfacet
  facet normal 0 -0.589898 0.807478
    outer loop
      vertex -1.30661 -12.4315 21.6506
      vertex 1.74858 -16.6366 18.5786
      vertex 1.30661 -12.4315 21.6506
    endloop
  endfacet
  facet normal 0 -0.589898 0.807478
    outer loop
      vertex 1.74858 -16.6366 18.5786
      vertex -1.30661 -12.4315 21.6506
      vertex -1.74858 -16.6366 18.5786
    endloop
  endfacet
  facet normal 0.739118 -0.665505 0.103962
    outer loop
      vertex 19.7835 -14.3735 5.19779
      vertex 16.3627 -18.1726 5.19779
      vertex 20.2254 -14.6946 0
    endloop
  endfacet
  facet normal 0.510867 0.294949 0.807478
    outer loop
      vertex 13.5335 9.83263 18.5786
      vertex 10.1127 7.34731 21.6506
      vertex 11.4193 5.08421 21.6506
    endloop
  endfacet
  facet normal 0.751026 -0.433605 0.497942
    outer loop
      vertex 18.4768 -8.22642 14.6946
      vertex 16.3627 -11.8882 14.6946
      vertex 18.4768 -13.4242 10.1684
    endloop
  endfacet
  facet normal -0.861333 -0.49729 -0.103962
    outer loop
      vertex -20.2254 -14.6946 0
      vertex -21.3767 -12.1806 -2.48718
      vertex -19.7835 -14.3735 -5.19779
    endloop
  endfacet
  facet normal -0.861332 -0.497291 -0.103963
    outer loop
      vertex -21.3767 -12.1806 -2.48718
      vertex -20.2254 -14.6946 0
      vertex -21.3743 -12.4315 -1.30661
    endloop
  endfacet
  facet normal -0.861333 -0.49729 -0.103962
    outer loop
      vertex -21.3743 -12.4315 -1.30661
      vertex -20.2254 -14.6946 0
      vertex -21.532 -12.4315 0
    endloop
  endfacet
  facet normal -0.861333 -0.49729 0.103962
    outer loop
      vertex -20.2254 -14.6946 0
      vertex -21.3743 -12.4315 1.30661
      vertex -21.532 -12.4315 0
    endloop
  endfacet
  facet normal -0.861333 -0.497291 0.103962
    outer loop
      vertex -21.3743 -12.4315 1.30661
      vertex -20.2254 -14.6946 0
      vertex -19.7835 -14.3735 5.19779
    endloop
  endfacet
  facet normal -0.861333 -0.49729 0.103963
    outer loop
      vertex -21.3767 -12.1806 2.48718
      vertex -21.3743 -12.4315 1.30661
      vertex -19.7835 -14.3735 5.19779
    endloop
  endfacet
  facet normal -0.86246 -0.0906511 -0.497941
    outer loop
      vertex -20.2254 0 -14.6946
      vertex -21.4744 -0.406491 -12.4573
      vertex -21.4925 0 -12.5
    endloop
  endfacet
  facet normal -0.86246 -0.0906483 -0.497941
    outer loop
      vertex -20.2254 0 -14.6946
      vertex -21.4048 -1.97347 -12.2926
      vertex -21.4744 -0.406491 -12.4573
    endloop
  endfacet
  facet normal -0.86246 -0.0906483 -0.497941
    outer loop
      vertex -21.4048 -1.97347 -12.2926
      vertex -20.2254 0 -14.6946
      vertex -19.7835 -4.2051 -14.6946
    endloop
  endfacet
  facet normal -0.5846 -0.804633 -0.103962
    outer loop
      vertex -12.5 -21.6506 0
      vertex -16.3627 -18.1726 -5.19779
      vertex -12.2268 -21.1775 -5.19779
    endloop
  endfacet
  facet normal -0.303006 0.680563 -0.667099
    outer loop
      vertex -5.16932 15.9095 -18.5786
      vertex -8.36413 14.4871 -18.5786
      vertex -6.25 19.2355 -14.6946
    endloop
  endfacet
  facet normal -0.586667 0.0616609 -0.807478
    outer loop
      vertex -12.2268 2.5989 -21.6506
      vertex -16.7283 0 -18.5786
      vertex -16.3627 3.478 -18.5786
    endloop
  endfacet
  facet normal -0.561027 0.182289 -0.807478
    outer loop
      vertex -11.4193 5.08421 -21.6506
      vertex -16.3627 3.478 -18.5786
      vertex -15.282 6.804 -18.5786
    endloop
  endfacet
  facet normal -0.239933 0.538899 -0.807478
    outer loop
      vertex -3.86271 11.8882 -21.6506
      vertex -6.25 10.8253 -21.6506
      vertex -5.16932 15.9095 -18.5786
    endloop
  endfacet
  facet normal 0.239933 0.538899 -0.807478
    outer loop
      vertex 6.25 10.8253 -21.6506
      vertex 5.16932 15.9095 -18.5786
      vertex 8.36413 14.4871 -18.5786
    endloop
  endfacet
  facet normal 0.645162 0.372485 -0.667099
    outer loop
      vertex 15.282 6.804 -18.5786
      vertex 13.5335 9.83263 -18.5786
      vertex 18.4768 8.22642 -14.6946
    endloop
  endfacet
  facet normal 0.708508 0.230208 -0.667099
    outer loop
      vertex 16.3627 3.478 -18.5786
      vertex 15.282 6.804 -18.5786
      vertex 18.4768 8.22642 -14.6946
    endloop
  endfacet
  facet normal 0.740888 0.0778706 0.667099
    outer loop
      vertex 19.7835 4.2051 14.6946
      vertex 16.3627 3.478 18.5786
      vertex 20.2254 0 14.6946
    endloop
  endfacet
  facet normal 0 -0.867211 0.497942
    outer loop
      vertex -2.11413 -20.1146 14.6946
      vertex -1.30661 -21.4141 12.4315
      vertex 2.11413 -20.1146 14.6946
    endloop
  endfacet
  facet normal -5.36664e-07 -0.867211 0.497941
    outer loop
      vertex -1.30661 -21.4141 12.4315
      vertex -2.11413 -20.1146 14.6946
      vertex -2.26298 -21.5308 12.2282
    endloop
  endfacet
  facet normal 0 -0.867211 0.497942
    outer loop
      vertex 1.30661 -21.4141 12.4315
      vertex 2.11413 -20.1146 14.6946
      vertex -1.30661 -21.4141 12.4315
    endloop
  endfacet
  facet normal 5.36664e-07 -0.867211 0.497941
    outer loop
      vertex 2.11413 -20.1146 14.6946
      vertex 1.30661 -21.4141 12.4315
      vertex 2.26298 -21.5308 12.2282
    endloop
  endfacet
  facet normal 0.740888 -0.0778701 0.667099
    outer loop
      vertex 16.7283 0 18.5786
      vertex 16.3627 -3.478 18.5786
      vertex 20.2254 0 14.6946
    endloop
  endfacet
  facet normal 0.708508 0.230208 0.667099
    outer loop
      vertex 18.4768 8.22642 14.6946
      vertex 16.3627 3.478 18.5786
      vertex 19.7835 4.2051 14.6946
    endloop
  endfacet
  facet normal 0.510867 0.294949 0.807478
    outer loop
      vertex 13.5335 9.83263 18.5786
      vertex 11.4193 5.08421 21.6506
      vertex 15.282 6.804 18.5786
    endloop
  endfacet
  facet normal 0.751026 0.433605 0.497942
    outer loop
      vertex 18.4768 13.4242 10.1684
      vertex 16.3627 11.8882 14.6946
      vertex 18.4768 8.22642 14.6946
    endloop
  endfacet
  facet normal 0.707142 0.636713 0.307485
    outer loop
      vertex 16.3627 18.1726 5.19779
      vertex 15.282 16.9724 10.1684
      vertex 18.4768 13.4242 10.1684
    endloop
  endfacet
  facet normal 0.740888 0.0778706 -0.667099
    outer loop
      vertex 20.2254 0 -14.6946
      vertex 16.3627 3.478 -18.5786
      vertex 19.7835 4.2051 -14.6946
    endloop
  endfacet
  facet normal 0.559309 0.769823 -0.307485
    outer loop
      vertex 16.3627 18.1726 -5.19779
      vertex 11.4193 19.7788 -10.1684
      vertex 12.2268 21.1775 -5.19779
    endloop
  endfacet
  facet normal 0.509734 0.701588 -0.497942
    outer loop
      vertex 13.5335 15.0304 -14.6946
      vertex 11.4193 19.7788 -10.1684
      vertex 15.282 16.9724 -10.1684
    endloop
  endfacet
  facet normal 0.404532 0.908595 0.103962
    outer loop
      vertex 12.2268 21.1775 5.19779
      vertex 11.4193 21.55 5.08421
      vertex 11.3537 21.5662 5.19779
    endloop
  endfacet
  facet normal 0.404532 0.908595 0.103962
    outer loop
      vertex 12.2268 21.1775 5.19779
      vertex 12.2268 21.4749 2.5989
      vertex 11.4193 21.55 5.08421
    endloop
  endfacet
  facet normal 0.404532 0.908595 0.103962
    outer loop
      vertex 12.2268 21.4749 2.5989
      vertex 12.2268 21.1775 5.19779
      vertex 12.5 21.6506 0
    endloop
  endfacet
  facet normal 0.861333 0.497291 -0.103961
    outer loop
      vertex 21.3795 11.8882 -3.86271
      vertex 19.7835 14.3735 -5.19779
      vertex 21.3767 12.1806 -2.48718
    endloop
  endfacet
  facet normal 0.861333 0.497289 -0.103966
    outer loop
      vertex 21.4306 11.7212 -4.23778
      vertex 19.7835 14.3735 -5.19779
      vertex 21.3795 11.8882 -3.86271
    endloop
  endfacet
  facet normal 0.861333 0.49729 -0.103962
    outer loop
      vertex 19.7835 14.3735 -5.19779
      vertex 21.4306 11.7212 -4.23778
      vertex 21.5615 11.2938 -5.19779
    endloop
  endfacet
  facet normal -0.346733 0.477238 0.807478
    outer loop
      vertex -8.36413 14.4871 18.5786
      vertex -11.1934 12.4315 18.5786
      vertex -6.25 10.8253 21.6506
    endloop
  endfacet
  facet normal 0.387032 0.869287 0.307485
    outer loop
      vertex 11.4193 19.7788 10.1684
      vertex 8.36413 21.4501 9.28931
      vertex 7.15415 21.6778 10.1684
    endloop
  endfacet
  facet normal 0.387032 0.869287 0.307485
    outer loop
      vertex 8.36413 21.4501 9.28931
      vertex 11.4193 19.7788 10.1684
      vertex 9.63732 21.3834 7.8753
    endloop
  endfacet
  facet normal -0.55362 0.498482 0.667099
    outer loop
      vertex -13.5335 15.0304 14.6946
      vertex -16.3627 11.8882 14.6946
      vertex -13.5335 9.83263 18.5786
    endloop
  endfacet
  facet normal 0.861333 -0.49729 -0.103962
    outer loop
      vertex 20.2254 -14.6946 0
      vertex 21.3743 -12.4315 -1.30661
      vertex 21.532 -12.4315 0
    endloop
  endfacet
  facet normal 0.861333 -0.497291 -0.103962
    outer loop
      vertex 21.3743 -12.4315 -1.30661
      vertex 20.2254 -14.6946 0
      vertex 19.7835 -14.3735 -5.19779
    endloop
  endfacet
  facet normal 0.861333 -0.49729 -0.103963
    outer loop
      vertex 21.3767 -12.1806 -2.48718
      vertex 21.3743 -12.4315 -1.30661
      vertex 19.7835 -14.3735 -5.19779
    endloop
  endfacet
  facet normal -0.180301 -0.84826 -0.497942
    outer loop
      vertex -6.25 -19.2355 -14.6946
      vertex -6.68623 -21.6002 -10.5084
      vertex -7.04314 -21.6765 -10.2491
    endloop
  endfacet
  facet normal -0.180303 -0.84826 -0.497942
    outer loop
      vertex -6.68623 -21.6002 -10.5084
      vertex -6.25 -19.2355 -14.6946
      vertex -2.11413 -20.1146 -14.6946
    endloop
  endfacet
  facet normal -0.346734 -0.477238 0.807478
    outer loop
      vertex -6.25 -10.8253 21.6506
      vertex -11.1934 -12.4315 18.5786
      vertex -8.36413 -14.4871 18.5786
    endloop
  endfacet
  facet normal -0.404532 -0.908595 0.103962
    outer loop
      vertex -12.2268 -21.1775 5.19779
      vertex -11.4193 -21.55 5.08421
      vertex -11.3537 -21.5662 5.19779
    endloop
  endfacet
  facet normal -0.404532 -0.908595 0.103962
    outer loop
      vertex -12.2268 -21.1775 5.19779
      vertex -12.2268 -21.4749 2.5989
      vertex -11.4193 -21.55 5.08421
    endloop
  endfacet
  facet normal -0.404532 -0.908595 0.103962
    outer loop
      vertex -12.2268 -21.4749 2.5989
      vertex -12.2268 -21.1775 5.19779
      vertex -12.5 -21.6506 0
    endloop
  endfacet
  facet normal -0.303006 -0.680563 0.667099
    outer loop
      vertex -5.16932 -15.9095 18.5786
      vertex -8.36413 -14.4871 18.5786
      vertex -6.25 -19.2355 14.6946
    endloop
  endfacet
  facet normal -0.561027 -0.182289 0.807478
    outer loop
      vertex -12.2268 -2.5989 21.6506
      vertex -16.3627 -3.478 18.5786
      vertex -11.4193 -5.08421 21.6506
    endloop
  endfacet
  facet normal -0.55362 -0.498482 0.667099
    outer loop
      vertex -13.5335 -9.83263 18.5786
      vertex -13.5335 -15.0304 14.6946
      vertex -11.1934 -12.4315 18.5786
    endloop
  endfacet
  facet normal -0.708508 0.230208 0.667099
    outer loop
      vertex -18.4768 8.22642 14.6946
      vertex -19.7835 4.2051 14.6946
      vertex -16.3627 3.478 18.5786
    endloop
  endfacet
  facet normal -0.904981 -0.294046 0.307485
    outer loop
      vertex -21.4772 -9.28931 8.36413
      vertex -20.8641 -9.28931 10.1684
      vertex -21.4936 -8.42289 9.14426
    endloop
  endfacet
  facet normal -0.90498 -0.294049 0.307485
    outer loop
      vertex -21.5113 -9.52865 8.03471
      vertex -20.8641 -9.28931 10.1684
      vertex -21.4772 -9.28931 8.36413
    endloop
  endfacet
  facet normal -0.904981 -0.294044 0.307485
    outer loop
      vertex -20.8641 -9.28931 10.1684
      vertex -21.5113 -9.52865 8.03471
      vertex -21.5188 -9.58077 7.96296
    endloop
  endfacet
  facet normal -0.86246 -0.0906486 0.497941
    outer loop
      vertex -21.4048 -1.97347 12.2926
      vertex -20.2254 0 14.6946
      vertex -21.4925 0 12.5
    endloop
  endfacet
  facet normal -0.86246 -0.0906483 0.497941
    outer loop
      vertex -20.2254 0 14.6946
      vertex -21.4048 -1.97347 12.2926
      vertex -19.7835 -4.2051 14.6946
    endloop
  endfacet
  facet normal -0.645162 -0.372485 0.667099
    outer loop
      vertex -15.282 -6.804 18.5786
      vertex -18.4768 -8.22642 14.6946
      vertex -13.5335 -9.83263 18.5786
    endloop
  endfacet
  facet normal -0.739118 0.665505 0.103962
    outer loop
      vertex -16.3627 18.1726 5.19779
      vertex -20.2254 14.6946 0
      vertex -19.7835 14.3735 5.19779
    endloop
  endfacet
  facet normal -0.346734 0.477238 0.807478
    outer loop
      vertex -6.25 10.8253 21.6506
      vertex -11.1934 12.4315 18.5786
      vertex -8.36413 9.28931 21.6506
    endloop
  endfacet
  facet normal 0.645162 -0.372485 -0.667099
    outer loop
      vertex 18.4768 -8.22642 -14.6946
      vertex 13.5335 -9.83263 -18.5786
      vertex 15.282 -6.804 -18.5786
    endloop
  endfacet
  facet normal 0.55362 -0.498482 -0.667099
    outer loop
      vertex 16.3627 -11.8882 -14.6946
      vertex 13.5335 -15.0304 -14.6946
      vertex 13.5335 -9.83263 -18.5786
    endloop
  endfacet
  facet normal 0.707142 -0.636713 -0.307485
    outer loop
      vertex 19.7835 -14.3735 -5.19779
      vertex 16.3627 -18.1726 -5.19779
      vertex 18.4768 -13.4242 -10.1684
    endloop
  endfacet
  facet normal 0.707142 -0.636713 0.307485
    outer loop
      vertex 18.4768 -13.4242 10.1684
      vertex 16.3627 -18.1726 5.19779
      vertex 19.7835 -14.3735 5.19779
    endloop
  endfacet
  facet normal 0.861333 -0.49729 0.103962
    outer loop
      vertex 20.2254 -14.6946 0
      vertex 21.3767 -12.1806 2.48718
      vertex 19.7835 -14.3735 5.19779
    endloop
  endfacet
  facet normal 0.861332 -0.497291 0.103963
    outer loop
      vertex 21.3767 -12.1806 2.48718
      vertex 20.2254 -14.6946 0
      vertex 21.3743 -12.4315 1.30661
    endloop
  endfacet
  facet normal 0.861333 -0.49729 0.103962
    outer loop
      vertex 21.3743 -12.4315 1.30661
      vertex 20.2254 -14.6946 0
      vertex 21.532 -12.4315 0
    endloop
  endfacet
  facet normal 0.90498 0.294047 0.307485
    outer loop
      vertex 20.8641 9.28931 10.1684
      vertex 21.4772 9.28931 8.36413
      vertex 21.5188 9.58077 7.96297
    endloop
  endfacet
  facet normal 0.904981 0.294046 0.307485
    outer loop
      vertex 21.4772 9.28931 8.36413
      vertex 20.8641 9.28931 10.1684
      vertex 21.4936 8.42289 9.14426
    endloop
  endfacet
  facet normal 0.740888 -0.0778706 0.667099
    outer loop
      vertex 20.2254 0 14.6946
      vertex 16.3627 -3.478 18.5786
      vertex 19.7835 -4.2051 14.6946
    endloop
  endfacet
  facet normal 0.824766 -0.267984 0.497941
    outer loop
      vertex 19.7835 -4.2051 14.6946
      vertex 21.4752 -5.08421 11.4193
      vertex 21.5403 -4.57854 11.5836
    endloop
  endfacet
  facet normal 0.824766 -0.267983 0.497941
    outer loop
      vertex 18.4768 -8.22642 14.6946
      vertex 21.4752 -5.08421 11.4193
      vertex 19.7835 -4.2051 14.6946
    endloop
  endfacet
  facet normal 0.824766 -0.267983 0.497941
    outer loop
      vertex 21.4752 -5.08421 11.4193
      vertex 18.4768 -8.22642 14.6946
      vertex 21.4855 -5.51743 11.1692
    endloop
  endfacet
  facet normal 0.387032 -0.869287 0.307485
    outer loop
      vertex 10.1127 -21.3585 7.34731
      vertex 11.4193 -19.7788 10.1684
      vertex 9.63732 -21.3834 7.8753
    endloop
  endfacet
  facet normal 0.387031 -0.869287 0.307484
    outer loop
      vertex 12.2268 -21.1775 5.19779
      vertex 10.1127 -21.3585 7.34731
      vertex 11.3537 -21.5662 5.19779
    endloop
  endfacet
  facet normal 0.387032 -0.869287 0.307485
    outer loop
      vertex 10.1127 -21.3585 7.34731
      vertex 12.2268 -21.1775 5.19779
      vertex 11.4193 -19.7788 10.1684
    endloop
  endfacet
  facet normal -0.739118 -0.665505 -0.103962
    outer loop
      vertex -16.7283 -18.5786 0
      vertex -20.2254 -14.6946 0
      vertex -16.3627 -18.1726 -5.19779
    endloop
  endfacet
  facet normal 0.5846 -0.804633 -0.103962
    outer loop
      vertex 12.5 -21.6506 0
      vertex 12.2268 -21.1775 -5.19779
      vertex 16.3627 -18.1726 -5.19779
    endloop
  endfacet
  facet normal -0.824069 -0.475776 -0.307485
    outer loop
      vertex -18.4768 -13.4242 -10.1684
      vertex -21.5188 -9.58077 -7.96297
      vertex -20.8641 -9.28931 -10.1684
    endloop
  endfacet
  facet normal -0.824069 -0.475777 -0.307484
    outer loop
      vertex -21.5188 -9.58077 -7.96297
      vertex -18.4768 -13.4242 -10.1684
      vertex -21.4547 -10.5846 -6.58134
    endloop
  endfacet
  facet normal -0.82407 -0.475774 -0.307486
    outer loop
      vertex -21.4547 -10.5846 -6.58134
      vertex -18.4768 -13.4242 -10.1684
      vertex -21.444 -10.7525 -6.35017
    endloop
  endfacet
  facet normal -0.740888 -0.0778706 -0.667099
    outer loop
      vertex -19.7835 -4.2051 -14.6946
      vertex -20.2254 0 -14.6946
      vertex -16.3627 -3.478 -18.5786
    endloop
  endfacet
  facet normal -0.586667 -0.0616609 -0.807478
    outer loop
      vertex -16.3627 -3.478 -18.5786
      vertex -16.7283 0 -18.5786
      vertex -12.2268 -2.5989 -21.6506
    endloop
  endfacet
  facet normal -0.86246 0.0906481 -0.497941
    outer loop
      vertex -19.7835 4.2051 -14.6946
      vertex -21.377 2.5989 -12.2268
      vertex -21.5403 4.57854 -11.5836
    endloop
  endfacet
  facet normal -0.86246 0.0906477 -0.497942
    outer loop
      vertex -21.377 2.5989 -12.2268
      vertex -19.7835 4.2051 -14.6946
      vertex -21.4048 1.97347 -12.2926
    endloop
  endfacet
  facet normal -0.824766 0.267983 -0.497941
    outer loop
      vertex -18.4768 8.22642 -14.6946
      vertex -21.4752 5.08421 -11.4193
      vertex -21.4855 5.51743 -11.1692
    endloop
  endfacet
  facet normal -0.824766 0.267983 -0.497941
    outer loop
      vertex -19.7835 4.2051 -14.6946
      vertex -21.4752 5.08421 -11.4193
      vertex -18.4768 8.22642 -14.6946
    endloop
  endfacet
  facet normal -0.824766 0.267984 -0.497941
    outer loop
      vertex -21.4752 5.08421 -11.4193
      vertex -19.7835 4.2051 -14.6946
      vertex -21.5403 4.57854 -11.5836
    endloop
  endfacet
  facet normal -0.751026 -0.433605 -0.497942
    outer loop
      vertex -16.3627 -11.8882 -14.6946
      vertex -18.4768 -13.4242 -10.1684
      vertex -18.4768 -8.22642 -14.6946
    endloop
  endfacet
  facet normal -0.437882 0.602693 -0.667099
    outer loop
      vertex -11.1934 12.4315 -18.5786
      vertex -13.5335 15.0304 -14.6946
      vertex -8.36413 14.4871 -18.5786
    endloop
  endfacet
  facet normal -0.239933 0.538899 -0.807478
    outer loop
      vertex -6.25 10.8253 -21.6506
      vertex -8.36413 14.4871 -18.5786
      vertex -5.16932 15.9095 -18.5786
    endloop
  endfacet
  facet normal -0.154888 0.72869 -0.667099
    outer loop
      vertex -5.16932 15.9095 -18.5786
      vertex -6.25 19.2355 -14.6946
      vertex -2.11413 20.1146 -14.6946
    endloop
  endfacet
  facet normal 0.437882 -0.602693 -0.667099
    outer loop
      vertex 13.5335 -15.0304 -14.6946
      vertex 8.36413 -14.4871 -18.5786
      vertex 11.1934 -12.4315 -18.5786
    endloop
  endfacet
  facet normal -0.644463 -0.580277 -0.497942
    outer loop
      vertex -13.5335 -15.0304 -14.6946
      vertex -18.4768 -13.4242 -10.1684
      vertex -16.3627 -11.8882 -14.6946
    endloop
  endfacet
  facet normal -0.437882 -0.602693 -0.667099
    outer loop
      vertex -8.36413 -14.4871 -18.5786
      vertex -13.5335 -15.0304 -14.6946
      vertex -11.1934 -12.4315 -18.5786
    endloop
  endfacet
  facet normal -0.861333 0.497291 -0.103961
    outer loop
      vertex -19.7835 14.3735 -5.19779
      vertex -21.3795 11.8882 -3.86271
      vertex -21.3767 12.1806 -2.48718
    endloop
  endfacet
  facet normal -0.861333 0.49729 -0.103963
    outer loop
      vertex -21.3795 11.8882 -3.86271
      vertex -19.7835 14.3735 -5.19779
      vertex -21.5615 11.2938 -5.19779
    endloop
  endfacet
  facet normal -0.404534 0.908595 -0.103962
    outer loop
      vertex -12.2268 21.4749 -2.5989
      vertex -12.2268 21.1775 -5.19779
      vertex -12.5 21.6506 -1.81471e-05
    endloop
  endfacet
  facet normal -0.404532 0.908595 -0.103962
    outer loop
      vertex -11.4193 21.55 -5.08421
      vertex -12.2268 21.1775 -5.19779
      vertex -12.2268 21.4749 -2.5989
    endloop
  endfacet
  facet normal -0.404532 0.908595 -0.103962
    outer loop
      vertex -12.2268 21.1775 -5.19779
      vertex -11.4193 21.55 -5.08421
      vertex -11.3537 21.5662 -5.19779
    endloop
  endfacet
  facet normal 0.509734 0.701588 -0.497942
    outer loop
      vertex 13.5335 15.0304 -14.6946
      vertex 10.1127 17.5157 -14.6946
      vertex 11.4193 19.7788 -10.1684
    endloop
  endfacet
  facet normal 0.751026 0.433605 -0.497942
    outer loop
      vertex 18.4768 8.22642 -14.6946
      vertex 16.3627 11.8882 -14.6946
      vertex 18.4768 13.4242 -10.1684
    endloop
  endfacet
  facet normal 0.586667 -0.0616612 -0.807478
    outer loop
      vertex 16.7283 0 -18.5786
      vertex 12.2268 -2.5989 -21.6506
      vertex 12.5 0 -21.6506
    endloop
  endfacet
  facet normal 0.43838 0.394719 -0.807478
    outer loop
      vertex 13.5335 9.83263 -18.5786
      vertex 10.1127 7.34731 -21.6506
      vertex 11.1934 12.4315 -18.5786
    endloop
  endfacet
  facet normal 0.55362 0.498482 -0.667099
    outer loop
      vertex 16.3627 11.8882 -14.6946
      vertex 13.5335 9.83263 -18.5786
      vertex 13.5335 15.0304 -14.6946
    endloop
  endfacet
  facet normal 0.559309 0.769822 -0.307485
    outer loop
      vertex 15.282 16.9724 -10.1684
      vertex 11.4193 19.7788 -10.1684
      vertex 16.3627 18.1726 -5.19779
    endloop
  endfacet
  facet normal 0.751027 0.433605 -0.497941
    outer loop
      vertex 20.8641 9.28931 -10.1684
      vertex 18.4768 8.22642 -14.6946
      vertex 18.4768 13.4242 -10.1684
    endloop
  endfacet
  facet normal 0.824766 0.267983 0.497941
    outer loop
      vertex 18.4768 8.22642 14.6946
      vertex 21.5265 7.25083 10.1684
      vertex 20.8641 9.28931 10.1684
    endloop
  endfacet
  facet normal 0.824766 0.267982 0.497942
    outer loop
      vertex 21.5265 7.25083 10.1684
      vertex 18.4768 8.22642 14.6946
      vertex 21.4855 5.51743 11.1692
    endloop
  endfacet
  facet normal 0.586667 0.0616612 0.807478
    outer loop
      vertex 16.7283 0 18.5786
      vertex 12.2268 2.5989 21.6506
      vertex 12.5 0 21.6506
    endloop
  endfacet
  facet normal 0.644463 0.580277 0.497942
    outer loop
      vertex 18.4768 13.4242 10.1684
      vertex 13.5335 15.0304 14.6946
      vertex 16.3627 11.8882 14.6946
    endloop
  endfacet
  facet normal 0.645162 0.372485 0.667099
    outer loop
      vertex 16.3627 11.8882 14.6946
      vertex 13.5335 9.83263 18.5786
      vertex 18.4768 8.22642 14.6946
    endloop
  endfacet
  facet normal 0.559309 0.769822 0.307485
    outer loop
      vertex 16.3627 18.1726 5.19779
      vertex 11.4193 19.7788 10.1684
      vertex 15.282 16.9724 10.1684
    endloop
  endfacet
  facet normal 0 0.744969 0.667099
    outer loop
      vertex 2.11413 20.1146 14.6946
      vertex -1.74858 16.6366 18.5786
      vertex 1.74858 16.6366 18.5786
    endloop
  endfacet
  facet normal 0 0.744969 0.667099
    outer loop
      vertex -1.74858 16.6366 18.5786
      vertex 2.11413 20.1146 14.6946
      vertex -2.11413 20.1146 14.6946
    endloop
  endfacet
  facet normal 0.86246 0.0906486 -0.497941
    outer loop
      vertex 20.2254 0 -14.6946
      vertex 21.4048 1.97347 -12.2926
      vertex 21.4925 0 -12.5
    endloop
  endfacet
  facet normal 0.86246 0.0906483 -0.497941
    outer loop
      vertex 21.4048 1.97347 -12.2926
      vertex 20.2254 0 -14.6946
      vertex 19.7835 4.2051 -14.6946
    endloop
  endfacet
  facet normal 0.751027 -0.433605 -0.497941
    outer loop
      vertex 20.8641 -9.28931 -10.1684
      vertex 18.4768 -13.4242 -10.1684
      vertex 18.4768 -8.22642 -14.6946
    endloop
  endfacet
  facet normal 0.824766 -0.267982 -0.497942
    outer loop
      vertex 21.5265 -7.25083 -10.1684
      vertex 18.4768 -8.22642 -14.6946
      vertex 21.4855 -5.51743 -11.1692
    endloop
  endfacet
  facet normal 0.824766 -0.267983 -0.497941
    outer loop
      vertex 18.4768 -8.22642 -14.6946
      vertex 21.5265 -7.25083 -10.1684
      vertex 20.8641 -9.28931 -10.1684
    endloop
  endfacet
  facet normal 0.387032 0.869287 -0.307485
    outer loop
      vertex 11.4193 19.7788 -10.1684
      vertex 8.36413 21.4501 -9.28931
      vertex 9.63732 21.3834 -7.8753
    endloop
  endfacet
  facet normal 0.387032 0.869287 -0.307485
    outer loop
      vertex 8.36413 21.4501 -9.28931
      vertex 11.4193 19.7788 -10.1684
      vertex 7.15415 21.6778 -10.1684
    endloop
  endfacet
  facet normal 0.387031 0.869287 -0.307484
    outer loop
      vertex 12.2268 21.1775 -5.19779
      vertex 10.1127 21.3585 -7.34731
      vertex 11.3537 21.5662 -5.19779
    endloop
  endfacet
  facet normal 0.387032 0.869287 -0.307485
    outer loop
      vertex 11.4193 19.7788 -10.1684
      vertex 10.1127 21.3585 -7.34731
      vertex 12.2268 21.1775 -5.19779
    endloop
  endfacet
  facet normal 0.387032 0.869287 -0.307485
    outer loop
      vertex 10.1127 21.3585 -7.34731
      vertex 11.4193 19.7788 -10.1684
      vertex 9.63732 21.3834 -7.8753
    endloop
  endfacet
  facet normal 0.644463 0.580277 -0.497942
    outer loop
      vertex 18.4768 13.4242 -10.1684
      vertex 13.5335 15.0304 -14.6946
      vertex 15.282 16.9724 -10.1684
    endloop
  endfacet
  facet normal 0.707142 0.636713 -0.307485
    outer loop
      vertex 18.4768 13.4242 -10.1684
      vertex 15.282 16.9724 -10.1684
      vertex 16.3627 18.1726 -5.19779
    endloop
  endfacet
  facet normal 0.707142 0.636713 0.307485
    outer loop
      vertex 19.7835 14.3735 5.19779
      vertex 16.3627 18.1726 5.19779
      vertex 18.4768 13.4242 10.1684
    endloop
  endfacet
  facet normal 0.824069 0.475776 0.307485
    outer loop
      vertex 21.4394 10.8253 6.25
      vertex 19.7835 14.3735 5.19779
      vertex 18.4768 13.4242 10.1684
    endloop
  endfacet
  facet normal 0.82407 0.475776 0.307484
    outer loop
      vertex 19.7835 14.3735 5.19779
      vertex 21.4827 10.9914 5.87692
      vertex 21.5615 11.2938 5.19779
    endloop
  endfacet
  facet normal 0.824069 0.475776 0.307485
    outer loop
      vertex 19.7835 14.3735 5.19779
      vertex 21.4394 10.8253 6.25
      vertex 21.4827 10.9914 5.87692
    endloop
  endfacet
  facet normal 0.824068 0.475781 0.307481
    outer loop
      vertex 21.4394 10.8253 6.25
      vertex 18.4768 13.4242 10.1684
      vertex 21.444 10.7525 6.35017
    endloop
  endfacet
  facet normal 0.559309 0.769823 0.307485
    outer loop
      vertex 12.2268 21.1775 5.19779
      vertex 11.4193 19.7788 10.1684
      vertex 16.3627 18.1726 5.19779
    endloop
  endfacet
  facet normal 0.739118 0.665505 0.103962
    outer loop
      vertex 16.7283 18.5786 0
      vertex 16.3627 18.1726 5.19779
      vertex 20.2254 14.6946 0
    endloop
  endfacet
  facet normal 0.180303 0.84826 0.497941
    outer loop
      vertex 2.11413 20.1146 14.6946
      vertex 3.86271 21.3904 11.8882
      vertex 2.26298 21.5308 12.2282
    endloop
  endfacet
  facet normal 0.180303 0.84826 0.497941
    outer loop
      vertex 2.11413 20.1146 14.6946
      vertex 6.25 21.5069 10.8253
      vertex 3.86271 21.3904 11.8882
    endloop
  endfacet
  facet normal 0.180314 0.848253 0.49795
    outer loop
      vertex 6.25 21.5069 10.8253
      vertex 2.11413 20.1146 14.6946
      vertex 6.68623 21.6002 10.5084
    endloop
  endfacet
  facet normal -0.707142 0.636713 0.307485
    outer loop
      vertex -16.3627 18.1726 5.19779
      vertex -18.4768 13.4242 10.1684
      vertex -15.282 16.9724 10.1684
    endloop
  endfacet
  facet normal -0.86246 0.0906486 0.497941
    outer loop
      vertex -20.2254 0 14.6946
      vertex -21.4048 1.97347 12.2926
      vertex -21.4925 0 12.5
    endloop
  endfacet
  facet normal -0.86246 0.0906483 0.497941
    outer loop
      vertex -21.4048 1.97347 12.2926
      vertex -20.2254 0 14.6946
      vertex -19.7835 4.2051 14.6946
    endloop
  endfacet
  facet normal 0.239933 0.538899 0.807478
    outer loop
      vertex 8.36413 14.4871 18.5786
      vertex 5.16932 15.9095 18.5786
      vertex 6.25 10.8253 21.6506
    endloop
  endfacet
  facet normal 0.437882 0.602693 0.667099
    outer loop
      vertex 13.5335 15.0304 14.6946
      vertex 8.36413 14.4871 18.5786
      vertex 11.1934 12.4315 18.5786
    endloop
  endfacet
  facet normal 0.352726 0.792236 0.497942
    outer loop
      vertex 11.4193 19.7788 10.1684
      vertex 6.25 19.2355 14.6946
      vertex 10.1127 17.5157 14.6946
    endloop
  endfacet
  facet normal -0.352726 -0.792236 0.497942
    outer loop
      vertex -10.1127 -17.5157 14.6946
      vertex -11.4193 -19.7788 10.1684
      vertex -6.25 -19.2355 14.6946
    endloop
  endfacet
  facet normal -0.437882 -0.602693 0.667099
    outer loop
      vertex -8.36413 -14.4871 18.5786
      vertex -13.5335 -15.0304 14.6946
      vertex -10.1127 -17.5157 14.6946
    endloop
  endfacet
  facet normal -0.437882 -0.602693 0.667099
    outer loop
      vertex -11.1934 -12.4315 18.5786
      vertex -13.5335 -15.0304 14.6946
      vertex -8.36413 -14.4871 18.5786
    endloop
  endfacet
  facet normal -0.303006 -0.680563 0.667099
    outer loop
      vertex -8.36413 -14.4871 18.5786
      vertex -10.1127 -17.5157 14.6946
      vertex -6.25 -19.2355 14.6946
    endloop
  endfacet
  facet normal -0.510867 -0.294949 0.807478
    outer loop
      vertex -11.4193 -5.08421 21.6506
      vertex -15.282 -6.804 18.5786
      vertex -13.5335 -9.83263 18.5786
    endloop
  endfacet
  facet normal -0.740888 -0.0778706 0.667099
    outer loop
      vertex -16.3627 -3.478 18.5786
      vertex -20.2254 0 14.6946
      vertex -19.7835 -4.2051 14.6946
    endloop
  endfacet
  facet normal -0.861333 0.497291 0.103961
    outer loop
      vertex -21.3795 11.8882 3.86271
      vertex -19.7835 14.3735 5.19779
      vertex -21.3767 12.1806 2.48718
    endloop
  endfacet
  facet normal -0.861333 0.497289 0.103966
    outer loop
      vertex -21.4306 11.7212 4.23778
      vertex -19.7835 14.3735 5.19779
      vertex -21.3795 11.8882 3.86271
    endloop
  endfacet
  facet normal -0.861333 0.49729 0.103962
    outer loop
      vertex -19.7835 14.3735 5.19779
      vertex -21.4306 11.7212 4.23778
      vertex -21.5615 11.2938 5.19779
    endloop
  endfacet
  facet normal -0.645162 0.372485 0.667099
    outer loop
      vertex -13.5335 9.83263 18.5786
      vertex -18.4768 8.22642 14.6946
      vertex -15.282 6.804 18.5786
    endloop
  endfacet
  facet normal -0.586667 0.0616609 0.807478
    outer loop
      vertex -16.3627 3.478 18.5786
      vertex -16.7283 0 18.5786
      vertex -12.2268 2.5989 21.6506
    endloop
  endfacet
  facet normal -0.740888 0.0778701 0.667099
    outer loop
      vertex -16.3627 3.478 18.5786
      vertex -20.2254 0 14.6946
      vertex -16.7283 0 18.5786
    endloop
  endfacet
  facet normal -0.352726 0.792236 0.497942
    outer loop
      vertex -7.04314 21.6765 10.2491
      vertex -10.1127 17.5157 14.6946
      vertex -6.25 19.2355 14.6946
    endloop
  endfacet
  facet normal -0.352711 0.792236 0.497952
    outer loop
      vertex -10.1127 17.5157 14.6946
      vertex -7.04314 21.6765 10.2491
      vertex -7.08932 21.6771 10.2155
    endloop
  endfacet
  facet normal -0.239933 0.538899 0.807478
    outer loop
      vertex -5.16932 15.9095 18.5786
      vertex -6.25 10.8253 21.6506
      vertex -3.86271 11.8882 21.6506
    endloop
  endfacet
  facet normal 0.586667 0.0616609 -0.807478
    outer loop
      vertex 16.7283 0 -18.5786
      vertex 12.2268 2.5989 -21.6506
      vertex 16.3627 3.478 -18.5786
    endloop
  endfacet
  facet normal 0.586667 0.0616612 -0.807478
    outer loop
      vertex 12.5 0 -21.6506
      vertex 12.2268 2.5989 -21.6506
      vertex 16.7283 0 -18.5786
    endloop
  endfacet
  facet normal 0.122647 0.577007 -0.807478
    outer loop
      vertex 3.86271 11.8882 -21.6506
      vertex 1.30661 12.4315 -21.6506
      vertex 5.16932 15.9095 -18.5786
    endloop
  endfacet
  facet normal 0.239933 0.538899 -0.807478
    outer loop
      vertex 6.25 10.8253 -21.6506
      vertex 3.86271 11.8882 -21.6506
      vertex 5.16932 15.9095 -18.5786
    endloop
  endfacet
  facet normal 0.751026 -0.433605 -0.497942
    outer loop
      vertex 18.4768 -13.4242 -10.1684
      vertex 16.3627 -11.8882 -14.6946
      vertex 18.4768 -8.22642 -14.6946
    endloop
  endfacet
  facet normal 0.644463 -0.580277 -0.497942
    outer loop
      vertex 18.4768 -13.4242 -10.1684
      vertex 13.5335 -15.0304 -14.6946
      vertex 16.3627 -11.8882 -14.6946
    endloop
  endfacet
  facet normal 0.644463 -0.580277 -0.497942
    outer loop
      vertex 15.282 -16.9724 -10.1684
      vertex 13.5335 -15.0304 -14.6946
      vertex 18.4768 -13.4242 -10.1684
    endloop
  endfacet
  facet normal 0.387032 -0.869287 -0.307485
    outer loop
      vertex 8.36413 -21.4501 -9.28931
      vertex 11.4193 -19.7788 -10.1684
      vertex 9.63732 -21.3834 -7.8753
    endloop
  endfacet
  facet normal 0.387032 -0.869287 -0.307485
    outer loop
      vertex 11.4193 -19.7788 -10.1684
      vertex 8.36413 -21.4501 -9.28931
      vertex 7.15415 -21.6778 -10.1684
    endloop
  endfacet
  facet normal 0.824069 -0.475776 0.307485
    outer loop
      vertex 19.7835 -14.3735 5.19779
      vertex 21.4827 -10.9914 5.87692
      vertex 21.4394 -10.8253 6.25
    endloop
  endfacet
  facet normal 0.824068 -0.475781 0.307481
    outer loop
      vertex 18.4768 -13.4242 10.1684
      vertex 21.4394 -10.8253 6.25
      vertex 21.444 -10.7525 6.35017
    endloop
  endfacet
  facet normal 0.824069 -0.475776 0.307485
    outer loop
      vertex 19.7835 -14.3735 5.19779
      vertex 21.4394 -10.8253 6.25
      vertex 18.4768 -13.4242 10.1684
    endloop
  endfacet
  facet normal 0.82407 -0.475776 0.307484
    outer loop
      vertex 21.4827 -10.9914 5.87692
      vertex 19.7835 -14.3735 5.19779
      vertex 21.5615 -11.2938 5.19779
    endloop
  endfacet
  facet normal 0.861333 -0.497291 0.103961
    outer loop
      vertex 21.3795 -11.8882 3.86271
      vertex 19.7835 -14.3735 5.19779
      vertex 21.3767 -12.1806 2.48718
    endloop
  endfacet
  facet normal 0.861333 -0.49729 0.103963
    outer loop
      vertex 19.7835 -14.3735 5.19779
      vertex 21.3795 -11.8882 3.86271
      vertex 21.5615 -11.2938 5.19779
    endloop
  endfacet
  facet normal 0.861332 0.497291 0.103963
    outer loop
      vertex 20.2254 14.6946 0
      vertex 21.3767 12.1806 2.48718
      vertex 21.3743 12.4315 1.30661
    endloop
  endfacet
  facet normal 0.861333 0.49729 0.103962
    outer loop
      vertex 21.3767 12.1806 2.48718
      vertex 20.2254 14.6946 0
      vertex 19.7835 14.3735 5.19779
    endloop
  endfacet
  facet normal 0.861333 0.49729 0.103962
    outer loop
      vertex 20.2254 14.6946 0
      vertex 21.3743 12.4315 1.30661
      vertex 21.532 12.4315 0
    endloop
  endfacet
  facet normal 0.861333 0.497291 0.103961
    outer loop
      vertex 19.7835 14.3735 5.19779
      vertex 21.3795 11.8882 3.86271
      vertex 21.3767 12.1806 2.48718
    endloop
  endfacet
  facet normal 0.861333 0.49729 0.103963
    outer loop
      vertex 21.3795 11.8882 3.86271
      vertex 19.7835 14.3735 5.19779
      vertex 21.5615 11.2938 5.19779
    endloop
  endfacet
  facet normal 0.644463 0.580277 -0.497942
    outer loop
      vertex 16.3627 11.8882 -14.6946
      vertex 13.5335 15.0304 -14.6946
      vertex 18.4768 13.4242 -10.1684
    endloop
  endfacet
  facet normal 0.586667 -0.0616609 0.807478
    outer loop
      vertex 16.7283 0 18.5786
      vertex 12.2268 -2.5989 21.6506
      vertex 16.3627 -3.478 18.5786
    endloop
  endfacet
  facet normal 0.586667 -0.0616612 0.807478
    outer loop
      vertex 12.5 0 21.6506
      vertex 12.2268 -2.5989 21.6506
      vertex 16.7283 0 18.5786
    endloop
  endfacet
  facet normal 0.645162 -0.372485 0.667099
    outer loop
      vertex 18.4768 -8.22642 14.6946
      vertex 13.5335 -9.83263 18.5786
      vertex 16.3627 -11.8882 14.6946
    endloop
  endfacet
  facet normal 0.645162 -0.372485 0.667099
    outer loop
      vertex 15.282 -6.804 18.5786
      vertex 13.5335 -9.83263 18.5786
      vertex 18.4768 -8.22642 14.6946
    endloop
  endfacet
  facet normal -0.861333 -0.497291 -0.103961
    outer loop
      vertex -21.3795 -11.8882 -3.86271
      vertex -19.7835 -14.3735 -5.19779
      vertex -21.3767 -12.1806 -2.48718
    endloop
  endfacet
  facet normal -0.861333 -0.49729 -0.103963
    outer loop
      vertex -19.7835 -14.3735 -5.19779
      vertex -21.3795 -11.8882 -3.86271
      vertex -21.5615 -11.2938 -5.19779
    endloop
  endfacet
  facet normal -0.387031 -0.869287 0.307485
    outer loop
      vertex -11.4193 -19.7788 10.1684
      vertex -8.36413 -21.4501 9.28931
      vertex -7.15415 -21.6778 10.1684
    endloop
  endfacet
  facet normal -0.387032 -0.869287 0.307485
    outer loop
      vertex -8.36413 -21.4501 9.28931
      vertex -11.4193 -19.7788 10.1684
      vertex -9.63732 -21.3834 7.8753
    endloop
  endfacet
  facet normal -0.5846 -0.804633 -0.103962
    outer loop
      vertex -12.5 -21.6506 0
      vertex -16.7283 -18.5786 0
      vertex -16.3627 -18.1726 -5.19779
    endloop
  endfacet
  facet normal -0.303006 -0.680563 -0.667099
    outer loop
      vertex -6.25 -19.2355 -14.6946
      vertex -8.36413 -14.4871 -18.5786
      vertex -5.16932 -15.9095 -18.5786
    endloop
  endfacet
  facet normal -0.387032 -0.869287 -0.307485
    outer loop
      vertex -11.4193 -19.7788 -10.1684
      vertex -8.36413 -21.4501 -9.28931
      vertex -9.63732 -21.3834 -7.8753
    endloop
  endfacet
  facet normal -0.387032 -0.869287 -0.307485
    outer loop
      vertex -8.36413 -21.4501 -9.28931
      vertex -11.4193 -19.7788 -10.1684
      vertex -7.15415 -21.6778 -10.1684
    endloop
  endfacet
  facet normal -0.824766 -0.267982 -0.497942
    outer loop
      vertex -18.4768 -8.22642 -14.6946
      vertex -21.5265 -7.25083 -10.1684
      vertex -21.4855 -5.51743 -11.1692
    endloop
  endfacet
  facet normal -0.824766 -0.267983 -0.497941
    outer loop
      vertex -21.5265 -7.25083 -10.1684
      vertex -18.4768 -8.22642 -14.6946
      vertex -20.8641 -9.28931 -10.1684
    endloop
  endfacet
  facet normal -0.751027 -0.433605 -0.497941
    outer loop
      vertex -18.4768 -13.4242 -10.1684
      vertex -20.8641 -9.28931 -10.1684
      vertex -18.4768 -8.22642 -14.6946
    endloop
  endfacet
  facet normal -0.90498 0.294047 -0.307485
    outer loop
      vertex -20.8641 9.28931 -10.1684
      vertex -21.4772 9.28931 -8.36413
      vertex -21.5188 9.58077 -7.96297
    endloop
  endfacet
  facet normal -0.904981 0.294046 -0.307485
    outer loop
      vertex -21.4772 9.28931 -8.36413
      vertex -20.8641 9.28931 -10.1684
      vertex -21.4936 8.42289 -9.14426
    endloop
  endfacet
  facet normal -0.904981 -0.294046 -0.307484
    outer loop
      vertex -21.5141 -7.34731 -10.1127
      vertex -20.8641 -9.28931 -10.1684
      vertex -21.4936 -8.42289 -9.14426
    endloop
  endfacet
  facet normal -0.904981 -0.294046 -0.307483
    outer loop
      vertex -20.8641 -9.28931 -10.1684
      vertex -21.5141 -7.34731 -10.1127
      vertex -21.5265 -7.25083 -10.1684
    endloop
  endfacet
  facet normal -0.708508 -0.230208 -0.667099
    outer loop
      vertex -18.4768 -8.22642 -14.6946
      vertex -19.7835 -4.2051 -14.6946
      vertex -16.3627 -3.478 -18.5786
    endloop
  endfacet
  facet normal -0.824766 -0.267984 -0.497941
    outer loop
      vertex -19.7835 -4.2051 -14.6946
      vertex -21.4752 -5.08421 -11.4193
      vertex -21.5403 -4.57854 -11.5836
    endloop
  endfacet
  facet normal -0.824766 -0.267983 -0.497941
    outer loop
      vertex -18.4768 -8.22642 -14.6946
      vertex -21.4752 -5.08421 -11.4193
      vertex -19.7835 -4.2051 -14.6946
    endloop
  endfacet
  facet normal -0.824766 -0.267983 -0.497941
    outer loop
      vertex -21.4752 -5.08421 -11.4193
      vertex -18.4768 -8.22642 -14.6946
      vertex -21.4855 -5.51743 -11.1692
    endloop
  endfacet
  facet normal -0.86246 -0.0906477 -0.497942
    outer loop
      vertex -19.7835 -4.2051 -14.6946
      vertex -21.377 -2.5989 -12.2268
      vertex -21.4048 -1.97347 -12.2926
    endloop
  endfacet
  facet normal -0.86246 -0.0906481 -0.497941
    outer loop
      vertex -21.377 -2.5989 -12.2268
      vertex -19.7835 -4.2051 -14.6946
      vertex -21.5403 -4.57854 -11.5836
    endloop
  endfacet
  facet normal -0.586667 -0.0616612 -0.807478
    outer loop
      vertex -12.2268 -2.5989 -21.6506
      vertex -16.7283 0 -18.5786
      vertex -12.5 0 -21.6506
    endloop
  endfacet
  facet normal -0.707142 0.636713 -0.307485
    outer loop
      vertex -15.282 16.9724 -10.1684
      vertex -18.4768 13.4242 -10.1684
      vertex -16.3627 18.1726 -5.19779
    endloop
  endfacet
  facet normal -0.824766 0.267983 -0.497941
    outer loop
      vertex -18.4768 8.22642 -14.6946
      vertex -21.5265 7.25083 -10.1684
      vertex -20.8641 9.28931 -10.1684
    endloop
  endfacet
  facet normal -0.824766 0.267982 -0.497942
    outer loop
      vertex -21.5265 7.25083 -10.1684
      vertex -18.4768 8.22642 -14.6946
      vertex -21.4855 5.51743 -11.1692
    endloop
  endfacet
  facet normal -0.55362 0.498482 -0.667099
    outer loop
      vertex -11.1934 12.4315 -18.5786
      vertex -13.5335 9.83263 -18.5786
      vertex -13.5335 15.0304 -14.6946
    endloop
  endfacet
  facet normal -0.740888 0.0778706 -0.667099
    outer loop
      vertex -16.3627 3.478 -18.5786
      vertex -20.2254 0 -14.6946
      vertex -19.7835 4.2051 -14.6946
    endloop
  endfacet
  facet normal -0.86246 0.0906483 -0.497941
    outer loop
      vertex -20.2254 0 -14.6946
      vertex -21.4048 1.97347 -12.2926
      vertex -19.7835 4.2051 -14.6946
    endloop
  endfacet
  facet normal -0.86246 0.0906483 -0.497941
    outer loop
      vertex -21.4048 1.97347 -12.2926
      vertex -20.2254 0 -14.6946
      vertex -21.4744 0.406491 -12.4573
    endloop
  endfacet
  facet normal -0.86246 0.0906511 -0.497941
    outer loop
      vertex -21.4744 0.406491 -12.4573
      vertex -20.2254 0 -14.6946
      vertex -21.4925 0 -12.5
    endloop
  endfacet
  facet normal 0.154888 0.72869 -0.667099
    outer loop
      vertex 5.16932 15.9095 -18.5786
      vertex 2.11413 20.1146 -14.6946
      vertex 6.25 19.2355 -14.6946
    endloop
  endfacet
  facet normal -0.346733 0.477238 -0.807478
    outer loop
      vertex -6.25 10.8253 -21.6506
      vertex -11.1934 12.4315 -18.5786
      vertex -8.36413 14.4871 -18.5786
    endloop
  endfacet
  facet normal -0.352726 0.792236 -0.497942
    outer loop
      vertex -10.1127 17.5157 -14.6946
      vertex -7.04314 21.6765 -10.2491
      vertex -6.25 19.2355 -14.6946
    endloop
  endfacet
  facet normal -0.35274 0.792236 -0.497932
    outer loop
      vertex -7.04314 21.6765 -10.2491
      vertex -10.1127 17.5157 -14.6946
      vertex -7.08932 21.6771 -10.2155
    endloop
  endfacet
  facet normal -0.559309 0.769823 -0.307485
    outer loop
      vertex -11.4193 19.7788 -10.1684
      vertex -16.3627 18.1726 -5.19779
      vertex -12.2268 21.1775 -5.19779
    endloop
  endfacet
  facet normal -0.509734 0.701588 -0.497942
    outer loop
      vertex -10.1127 17.5157 -14.6946
      vertex -13.5335 15.0304 -14.6946
      vertex -11.4193 19.7788 -10.1684
    endloop
  endfacet
  facet normal -0.644463 0.580277 -0.497942
    outer loop
      vertex -13.5335 15.0304 -14.6946
      vertex -18.4768 13.4242 -10.1684
      vertex -15.282 16.9724 -10.1684
    endloop
  endfacet
  facet normal -0.55362 -0.498482 -0.667099
    outer loop
      vertex -13.5335 -15.0304 -14.6946
      vertex -16.3627 -11.8882 -14.6946
      vertex -13.5335 -9.83263 -18.5786
    endloop
  endfacet
  facet normal 0.43838 -0.394719 -0.807478
    outer loop
      vertex 11.1934 -12.4315 -18.5786
      vertex 10.1127 -7.34731 -21.6506
      vertex 13.5335 -9.83263 -18.5786
    endloop
  endfacet
  facet normal 0.55362 -0.498482 -0.667099
    outer loop
      vertex 13.5335 -15.0304 -14.6946
      vertex 11.1934 -12.4315 -18.5786
      vertex 13.5335 -9.83263 -18.5786
    endloop
  endfacet
  facet normal -0.510867 0.294949 -0.807478
    outer loop
      vertex -11.4193 5.08421 -21.6506
      vertex -15.282 6.804 -18.5786
      vertex -13.5335 9.83263 -18.5786
    endloop
  endfacet
  facet normal 0.561027 -0.182289 -0.807478
    outer loop
      vertex 15.282 -6.804 -18.5786
      vertex 11.4193 -5.08421 -21.6506
      vertex 16.3627 -3.478 -18.5786
    endloop
  endfacet
  facet normal -0.645162 0.372485 -0.667099
    outer loop
      vertex -13.5335 9.83263 -18.5786
      vertex -18.4768 8.22642 -14.6946
      vertex -16.3627 11.8882 -14.6946
    endloop
  endfacet
  facet normal -0.751027 0.433605 -0.497941
    outer loop
      vertex -18.4768 8.22642 -14.6946
      vertex -20.8641 9.28931 -10.1684
      vertex -18.4768 13.4242 -10.1684
    endloop
  endfacet
  facet normal -0.861332 0.497291 -0.103963
    outer loop
      vertex -20.2254 14.6946 0
      vertex -21.3767 12.1806 -2.48718
      vertex -21.3743 12.4315 -1.30661
    endloop
  endfacet
  facet normal -0.861333 0.49729 -0.103962
    outer loop
      vertex -21.3767 12.1806 -2.48718
      vertex -20.2254 14.6946 0
      vertex -19.7835 14.3735 -5.19779
    endloop
  endfacet
  facet normal -0.861333 0.49729 -0.103962
    outer loop
      vertex -20.2254 14.6946 0
      vertex -21.3743 12.4315 -1.30661
      vertex -21.532 12.4315 0
    endloop
  endfacet
  facet normal -0.861333 0.49729 0.103962
    outer loop
      vertex -21.3743 12.4315 1.30661
      vertex -20.2254 14.6946 0
      vertex -21.532 12.4315 0
    endloop
  endfacet
  facet normal -0.861332 0.497291 0.103963
    outer loop
      vertex -21.3767 12.1806 2.48718
      vertex -20.2254 14.6946 0
      vertex -21.3743 12.4315 1.30661
    endloop
  endfacet
  facet normal -0.861333 0.49729 0.103962
    outer loop
      vertex -20.2254 14.6946 0
      vertex -21.3767 12.1806 2.48718
      vertex -19.7835 14.3735 5.19779
    endloop
  endfacet
  facet normal 0.303006 0.680563 -0.667099
    outer loop
      vertex 8.36413 14.4871 -18.5786
      vertex 5.16932 15.9095 -18.5786
      vertex 6.25 19.2355 -14.6946
    endloop
  endfacet
  facet normal 0.346734 0.477238 -0.807478
    outer loop
      vertex 11.1934 12.4315 -18.5786
      vertex 6.25 10.8253 -21.6506
      vertex 8.36413 14.4871 -18.5786
    endloop
  endfacet
  facet normal 0.645162 0.372485 -0.667099
    outer loop
      vertex 18.4768 8.22642 -14.6946
      vertex 13.5335 9.83263 -18.5786
      vertex 16.3627 11.8882 -14.6946
    endloop
  endfacet
  facet normal 0.86246 -0.0906486 -0.497941
    outer loop
      vertex 21.4048 -1.97347 -12.2926
      vertex 20.2254 0 -14.6946
      vertex 21.4925 0 -12.5
    endloop
  endfacet
  facet normal 0.86246 -0.0906483 -0.497941
    outer loop
      vertex 20.2254 0 -14.6946
      vertex 21.4048 -1.97347 -12.2926
      vertex 19.7835 -4.2051 -14.6946
    endloop
  endfacet
  facet normal 0.510867 0.294949 -0.807478
    outer loop
      vertex 15.282 6.804 -18.5786
      vertex 11.4193 5.08421 -21.6506
      vertex 13.5335 9.83263 -18.5786
    endloop
  endfacet
  facet normal 0.510867 0.294949 -0.807478
    outer loop
      vertex 11.4193 5.08421 -21.6506
      vertex 10.1127 7.34731 -21.6506
      vertex 13.5335 9.83263 -18.5786
    endloop
  endfacet
  facet normal 0.346734 0.477238 0.807478
    outer loop
      vertex 8.36413 14.4871 18.5786
      vertex 6.25 10.8253 21.6506
      vertex 11.1934 12.4315 18.5786
    endloop
  endfacet
  facet normal 0.352726 0.792236 0.497942
    outer loop
      vertex 7.15415 21.6778 10.1684
      vertex 6.25 19.2355 14.6946
      vertex 11.4193 19.7788 10.1684
    endloop
  endfacet
  facet normal 0.352724 0.792237 0.497942
    outer loop
      vertex 6.25 19.2355 14.6946
      vertex 7.15415 21.6778 10.1684
      vertex 7.04314 21.6765 10.2491
    endloop
  endfacet
  facet normal 0.586667 0.0616609 0.807478
    outer loop
      vertex 16.3627 3.478 18.5786
      vertex 12.2268 2.5989 21.6506
      vertex 16.7283 0 18.5786
    endloop
  endfacet
  facet normal 0.708508 0.230208 0.667099
    outer loop
      vertex 18.4768 8.22642 14.6946
      vertex 15.282 6.804 18.5786
      vertex 16.3627 3.478 18.5786
    endloop
  endfacet
  facet normal 0.561027 0.182289 0.807478
    outer loop
      vertex 15.282 6.804 18.5786
      vertex 11.4193 5.08421 21.6506
      vertex 16.3627 3.478 18.5786
    endloop
  endfacet
  facet normal 0.86246 -0.0906511 0.497941
    outer loop
      vertex 20.2254 0 14.6946
      vertex 21.4744 -0.406491 12.4573
      vertex 21.4925 0 12.5
    endloop
  endfacet
  facet normal 0.86246 -0.0906483 0.497941
    outer loop
      vertex 20.2254 0 14.6946
      vertex 21.4048 -1.97347 12.2926
      vertex 21.4744 -0.406491 12.4573
    endloop
  endfacet
  facet normal 0.86246 -0.0906483 0.497941
    outer loop
      vertex 21.4048 -1.97347 12.2926
      vertex 20.2254 0 14.6946
      vertex 19.7835 -4.2051 14.6946
    endloop
  endfacet
  facet normal 0.86246 -0.0906477 0.497942
    outer loop
      vertex 19.7835 -4.2051 14.6946
      vertex 21.377 -2.5989 12.2268
      vertex 21.4048 -1.97347 12.2926
    endloop
  endfacet
  facet normal 0.86246 -0.0906481 0.497941
    outer loop
      vertex 21.377 -2.5989 12.2268
      vertex 19.7835 -4.2051 14.6946
      vertex 21.5403 -4.57854 11.5836
    endloop
  endfacet
  facet normal 0.509734 0.701588 0.497942
    outer loop
      vertex 15.282 16.9724 10.1684
      vertex 11.4193 19.7788 10.1684
      vertex 13.5335 15.0304 14.6946
    endloop
  endfacet
  facet normal 0.644463 0.580277 0.497942
    outer loop
      vertex 15.282 16.9724 10.1684
      vertex 13.5335 15.0304 14.6946
      vertex 18.4768 13.4242 10.1684
    endloop
  endfacet
  facet normal 0.824069 0.475777 -0.307484
    outer loop
      vertex 21.5188 9.58077 -7.96296
      vertex 18.4768 13.4242 -10.1684
      vertex 21.444 10.7525 -6.35017
    endloop
  endfacet
  facet normal 0.824069 0.475776 -0.307485
    outer loop
      vertex 18.4768 13.4242 -10.1684
      vertex 21.5188 9.58077 -7.96296
      vertex 20.8641 9.28931 -10.1684
    endloop
  endfacet
  facet normal 0.708508 0.230208 -0.667099
    outer loop
      vertex 19.7835 4.2051 -14.6946
      vertex 16.3627 3.478 -18.5786
      vertex 18.4768 8.22642 -14.6946
    endloop
  endfacet
  facet normal 0.861333 -0.49729 -0.103962
    outer loop
      vertex 21.4306 -11.7212 -4.23778
      vertex 19.7835 -14.3735 -5.19779
      vertex 21.5615 -11.2938 -5.19779
    endloop
  endfacet
  facet normal 0.861333 -0.497289 -0.103966
    outer loop
      vertex 21.3795 -11.8882 -3.86271
      vertex 19.7835 -14.3735 -5.19779
      vertex 21.4306 -11.7212 -4.23778
    endloop
  endfacet
  facet normal 0.861333 -0.497291 -0.103961
    outer loop
      vertex 19.7835 -14.3735 -5.19779
      vertex 21.3795 -11.8882 -3.86271
      vertex 21.3767 -12.1806 -2.48718
    endloop
  endfacet
  facet normal 0.561027 -0.182289 -0.807478
    outer loop
      vertex 16.3627 -3.478 -18.5786
      vertex 11.4193 -5.08421 -21.6506
      vertex 12.2268 -2.5989 -21.6506
    endloop
  endfacet
  facet normal 0.740888 0.0778701 -0.667099
    outer loop
      vertex 16.7283 0 -18.5786
      vertex 16.3627 3.478 -18.5786
      vertex 20.2254 0 -14.6946
    endloop
  endfacet
  facet normal 0.437882 0.602693 -0.667099
    outer loop
      vertex 13.5335 15.0304 -14.6946
      vertex 8.36413 14.4871 -18.5786
      vertex 10.1127 17.5157 -14.6946
    endloop
  endfacet
  facet normal 0.303006 0.680563 -0.667099
    outer loop
      vertex 8.36413 14.4871 -18.5786
      vertex 6.25 19.2355 -14.6946
      vertex 10.1127 17.5157 -14.6946
    endloop
  endfacet
  facet normal 0.739118 0.665505 -0.103962
    outer loop
      vertex 20.2254 14.6946 0
      vertex 16.3627 18.1726 -5.19779
      vertex 16.7283 18.5786 0
    endloop
  endfacet
  facet normal 0.739118 0.665505 -0.103962
    outer loop
      vertex 19.7835 14.3735 -5.19779
      vertex 16.3627 18.1726 -5.19779
      vertex 20.2254 14.6946 0
    endloop
  endfacet
  facet normal 0.5846 0.804633 0.103962
    outer loop
      vertex 12.5 21.6506 0
      vertex 12.2268 21.1775 5.19779
      vertex 16.3627 18.1726 5.19779
    endloop
  endfacet
  facet normal 0.5846 0.804633 0.103962
    outer loop
      vertex 16.7283 18.5786 0
      vertex 12.5 21.6506 0
      vertex 16.3627 18.1726 5.19779
    endloop
  endfacet
  facet normal -0.824069 0.475777 0.307484
    outer loop
      vertex -21.5188 9.58077 7.96296
      vertex -18.4768 13.4242 10.1684
      vertex -21.444 10.7525 6.35017
    endloop
  endfacet
  facet normal -0.824069 0.475776 0.307485
    outer loop
      vertex -18.4768 13.4242 10.1684
      vertex -21.5188 9.58077 7.96296
      vertex -20.8641 9.28931 10.1684
    endloop
  endfacet
  facet normal 0.122647 0.577008 0.807478
    outer loop
      vertex 1.74858 16.6366 18.5786
      vertex 1.30661 12.4315 21.6506
      vertex 5.16932 15.9095 18.5786
    endloop
  endfacet
  facet normal 0.154888 0.72869 0.667099
    outer loop
      vertex 2.11413 20.1146 14.6946
      vertex 1.74858 16.6366 18.5786
      vertex 5.16932 15.9095 18.5786
    endloop
  endfacet
  facet normal 0 0.867211 0.497942
    outer loop
      vertex 2.11413 20.1146 14.6946
      vertex 1.30661 21.4141 12.4315
      vertex -2.11413 20.1146 14.6946
    endloop
  endfacet
  facet normal 5.36664e-07 0.867211 0.497941
    outer loop
      vertex 1.30661 21.4141 12.4315
      vertex 2.11413 20.1146 14.6946
      vertex 2.26298 21.5308 12.2282
    endloop
  endfacet
  facet normal -0 0.867211 0.497942
    outer loop
      vertex -1.30661 21.4141 12.4315
      vertex -2.11413 20.1146 14.6946
      vertex 1.30661 21.4141 12.4315
    endloop
  endfacet
  facet normal -5.36664e-07 0.867211 0.497941
    outer loop
      vertex -2.11413 20.1146 14.6946
      vertex -1.30661 21.4141 12.4315
      vertex -2.26298 21.5308 12.2282
    endloop
  endfacet
  facet normal 0.180303 0.84826 0.497942
    outer loop
      vertex 6.25 19.2355 14.6946
      vertex 6.68623 21.6002 10.5084
      vertex 2.11413 20.1146 14.6946
    endloop
  endfacet
  facet normal 0.180301 0.84826 0.497942
    outer loop
      vertex 6.68623 21.6002 10.5084
      vertex 6.25 19.2355 14.6946
      vertex 7.04314 21.6765 10.2491
    endloop
  endfacet
  facet normal 0.154888 0.72869 0.667099
    outer loop
      vertex 6.25 19.2355 14.6946
      vertex 2.11413 20.1146 14.6946
      vertex 5.16932 15.9095 18.5786
    endloop
  endfacet
  facet normal -0.154888 -0.72869 0.667099
    outer loop
      vertex -1.74858 -16.6366 18.5786
      vertex -5.16932 -15.9095 18.5786
      vertex -2.11413 -20.1146 14.6946
    endloop
  endfacet
  facet normal 0.404532 -0.908595 0.103962
    outer loop
      vertex 11.4193 -21.55 5.08421
      vertex 12.2268 -21.1775 5.19779
      vertex 11.3537 -21.5662 5.19779
    endloop
  endfacet
  facet normal 0.404532 -0.908595 0.103962
    outer loop
      vertex 12.2268 -21.4749 2.5989
      vertex 12.2268 -21.1775 5.19779
      vertex 11.4193 -21.55 5.08421
    endloop
  endfacet
  facet normal 0.404532 -0.908595 0.103962
    outer loop
      vertex 12.2268 -21.1775 5.19779
      vertex 12.2268 -21.4749 2.5989
      vertex 12.5 -21.6506 0
    endloop
  endfacet
  facet normal 0.707142 -0.636713 0.307485
    outer loop
      vertex 18.4768 -13.4242 10.1684
      vertex 15.282 -16.9724 10.1684
      vertex 16.3627 -18.1726 5.19779
    endloop
  endfacet
  facet normal 0.387032 -0.869287 0.307485
    outer loop
      vertex 8.36413 -21.4501 9.28931
      vertex 11.4193 -19.7788 10.1684
      vertex 7.15415 -21.6778 10.1684
    endloop
  endfacet
  facet normal 0.387032 -0.869287 0.307485
    outer loop
      vertex 11.4193 -19.7788 10.1684
      vertex 8.36413 -21.4501 9.28931
      vertex 9.63732 -21.3834 7.8753
    endloop
  endfacet
  facet normal -0.239933 -0.538899 0.807478
    outer loop
      vertex -6.25 -10.8253 21.6506
      vertex -8.36413 -14.4871 18.5786
      vertex -5.16932 -15.9095 18.5786
    endloop
  endfacet
  facet normal 0 -0.744969 0.667099
    outer loop
      vertex -2.11413 -20.1146 14.6946
      vertex 1.74858 -16.6366 18.5786
      vertex -1.74858 -16.6366 18.5786
    endloop
  endfacet
  facet normal 0 -0.744969 0.667099
    outer loop
      vertex 1.74858 -16.6366 18.5786
      vertex -2.11413 -20.1146 14.6946
      vertex 2.11413 -20.1146 14.6946
    endloop
  endfacet
  facet normal -0.154888 -0.72869 0.667099
    outer loop
      vertex -5.16932 -15.9095 18.5786
      vertex -6.25 -19.2355 14.6946
      vertex -2.11413 -20.1146 14.6946
    endloop
  endfacet
  facet normal -0.122647 -0.577008 0.807478
    outer loop
      vertex -1.30661 -12.4315 21.6506
      vertex -5.16932 -15.9095 18.5786
      vertex -1.74858 -16.6366 18.5786
    endloop
  endfacet
  facet normal -0.739118 -0.665505 0.103962
    outer loop
      vertex -16.3627 -18.1726 5.19779
      vertex -20.2254 -14.6946 0
      vertex -16.7283 -18.5786 0
    endloop
  endfacet
  facet normal -0.5846 -0.804633 0.103962
    outer loop
      vertex -16.3627 -18.1726 5.19779
      vertex -16.7283 -18.5786 0
      vertex -12.5 -21.6506 0
    endloop
  endfacet
  facet normal -0.708508 -0.230208 0.667099
    outer loop
      vertex -16.3627 -3.478 18.5786
      vertex -19.7835 -4.2051 14.6946
      vertex -18.4768 -8.22642 14.6946
    endloop
  endfacet
  facet normal -0.43838 -0.394719 0.807478
    outer loop
      vertex -10.1127 -7.34731 21.6506
      vertex -11.1934 -12.4315 18.5786
      vertex -8.36413 -9.28931 21.6506
    endloop
  endfacet
  facet normal -0.904981 0.294044 0.307485
    outer loop
      vertex -21.5113 9.52865 8.03471
      vertex -20.8641 9.28931 10.1684
      vertex -21.5188 9.58077 7.96296
    endloop
  endfacet
  facet normal -0.90498 0.294049 0.307485
    outer loop
      vertex -21.4772 9.28931 8.36413
      vertex -20.8641 9.28931 10.1684
      vertex -21.5113 9.52865 8.03471
    endloop
  endfacet
  facet normal -0.904981 0.294046 0.307485
    outer loop
      vertex -20.8641 9.28931 10.1684
      vertex -21.4772 9.28931 8.36413
      vertex -21.4936 8.42289 9.14426
    endloop
  endfacet
  facet normal -0.904981 0.294046 0.307484
    outer loop
      vertex -21.5141 7.34731 10.1127
      vertex -20.8641 9.28931 10.1684
      vertex -21.4936 8.42289 9.14426
    endloop
  endfacet
  facet normal -0.904981 0.294046 0.307483
    outer loop
      vertex -20.8641 9.28931 10.1684
      vertex -21.5141 7.34731 10.1127
      vertex -21.5265 7.25083 10.1684
    endloop
  endfacet
  facet normal -0.86246 -0.0906484 0.497941
    outer loop
      vertex -19.7835 -4.2051 14.6946
      vertex -21.3953 -2.18911 12.2699
      vertex -21.377 -2.5989 12.2268
    endloop
  endfacet
  facet normal -0.862459 -0.0906462 0.497943
    outer loop
      vertex -21.3953 -2.18911 12.2699
      vertex -19.7835 -4.2051 14.6946
      vertex -21.4048 -1.97347 12.2926
    endloop
  endfacet
  facet normal -0.86246 -0.0906481 0.497941
    outer loop
      vertex -19.7835 -4.2051 14.6946
      vertex -21.377 -2.5989 12.2268
      vertex -21.5403 -4.57854 11.5836
    endloop
  endfacet
  facet normal -0.561027 0.182289 0.807478
    outer loop
      vertex -11.4193 5.08421 21.6506
      vertex -16.3627 3.478 18.5786
      vertex -12.2268 2.5989 21.6506
    endloop
  endfacet
  facet normal -0.586667 -0.0616609 0.807478
    outer loop
      vertex -12.2268 -2.5989 21.6506
      vertex -16.7283 0 18.5786
      vertex -16.3627 -3.478 18.5786
    endloop
  endfacet
  facet normal -0.586667 -0.0616612 0.807478
    outer loop
      vertex -12.5 0 21.6506
      vertex -16.7283 0 18.5786
      vertex -12.2268 -2.5989 21.6506
    endloop
  endfacet
  facet normal -0.708508 0.230208 0.667099
    outer loop
      vertex -15.282 6.804 18.5786
      vertex -18.4768 8.22642 14.6946
      vertex -16.3627 3.478 18.5786
    endloop
  endfacet
  facet normal -0.86246 0.0906481 0.497941
    outer loop
      vertex -21.377 2.5989 12.2268
      vertex -19.7835 4.2051 14.6946
      vertex -21.5403 4.57854 11.5836
    endloop
  endfacet
  facet normal -0.86246 0.0906484 0.497941
    outer loop
      vertex -21.3953 2.18911 12.2699
      vertex -19.7835 4.2051 14.6946
      vertex -21.377 2.5989 12.2268
    endloop
  endfacet
  facet normal -0.862459 0.0906462 0.497943
    outer loop
      vertex -19.7835 4.2051 14.6946
      vertex -21.3953 2.18911 12.2699
      vertex -21.4048 1.97347 12.2926
    endloop
  endfacet
  facet normal -0.510867 0.294949 0.807478
    outer loop
      vertex -13.5335 9.83263 18.5786
      vertex -15.282 6.804 18.5786
      vertex -11.4193 5.08421 21.6506
    endloop
  endfacet
  facet normal -0.751026 0.433605 0.497942
    outer loop
      vertex -18.4768 13.4242 10.1684
      vertex -18.4768 8.22642 14.6946
      vertex -16.3627 11.8882 14.6946
    endloop
  endfacet
  facet normal -0.239933 0.538899 0.807478
    outer loop
      vertex -5.16932 15.9095 18.5786
      vertex -8.36413 14.4871 18.5786
      vertex -6.25 10.8253 21.6506
    endloop
  endfacet
  facet normal -0.437882 0.602693 0.667099
    outer loop
      vertex -10.1127 17.5157 14.6946
      vertex -13.5335 15.0304 14.6946
      vertex -8.36413 14.4871 18.5786
    endloop
  endfacet
  facet normal 0.708508 -0.230208 -0.667099
    outer loop
      vertex 18.4768 -8.22642 -14.6946
      vertex 15.282 -6.804 -18.5786
      vertex 16.3627 -3.478 -18.5786
    endloop
  endfacet
  facet normal 0.561027 0.182289 -0.807478
    outer loop
      vertex 12.2268 2.5989 -21.6506
      vertex 11.4193 5.08421 -21.6506
      vertex 16.3627 3.478 -18.5786
    endloop
  endfacet
  facet normal 0.352726 -0.792236 -0.497942
    outer loop
      vertex 10.1127 -17.5157 -14.6946
      vertex 7.04314 -21.6765 -10.2491
      vertex 6.25 -19.2355 -14.6946
    endloop
  endfacet
  facet normal 0.35274 -0.792236 -0.497932
    outer loop
      vertex 7.04314 -21.6765 -10.2491
      vertex 10.1127 -17.5157 -14.6946
      vertex 7.08932 -21.6771 -10.2155
    endloop
  endfacet
  facet normal 0.739118 -0.665505 0.103962
    outer loop
      vertex 20.2254 -14.6946 0
      vertex 16.3627 -18.1726 5.19779
      vertex 16.7283 -18.5786 0
    endloop
  endfacet
  facet normal 0.739118 -0.665505 -0.103962
    outer loop
      vertex 20.2254 -14.6946 0
      vertex 16.3627 -18.1726 -5.19779
      vertex 19.7835 -14.3735 -5.19779
    endloop
  endfacet
  facet normal 0.904981 -0.294046 0.307485
    outer loop
      vertex 20.8641 -9.28931 10.1684
      vertex 21.4772 -9.28931 8.36413
      vertex 21.4936 -8.42289 9.14426
    endloop
  endfacet
  facet normal 0.90498 -0.294047 0.307485
    outer loop
      vertex 21.4772 -9.28931 8.36413
      vertex 20.8641 -9.28931 10.1684
      vertex 21.5188 -9.58077 7.96297
    endloop
  endfacet
  facet normal 0.904981 -0.294046 0.307484
    outer loop
      vertex 21.5141 -7.34731 10.1127
      vertex 20.8641 -9.28931 10.1684
      vertex 21.4936 -8.42289 9.14426
    endloop
  endfacet
  facet normal 0.904981 -0.294046 0.307483
    outer loop
      vertex 20.8641 -9.28931 10.1684
      vertex 21.5141 -7.34731 10.1127
      vertex 21.5265 -7.25083 10.1684
    endloop
  endfacet
  facet normal 0.510867 -0.294949 0.807478
    outer loop
      vertex 11.4193 -5.08421 21.6506
      vertex 10.1127 -7.34731 21.6506
      vertex 13.5335 -9.83263 18.5786
    endloop
  endfacet
  facet normal -0.707142 -0.636713 -0.307485
    outer loop
      vertex -16.3627 -18.1726 -5.19779
      vertex -19.7835 -14.3735 -5.19779
      vertex -18.4768 -13.4242 -10.1684
    endloop
  endfacet
  facet normal -0.739118 -0.665505 -0.103962
    outer loop
      vertex -16.3627 -18.1726 -5.19779
      vertex -20.2254 -14.6946 0
      vertex -19.7835 -14.3735 -5.19779
    endloop
  endfacet
  facet normal -0.509734 -0.701588 0.497942
    outer loop
      vertex -10.1127 -17.5157 14.6946
      vertex -13.5335 -15.0304 14.6946
      vertex -11.4193 -19.7788 10.1684
    endloop
  endfacet
  facet normal -0.707142 -0.636713 0.307485
    outer loop
      vertex -15.282 -16.9724 10.1684
      vertex -18.4768 -13.4242 10.1684
      vertex -16.3627 -18.1726 5.19779
    endloop
  endfacet
  facet normal -0.352726 -0.792236 0.497942
    outer loop
      vertex -7.15415 -21.6778 10.1684
      vertex -6.25 -19.2355 14.6946
      vertex -11.4193 -19.7788 10.1684
    endloop
  endfacet
  facet normal -0.352723 -0.792238 0.497942
    outer loop
      vertex -6.25 -19.2355 14.6946
      vertex -7.15415 -21.6778 10.1684
      vertex -7.04314 -21.6765 10.2491
    endloop
  endfacet
  facet normal -0.509734 -0.701588 0.497942
    outer loop
      vertex -13.5335 -15.0304 14.6946
      vertex -15.282 -16.9724 10.1684
      vertex -11.4193 -19.7788 10.1684
    endloop
  endfacet
  facet normal -0.387032 -0.869287 0.307485
    outer loop
      vertex -11.4193 -19.7788 10.1684
      vertex -10.1127 -21.3585 7.34731
      vertex -9.63732 -21.3834 7.8753
    endloop
  endfacet
  facet normal -0.387032 -0.869287 0.307485
    outer loop
      vertex -12.2268 -21.1775 5.19779
      vertex -10.1127 -21.3585 7.34731
      vertex -11.4193 -19.7788 10.1684
    endloop
  endfacet
  facet normal -0.387031 -0.869287 0.307484
    outer loop
      vertex -10.1127 -21.3585 7.34731
      vertex -12.2268 -21.1775 5.19779
      vertex -11.3537 -21.5662 5.19779
    endloop
  endfacet
  facet normal -0.904981 0.294046 -0.307484
    outer loop
      vertex -20.8641 9.28931 -10.1684
      vertex -21.5141 7.34731 -10.1127
      vertex -21.4936 8.42289 -9.14426
    endloop
  endfacet
  facet normal -0.904981 0.294046 -0.307483
    outer loop
      vertex -21.5141 7.34731 -10.1127
      vertex -20.8641 9.28931 -10.1684
      vertex -21.5265 7.25083 -10.1684
    endloop
  endfacet
  facet normal -0.43838 0.394719 -0.807478
    outer loop
      vertex -10.1127 7.34731 -21.6506
      vertex -13.5335 9.83263 -18.5786
      vertex -11.1934 12.4315 -18.5786
    endloop
  endfacet
  facet normal 0 0.589898 -0.807478
    outer loop
      vertex -1.30661 12.4315 -21.6506
      vertex 1.74858 16.6366 -18.5786
      vertex 1.30661 12.4315 -21.6506
    endloop
  endfacet
  facet normal 0 0.589898 -0.807478
    outer loop
      vertex 1.74858 16.6366 -18.5786
      vertex -1.30661 12.4315 -21.6506
      vertex -1.74858 16.6366 -18.5786
    endloop
  endfacet
  facet normal -0.180314 0.848253 -0.49795
    outer loop
      vertex -6.25 21.5069 -10.8253
      vertex -2.11413 20.1146 -14.6946
      vertex -6.68623 21.6002 -10.5084
    endloop
  endfacet
  facet normal -0.180303 0.84826 -0.497941
    outer loop
      vertex -3.86271 21.3904 -11.8882
      vertex -2.11413 20.1146 -14.6946
      vertex -6.25 21.5069 -10.8253
    endloop
  endfacet
  facet normal -0.180303 0.84826 -0.497941
    outer loop
      vertex -2.11413 20.1146 -14.6946
      vertex -3.86271 21.3904 -11.8882
      vertex -2.26298 21.5308 -12.2282
    endloop
  endfacet
  facet normal -0.559309 0.769822 -0.307485
    outer loop
      vertex -15.282 16.9724 -10.1684
      vertex -16.3627 18.1726 -5.19779
      vertex -11.4193 19.7788 -10.1684
    endloop
  endfacet
  facet normal -0.509734 0.701588 -0.497942
    outer loop
      vertex -13.5335 15.0304 -14.6946
      vertex -15.282 16.9724 -10.1684
      vertex -11.4193 19.7788 -10.1684
    endloop
  endfacet
  facet normal 0.180303 0.84826 -0.497941
    outer loop
      vertex 3.86271 21.3904 -11.8882
      vertex 2.11413 20.1146 -14.6946
      vertex 2.26298 21.5308 -12.2282
    endloop
  endfacet
  facet normal 0.180303 0.84826 -0.497941
    outer loop
      vertex 6.25 21.5069 -10.8253
      vertex 2.11413 20.1146 -14.6946
      vertex 3.86271 21.3904 -11.8882
    endloop
  endfacet
  facet normal 0.180314 0.848253 -0.49795
    outer loop
      vertex 2.11413 20.1146 -14.6946
      vertex 6.25 21.5069 -10.8253
      vertex 6.68623 21.6002 -10.5084
    endloop
  endfacet
  facet normal 0 0.867211 -0.497942
    outer loop
      vertex 1.30661 21.4141 -12.4315
      vertex 2.11413 20.1146 -14.6946
      vertex -1.30661 21.4141 -12.4315
    endloop
  endfacet
  facet normal 5.36664e-07 0.867211 -0.497941
    outer loop
      vertex 2.11413 20.1146 -14.6946
      vertex 1.30661 21.4141 -12.4315
      vertex 2.26298 21.5308 -12.2282
    endloop
  endfacet
  facet normal 0 0.867211 -0.497942
    outer loop
      vertex -2.11413 20.1146 -14.6946
      vertex -1.30661 21.4141 -12.4315
      vertex 2.11413 20.1146 -14.6946
    endloop
  endfacet
  facet normal -5.36664e-07 0.867211 -0.497941
    outer loop
      vertex -1.30661 21.4141 -12.4315
      vertex -2.11413 20.1146 -14.6946
      vertex -2.26298 21.5308 -12.2282
    endloop
  endfacet
  facet normal -0.510867 -0.294949 -0.807478
    outer loop
      vertex -10.1127 -7.34731 -21.6506
      vertex -13.5335 -9.83263 -18.5786
      vertex -11.4193 -5.08421 -21.6506
    endloop
  endfacet
  facet normal -0.43838 -0.394719 -0.807478
    outer loop
      vertex -8.36413 -9.28931 -21.6506
      vertex -11.1934 -12.4315 -18.5786
      vertex -10.1127 -7.34731 -21.6506
    endloop
  endfacet
  facet normal -0.561027 -0.182289 -0.807478
    outer loop
      vertex -15.282 -6.804 -18.5786
      vertex -16.3627 -3.478 -18.5786
      vertex -11.4193 -5.08421 -21.6506
    endloop
  endfacet
  facet normal -0.645162 -0.372485 -0.667099
    outer loop
      vertex -16.3627 -11.8882 -14.6946
      vertex -18.4768 -8.22642 -14.6946
      vertex -13.5335 -9.83263 -18.5786
    endloop
  endfacet
  facet normal -0.561027 0.182289 -0.807478
    outer loop
      vertex -12.2268 2.5989 -21.6506
      vertex -16.3627 3.478 -18.5786
      vertex -11.4193 5.08421 -21.6506
    endloop
  endfacet
  facet normal -0.708508 0.230208 -0.667099
    outer loop
      vertex -16.3627 3.478 -18.5786
      vertex -18.4768 8.22642 -14.6946
      vertex -15.282 6.804 -18.5786
    endloop
  endfacet
  facet normal -0.645162 0.372485 -0.667099
    outer loop
      vertex -15.282 6.804 -18.5786
      vertex -18.4768 8.22642 -14.6946
      vertex -13.5335 9.83263 -18.5786
    endloop
  endfacet
  facet normal 0.239933 -0.538899 -0.807478
    outer loop
      vertex 5.16932 -15.9095 -18.5786
      vertex 3.86271 -11.8882 -21.6506
      vertex 6.25 -10.8253 -21.6506
    endloop
  endfacet
  facet normal -0.644463 0.580277 -0.497942
    outer loop
      vertex -16.3627 11.8882 -14.6946
      vertex -18.4768 13.4242 -10.1684
      vertex -13.5335 15.0304 -14.6946
    endloop
  endfacet
  facet normal -0.55362 0.498482 -0.667099
    outer loop
      vertex -13.5335 9.83263 -18.5786
      vertex -16.3627 11.8882 -14.6946
      vertex -13.5335 15.0304 -14.6946
    endloop
  endfacet
  facet normal 0.154888 0.72869 -0.667099
    outer loop
      vertex 5.16932 15.9095 -18.5786
      vertex 1.74858 16.6366 -18.5786
      vertex 2.11413 20.1146 -14.6946
    endloop
  endfacet
  facet normal 0.122647 0.577008 -0.807478
    outer loop
      vertex 5.16932 15.9095 -18.5786
      vertex 1.30661 12.4315 -21.6506
      vertex 1.74858 16.6366 -18.5786
    endloop
  endfacet
  facet normal 0.346734 0.477238 0.807478
    outer loop
      vertex 11.1934 12.4315 18.5786
      vertex 6.25 10.8253 21.6506
      vertex 8.36413 9.28931 21.6506
    endloop
  endfacet
  facet normal 0.561027 0.182289 0.807478
    outer loop
      vertex 16.3627 3.478 18.5786
      vertex 11.4193 5.08421 21.6506
      vertex 12.2268 2.5989 21.6506
    endloop
  endfacet
  facet normal 0.824766 0.267983 0.497941
    outer loop
      vertex 18.4768 8.22642 14.6946
      vertex 21.4752 5.08421 11.4193
      vertex 21.4855 5.51743 11.1692
    endloop
  endfacet
  facet normal 0.824766 0.267983 0.497941
    outer loop
      vertex 19.7835 4.2051 14.6946
      vertex 21.4752 5.08421 11.4193
      vertex 18.4768 8.22642 14.6946
    endloop
  endfacet
  facet normal 0.824766 0.267984 0.497941
    outer loop
      vertex 21.4752 5.08421 11.4193
      vertex 19.7835 4.2051 14.6946
      vertex 21.5403 4.57854 11.5836
    endloop
  endfacet
  facet normal 0.86246 0.0906481 0.497941
    outer loop
      vertex 19.7835 4.2051 14.6946
      vertex 21.377 2.5989 12.2268
      vertex 21.5403 4.57854 11.5836
    endloop
  endfacet
  facet normal 0.86246 0.0906477 0.497942
    outer loop
      vertex 21.377 2.5989 12.2268
      vertex 19.7835 4.2051 14.6946
      vertex 21.4048 1.97347 12.2926
    endloop
  endfacet
  facet normal 0.55362 0.498482 0.667099
    outer loop
      vertex 13.5335 15.0304 14.6946
      vertex 11.1934 12.4315 18.5786
      vertex 13.5335 9.83263 18.5786
    endloop
  endfacet
  facet normal 0.55362 0.498482 0.667099
    outer loop
      vertex 13.5335 15.0304 14.6946
      vertex 13.5335 9.83263 18.5786
      vertex 16.3627 11.8882 14.6946
    endloop
  endfacet
  facet normal 0.824069 0.475776 -0.307485
    outer loop
      vertex 21.4394 10.8253 -6.25
      vertex 19.7835 14.3735 -5.19779
      vertex 21.5615 11.2938 -5.19779
    endloop
  endfacet
  facet normal 0.824069 0.475776 -0.307485
    outer loop
      vertex 19.7835 14.3735 -5.19779
      vertex 21.4394 10.8253 -6.25
      vertex 18.4768 13.4242 -10.1684
    endloop
  endfacet
  facet normal 0.824069 0.475779 -0.307482
    outer loop
      vertex 18.4768 13.4242 -10.1684
      vertex 21.4394 10.8253 -6.25
      vertex 21.444 10.7525 -6.35017
    endloop
  endfacet
  facet normal 0.904981 0.294044 -0.307485
    outer loop
      vertex 21.5113 9.52865 -8.03471
      vertex 20.8641 9.28931 -10.1684
      vertex 21.5188 9.58077 -7.96296
    endloop
  endfacet
  facet normal 0.90498 0.294049 -0.307485
    outer loop
      vertex 21.4772 9.28931 -8.36413
      vertex 20.8641 9.28931 -10.1684
      vertex 21.5113 9.52865 -8.03471
    endloop
  endfacet
  facet normal 0.904981 0.294046 -0.307485
    outer loop
      vertex 20.8641 9.28931 -10.1684
      vertex 21.4772 9.28931 -8.36413
      vertex 21.4936 8.42289 -9.14426
    endloop
  endfacet
  facet normal 0.86246 0.0906481 -0.497941
    outer loop
      vertex 21.377 2.5989 -12.2268
      vertex 19.7835 4.2051 -14.6946
      vertex 21.5403 4.57854 -11.5836
    endloop
  endfacet
  facet normal 0.86246 0.0906484 -0.497941
    outer loop
      vertex 21.3953 2.18911 -12.2699
      vertex 19.7835 4.2051 -14.6946
      vertex 21.377 2.5989 -12.2268
    endloop
  endfacet
  facet normal 0.862459 0.0906462 -0.497943
    outer loop
      vertex 19.7835 4.2051 -14.6946
      vertex 21.3953 2.18911 -12.2699
      vertex 21.4048 1.97347 -12.2926
    endloop
  endfacet
  facet normal 0.824766 0.267983 -0.497941
    outer loop
      vertex 21.4752 5.08421 -11.4193
      vertex 18.4768 8.22642 -14.6946
      vertex 21.4855 5.51743 -11.1692
    endloop
  endfacet
  facet normal 0.824766 0.267984 -0.497941
    outer loop
      vertex 19.7835 4.2051 -14.6946
      vertex 21.4752 5.08421 -11.4193
      vertex 21.5403 4.57854 -11.5836
    endloop
  endfacet
  facet normal 0.824766 0.267983 -0.497941
    outer loop
      vertex 21.4752 5.08421 -11.4193
      vertex 19.7835 4.2051 -14.6946
      vertex 18.4768 8.22642 -14.6946
    endloop
  endfacet
  facet normal 0.86246 -0.0906484 -0.497941
    outer loop
      vertex 19.7835 -4.2051 -14.6946
      vertex 21.3953 -2.18911 -12.2699
      vertex 21.377 -2.5989 -12.2268
    endloop
  endfacet
  facet normal 0.862459 -0.0906462 -0.497943
    outer loop
      vertex 21.3953 -2.18911 -12.2699
      vertex 19.7835 -4.2051 -14.6946
      vertex 21.4048 -1.97347 -12.2926
    endloop
  endfacet
  facet normal 0.86246 -0.0906481 -0.497941
    outer loop
      vertex 19.7835 -4.2051 -14.6946
      vertex 21.377 -2.5989 -12.2268
      vertex 21.5403 -4.57854 -11.5836
    endloop
  endfacet
  facet normal 0.904981 -0.294046 -0.307485
    outer loop
      vertex 21.4772 -9.28931 -8.36413
      vertex 20.8641 -9.28931 -10.1684
      vertex 21.4936 -8.42289 -9.14426
    endloop
  endfacet
  facet normal 0.90498 -0.294049 -0.307485
    outer loop
      vertex 21.5113 -9.52865 -8.03471
      vertex 20.8641 -9.28931 -10.1684
      vertex 21.4772 -9.28931 -8.36413
    endloop
  endfacet
  facet normal 0.904981 -0.294044 -0.307485
    outer loop
      vertex 20.8641 -9.28931 -10.1684
      vertex 21.5113 -9.52865 -8.03471
      vertex 21.5188 -9.58077 -7.96296
    endloop
  endfacet
  facet normal 0.5846 0.804633 -0.103962
    outer loop
      vertex 16.3627 18.1726 -5.19779
      vertex 12.2268 21.1775 -5.19779
      vertex 12.5 21.6506 0
    endloop
  endfacet
  facet normal 0.5846 0.804633 -0.103962
    outer loop
      vertex 16.3627 18.1726 -5.19779
      vertex 12.5 21.6506 0
      vertex 16.7283 18.5786 0
    endloop
  endfacet
  facet normal -0.644463 0.580277 0.497942
    outer loop
      vertex -13.5335 15.0304 14.6946
      vertex -18.4768 13.4242 10.1684
      vertex -16.3627 11.8882 14.6946
    endloop
  endfacet
  facet normal -0.55362 0.498482 0.667099
    outer loop
      vertex -13.5335 15.0304 14.6946
      vertex -13.5335 9.83263 18.5786
      vertex -11.1934 12.4315 18.5786
    endloop
  endfacet
  facet normal -0.180303 0.84826 0.497941
    outer loop
      vertex -3.86271 21.3904 11.8882
      vertex -2.11413 20.1146 14.6946
      vertex -2.26298 21.5308 12.2282
    endloop
  endfacet
  facet normal -0.180303 0.84826 0.497941
    outer loop
      vertex -6.25 21.5069 10.8253
      vertex -2.11413 20.1146 14.6946
      vertex -3.86271 21.3904 11.8882
    endloop
  endfacet
  facet normal -0.180306 0.848258 0.497943
    outer loop
      vertex -2.11413 20.1146 14.6946
      vertex -6.25 21.5069 10.8253
      vertex -6.68624 21.6002 10.5084
    endloop
  endfacet
  facet normal -0.707142 0.636713 0.307485
    outer loop
      vertex -16.3627 18.1726 5.19779
      vertex -19.7835 14.3735 5.19779
      vertex -18.4768 13.4242 10.1684
    endloop
  endfacet
  facet normal 0.239933 0.538899 0.807478
    outer loop
      vertex 5.16932 15.9095 18.5786
      vertex 3.86271 11.8882 21.6506
      vertex 6.25 10.8253 21.6506
    endloop
  endfacet
  facet normal 0.122647 0.577007 0.807478
    outer loop
      vertex 5.16932 15.9095 18.5786
      vertex 1.30661 12.4315 21.6506
      vertex 3.86271 11.8882 21.6506
    endloop
  endfacet
  facet normal 0.303006 0.680563 0.667099
    outer loop
      vertex 10.1127 17.5157 14.6946
      vertex 6.25 19.2355 14.6946
      vertex 8.36413 14.4871 18.5786
    endloop
  endfacet
  facet normal 0.303006 0.680563 0.667099
    outer loop
      vertex 6.25 19.2355 14.6946
      vertex 5.16932 15.9095 18.5786
      vertex 8.36413 14.4871 18.5786
    endloop
  endfacet
  facet normal 0 0.589898 0.807478
    outer loop
      vertex 1.74858 16.6366 18.5786
      vertex -1.30661 12.4315 21.6506
      vertex 1.30661 12.4315 21.6506
    endloop
  endfacet
  facet normal 0 0.589898 0.807478
    outer loop
      vertex -1.30661 12.4315 21.6506
      vertex 1.74858 16.6366 18.5786
      vertex -1.74858 16.6366 18.5786
    endloop
  endfacet
  facet normal 0.352726 -0.792236 0.497942
    outer loop
      vertex 7.04314 -21.6765 10.2491
      vertex 10.1127 -17.5157 14.6946
      vertex 6.25 -19.2355 14.6946
    endloop
  endfacet
  facet normal 0.35274 -0.792236 0.497932
    outer loop
      vertex 10.1127 -17.5157 14.6946
      vertex 7.04314 -21.6765 10.2491
      vertex 7.08932 -21.6771 10.2155
    endloop
  endfacet
  facet normal -0.180303 -0.84826 0.497942
    outer loop
      vertex -6.25 -19.2355 14.6946
      vertex -6.68624 -21.6002 10.5084
      vertex -2.11413 -20.1146 14.6946
    endloop
  endfacet
  facet normal -0.180304 -0.84826 0.497942
    outer loop
      vertex -6.68624 -21.6002 10.5084
      vertex -6.25 -19.2355 14.6946
      vertex -7.04314 -21.6765 10.2491
    endloop
  endfacet
  facet normal -0.180303 -0.84826 0.497941
    outer loop
      vertex -2.11413 -20.1146 14.6946
      vertex -3.86271 -21.3904 11.8882
      vertex -2.26298 -21.5308 12.2282
    endloop
  endfacet
  facet normal -0.180303 -0.84826 0.497941
    outer loop
      vertex -2.11413 -20.1146 14.6946
      vertex -6.25 -21.5069 10.8253
      vertex -3.86271 -21.3904 11.8882
    endloop
  endfacet
  facet normal -0.180306 -0.848258 0.497943
    outer loop
      vertex -6.25 -21.5069 10.8253
      vertex -2.11413 -20.1146 14.6946
      vertex -6.68624 -21.6002 10.5084
    endloop
  endfacet
  facet normal 0.559309 -0.769822 -0.307485
    outer loop
      vertex 16.3627 -18.1726 -5.19779
      vertex 11.4193 -19.7788 -10.1684
      vertex 15.282 -16.9724 -10.1684
    endloop
  endfacet
  facet normal 0.644463 -0.580277 0.497942
    outer loop
      vertex 18.4768 -13.4242 10.1684
      vertex 13.5335 -15.0304 14.6946
      vertex 15.282 -16.9724 10.1684
    endloop
  endfacet
  facet normal 0.346734 -0.477238 0.807478
    outer loop
      vertex 11.1934 -12.4315 18.5786
      vertex 6.25 -10.8253 21.6506
      vertex 8.36413 -14.4871 18.5786
    endloop
  endfacet
  facet normal 0.239933 -0.538899 0.807478
    outer loop
      vertex 6.25 -10.8253 21.6506
      vertex 5.16932 -15.9095 18.5786
      vertex 8.36413 -14.4871 18.5786
    endloop
  endfacet
  facet normal 0.509734 -0.701588 0.497942
    outer loop
      vertex 13.5335 -15.0304 14.6946
      vertex 11.4193 -19.7788 10.1684
      vertex 15.282 -16.9724 10.1684
    endloop
  endfacet
  facet normal -0.239933 -0.538899 0.807478
    outer loop
      vertex -3.86271 -11.8882 21.6506
      vertex -6.25 -10.8253 21.6506
      vertex -5.16932 -15.9095 18.5786
    endloop
  endfacet
  facet normal -0.122647 -0.577007 0.807478
    outer loop
      vertex -3.86271 -11.8882 21.6506
      vertex -5.16932 -15.9095 18.5786
      vertex -1.30661 -12.4315 21.6506
    endloop
  endfacet
  facet normal -0.55362 -0.498482 0.667099
    outer loop
      vertex -13.5335 -9.83263 18.5786
      vertex -16.3627 -11.8882 14.6946
      vertex -13.5335 -15.0304 14.6946
    endloop
  endfacet
  facet normal -0.645162 -0.372485 0.667099
    outer loop
      vertex -13.5335 -9.83263 18.5786
      vertex -18.4768 -8.22642 14.6946
      vertex -16.3627 -11.8882 14.6946
    endloop
  endfacet
  facet normal -0.561027 -0.182289 0.807478
    outer loop
      vertex -11.4193 -5.08421 21.6506
      vertex -16.3627 -3.478 18.5786
      vertex -15.282 -6.804 18.5786
    endloop
  endfacet
  facet normal -0.708508 -0.230208 0.667099
    outer loop
      vertex -16.3627 -3.478 18.5786
      vertex -18.4768 -8.22642 14.6946
      vertex -15.282 -6.804 18.5786
    endloop
  endfacet
  facet normal -0.751027 0.433605 0.497941
    outer loop
      vertex -18.4768 13.4242 10.1684
      vertex -20.8641 9.28931 10.1684
      vertex -18.4768 8.22642 14.6946
    endloop
  endfacet
  facet normal -0.644463 0.580277 0.497942
    outer loop
      vertex -15.282 16.9724 10.1684
      vertex -18.4768 13.4242 10.1684
      vertex -13.5335 15.0304 14.6946
    endloop
  endfacet
  facet normal -0.904981 -0.294046 0.307483
    outer loop
      vertex -21.5141 -7.34731 10.1127
      vertex -20.8641 -9.28931 10.1684
      vertex -21.5265 -7.25083 10.1684
    endloop
  endfacet
  facet normal -0.904981 -0.294046 0.307484
    outer loop
      vertex -20.8641 -9.28931 10.1684
      vertex -21.5141 -7.34731 10.1127
      vertex -21.4936 -8.42289 9.14426
    endloop
  endfacet
  facet normal -0.824766 -0.267982 0.497942
    outer loop
      vertex -21.5265 -7.25083 10.1684
      vertex -18.4768 -8.22642 14.6946
      vertex -21.4855 -5.51743 11.1692
    endloop
  endfacet
  facet normal -0.824766 -0.267983 0.497941
    outer loop
      vertex -18.4768 -8.22642 14.6946
      vertex -21.5265 -7.25083 10.1684
      vertex -20.8641 -9.28931 10.1684
    endloop
  endfacet
  facet normal -0.586667 0.0616612 0.807478
    outer loop
      vertex -12.2268 2.5989 21.6506
      vertex -16.7283 0 18.5786
      vertex -12.5 0 21.6506
    endloop
  endfacet
  facet normal -0.43838 0.394719 0.807478
    outer loop
      vertex -8.36413 9.28931 21.6506
      vertex -11.1934 12.4315 18.5786
      vertex -10.1127 7.34731 21.6506
    endloop
  endfacet
  facet normal -0.645162 0.372485 0.667099
    outer loop
      vertex -16.3627 11.8882 14.6946
      vertex -18.4768 8.22642 14.6946
      vertex -13.5335 9.83263 18.5786
    endloop
  endfacet
  facet normal -0.561027 0.182289 0.807478
    outer loop
      vertex -15.282 6.804 18.5786
      vertex -16.3627 3.478 18.5786
      vertex -11.4193 5.08421 21.6506
    endloop
  endfacet
  facet normal 0.586667 -0.0616609 -0.807478
    outer loop
      vertex 16.3627 -3.478 -18.5786
      vertex 12.2268 -2.5989 -21.6506
      vertex 16.7283 0 -18.5786
    endloop
  endfacet
  facet normal 0.740888 -0.0778701 -0.667099
    outer loop
      vertex 20.2254 0 -14.6946
      vertex 16.3627 -3.478 -18.5786
      vertex 16.7283 0 -18.5786
    endloop
  endfacet
  facet normal 0.510867 -0.294949 -0.807478
    outer loop
      vertex 13.5335 -9.83263 -18.5786
      vertex 10.1127 -7.34731 -21.6506
      vertex 11.4193 -5.08421 -21.6506
    endloop
  endfacet
  facet normal 0.5846 -0.804633 -0.103962
    outer loop
      vertex 16.7283 -18.5786 0
      vertex 12.5 -21.6506 0
      vertex 16.3627 -18.1726 -5.19779
    endloop
  endfacet
  facet normal 0.739118 -0.665505 -0.103962
    outer loop
      vertex 16.7283 -18.5786 0
      vertex 16.3627 -18.1726 -5.19779
      vertex 20.2254 -14.6946 0
    endloop
  endfacet
  facet normal 0.751027 -0.433605 0.497941
    outer loop
      vertex 18.4768 -8.22642 14.6946
      vertex 18.4768 -13.4242 10.1684
      vertex 20.8641 -9.28931 10.1684
    endloop
  endfacet
  facet normal 0.824069 -0.475776 0.307485
    outer loop
      vertex 18.4768 -13.4242 10.1684
      vertex 21.5188 -9.58077 7.96297
      vertex 20.8641 -9.28931 10.1684
    endloop
  endfacet
  facet normal 0.824069 -0.475777 0.307484
    outer loop
      vertex 21.5188 -9.58077 7.96297
      vertex 18.4768 -13.4242 10.1684
      vertex 21.4547 -10.5846 6.58134
    endloop
  endfacet
  facet normal 0.82407 -0.475774 0.307486
    outer loop
      vertex 21.4547 -10.5846 6.58134
      vertex 18.4768 -13.4242 10.1684
      vertex 21.444 -10.7525 6.35017
    endloop
  endfacet
  facet normal -0.559309 -0.769823 0.307485
    outer loop
      vertex -11.4193 -19.7788 10.1684
      vertex -16.3627 -18.1726 5.19779
      vertex -12.2268 -21.1775 5.19779
    endloop
  endfacet
  facet normal -0.559309 -0.769822 0.307485
    outer loop
      vertex -15.282 -16.9724 10.1684
      vertex -16.3627 -18.1726 5.19779
      vertex -11.4193 -19.7788 10.1684
    endloop
  endfacet
  facet normal -0.644463 -0.580277 0.497942
    outer loop
      vertex -13.5335 -15.0304 14.6946
      vertex -18.4768 -13.4242 10.1684
      vertex -15.282 -16.9724 10.1684
    endloop
  endfacet
  facet normal -0.5846 -0.804633 0.103962
    outer loop
      vertex -12.2268 -21.1775 5.19779
      vertex -16.3627 -18.1726 5.19779
      vertex -12.5 -21.6506 0
    endloop
  endfacet
  facet normal -0.824069 0.475776 -0.307485
    outer loop
      vertex -21.4394 10.8253 -6.25
      vertex -19.7835 14.3735 -5.19779
      vertex -18.4768 13.4242 -10.1684
    endloop
  endfacet
  facet normal -0.82407 0.475776 -0.307484
    outer loop
      vertex -19.7835 14.3735 -5.19779
      vertex -21.4827 10.9914 -5.87692
      vertex -21.5615 11.2938 -5.19779
    endloop
  endfacet
  facet normal -0.824069 0.475776 -0.307485
    outer loop
      vertex -19.7835 14.3735 -5.19779
      vertex -21.4394 10.8253 -6.25
      vertex -21.4827 10.9914 -5.87692
    endloop
  endfacet
  facet normal -0.824068 0.475781 -0.307481
    outer loop
      vertex -21.4394 10.8253 -6.25
      vertex -18.4768 13.4242 -10.1684
      vertex -21.444 10.7525 -6.35017
    endloop
  endfacet
  facet normal -0.82407 0.475774 -0.307486
    outer loop
      vertex -18.4768 13.4242 -10.1684
      vertex -21.4547 10.5846 -6.58134
      vertex -21.444 10.7525 -6.35017
    endloop
  endfacet
  facet normal -0.824069 0.475777 -0.307484
    outer loop
      vertex -18.4768 13.4242 -10.1684
      vertex -21.5188 9.58077 -7.96297
      vertex -21.4547 10.5846 -6.58134
    endloop
  endfacet
  facet normal -0.824069 0.475776 -0.307485
    outer loop
      vertex -21.5188 9.58077 -7.96297
      vertex -18.4768 13.4242 -10.1684
      vertex -20.8641 9.28931 -10.1684
    endloop
  endfacet
  facet normal -0.43838 0.394719 -0.807478
    outer loop
      vertex -10.1127 7.34731 -21.6506
      vertex -11.1934 12.4315 -18.5786
      vertex -8.36413 9.28931 -21.6506
    endloop
  endfacet
  facet normal -0.346734 0.477238 -0.807478
    outer loop
      vertex -8.36413 9.28931 -21.6506
      vertex -11.1934 12.4315 -18.5786
      vertex -6.25 10.8253 -21.6506
    endloop
  endfacet
  facet normal 0.346734 0.477238 -0.807478
    outer loop
      vertex 8.36413 9.28931 -21.6506
      vertex 6.25 10.8253 -21.6506
      vertex 11.1934 12.4315 -18.5786
    endloop
  endfacet
  facet normal 0.43838 0.394719 -0.807478
    outer loop
      vertex 10.1127 7.34731 -21.6506
      vertex 8.36413 9.28931 -21.6506
      vertex 11.1934 12.4315 -18.5786
    endloop
  endfacet
  facet normal -0.122647 0.577007 -0.807478
    outer loop
      vertex -3.86271 11.8882 -21.6506
      vertex -5.16932 15.9095 -18.5786
      vertex -1.30661 12.4315 -21.6506
    endloop
  endfacet
  facet normal -0.352714 0.792237 -0.49795
    outer loop
      vertex -10.1127 17.5157 -14.6946
      vertex -7.15415 21.6778 -10.1684
      vertex -7.08932 21.6771 -10.2155
    endloop
  endfacet
  facet normal -0.352726 0.792236 -0.497942
    outer loop
      vertex -7.15415 21.6778 -10.1684
      vertex -10.1127 17.5157 -14.6946
      vertex -11.4193 19.7788 -10.1684
    endloop
  endfacet
  facet normal -0.387031 -0.869287 -0.307484
    outer loop
      vertex -12.2268 -21.1775 -5.19779
      vertex -10.1127 -21.3585 -7.34731
      vertex -11.3537 -21.5662 -5.19779
    endloop
  endfacet
  facet normal -0.387032 -0.869287 -0.307485
    outer loop
      vertex -11.4193 -19.7788 -10.1684
      vertex -10.1127 -21.3585 -7.34731
      vertex -12.2268 -21.1775 -5.19779
    endloop
  endfacet
  facet normal -0.387032 -0.869287 -0.307485
    outer loop
      vertex -10.1127 -21.3585 -7.34731
      vertex -11.4193 -19.7788 -10.1684
      vertex -9.63732 -21.3834 -7.8753
    endloop
  endfacet
  facet normal -0.404532 -0.908595 -0.103962
    outer loop
      vertex -12.2268 -21.1775 -5.19779
      vertex -12.2268 -21.4749 -2.5989
      vertex -12.5 -21.6506 0
    endloop
  endfacet
  facet normal -0.404532 -0.908595 -0.103962
    outer loop
      vertex -12.2268 -21.1775 -5.19779
      vertex -11.4193 -21.55 -5.08421
      vertex -12.2268 -21.4749 -2.5989
    endloop
  endfacet
  facet normal -0.404532 -0.908595 -0.103962
    outer loop
      vertex -11.4193 -21.55 -5.08421
      vertex -12.2268 -21.1775 -5.19779
      vertex -11.3537 -21.5662 -5.19779
    endloop
  endfacet
  facet normal -0.510867 -0.294949 -0.807478
    outer loop
      vertex -13.5335 -9.83263 -18.5786
      vertex -15.282 -6.804 -18.5786
      vertex -11.4193 -5.08421 -21.6506
    endloop
  endfacet
  facet normal -0.561027 -0.182289 -0.807478
    outer loop
      vertex -11.4193 -5.08421 -21.6506
      vertex -16.3627 -3.478 -18.5786
      vertex -12.2268 -2.5989 -21.6506
    endloop
  endfacet
  facet normal -0.239933 -0.538899 -0.807478
    outer loop
      vertex -5.16932 -15.9095 -18.5786
      vertex -8.36413 -14.4871 -18.5786
      vertex -6.25 -10.8253 -21.6506
    endloop
  endfacet
  facet normal -0.586667 0.0616612 -0.807478
    outer loop
      vertex -12.5 0 -21.6506
      vertex -16.7283 0 -18.5786
      vertex -12.2268 2.5989 -21.6506
    endloop
  endfacet
  facet normal 0.122647 -0.577007 -0.807478
    outer loop
      vertex 5.16932 -15.9095 -18.5786
      vertex 1.30661 -12.4315 -21.6506
      vertex 3.86271 -11.8882 -21.6506
    endloop
  endfacet
  facet normal 0.180314 -0.848253 -0.49795
    outer loop
      vertex 6.25 -21.5069 -10.8253
      vertex 2.11413 -20.1146 -14.6946
      vertex 6.68623 -21.6002 -10.5084
    endloop
  endfacet
  facet normal 0.180303 -0.84826 -0.497941
    outer loop
      vertex 3.86271 -21.3904 -11.8882
      vertex 2.11413 -20.1146 -14.6946
      vertex 6.25 -21.5069 -10.8253
    endloop
  endfacet
  facet normal 0.180303 -0.84826 -0.497941
    outer loop
      vertex 2.11413 -20.1146 -14.6946
      vertex 3.86271 -21.3904 -11.8882
      vertex 2.26298 -21.5308 -12.2282
    endloop
  endfacet
  facet normal -0.559309 -0.769823 -0.307485
    outer loop
      vertex -12.2268 -21.1775 -5.19779
      vertex -16.3627 -18.1726 -5.19779
      vertex -11.4193 -19.7788 -10.1684
    endloop
  endfacet
  facet normal 0.346734 -0.477238 -0.807478
    outer loop
      vertex 11.1934 -12.4315 -18.5786
      vertex 6.25 -10.8253 -21.6506
      vertex 8.36413 -9.28931 -21.6506
    endloop
  endfacet
  facet normal -0.122647 0.577008 -0.807478
    outer loop
      vertex -1.30661 12.4315 -21.6506
      vertex -5.16932 15.9095 -18.5786
      vertex -1.74858 16.6366 -18.5786
    endloop
  endfacet
  facet normal -0.154888 0.72869 -0.667099
    outer loop
      vertex -1.74858 16.6366 -18.5786
      vertex -5.16932 15.9095 -18.5786
      vertex -2.11413 20.1146 -14.6946
    endloop
  endfacet
  facet normal -0.122647 0.577007 0.807478
    outer loop
      vertex -1.30661 12.4315 21.6506
      vertex -5.16932 15.9095 18.5786
      vertex -3.86271 11.8882 21.6506
    endloop
  endfacet
  facet normal 0.43838 0.394719 0.807478
    outer loop
      vertex 11.1934 12.4315 18.5786
      vertex 8.36413 9.28931 21.6506
      vertex 10.1127 7.34731 21.6506
    endloop
  endfacet
  facet normal 0.904981 0.294046 -0.307484
    outer loop
      vertex 21.5141 7.34731 -10.1127
      vertex 20.8641 9.28931 -10.1684
      vertex 21.4936 8.42289 -9.14426
    endloop
  endfacet
  facet normal 0.904981 0.294046 -0.307483
    outer loop
      vertex 20.8641 9.28931 -10.1684
      vertex 21.5141 7.34731 -10.1127
      vertex 21.5265 7.25083 -10.1684
    endloop
  endfacet
  facet normal 0.824766 0.267983 -0.497941
    outer loop
      vertex 21.5265 7.25083 -10.1684
      vertex 18.4768 8.22642 -14.6946
      vertex 20.8641 9.28931 -10.1684
    endloop
  endfacet
  facet normal 0.824766 0.267982 -0.497942
    outer loop
      vertex 18.4768 8.22642 -14.6946
      vertex 21.5265 7.25083 -10.1684
      vertex 21.4855 5.51743 -11.1692
    endloop
  endfacet
  facet normal 0.904981 -0.294046 -0.307483
    outer loop
      vertex 21.5141 -7.34731 -10.1127
      vertex 20.8641 -9.28931 -10.1684
      vertex 21.5265 -7.25083 -10.1684
    endloop
  endfacet
  facet normal 0.904981 -0.294046 -0.307484
    outer loop
      vertex 20.8641 -9.28931 -10.1684
      vertex 21.5141 -7.34731 -10.1127
      vertex 21.4936 -8.42289 -9.14426
    endloop
  endfacet
  facet normal -0.437882 0.602693 0.667099
    outer loop
      vertex -8.36413 14.4871 18.5786
      vertex -13.5335 15.0304 14.6946
      vertex -11.1934 12.4315 18.5786
    endloop
  endfacet
  facet normal -0.509734 0.701588 0.497942
    outer loop
      vertex -11.4193 19.7788 10.1684
      vertex -15.282 16.9724 10.1684
      vertex -13.5335 15.0304 14.6946
    endloop
  endfacet
  facet normal -0.387032 0.869287 -0.307485
    outer loop
      vertex -8.36413 21.4501 -9.28931
      vertex -11.4193 19.7788 -10.1684
      vertex -9.63732 21.3834 -7.8753
    endloop
  endfacet
  facet normal -0.387032 0.869287 -0.307485
    outer loop
      vertex -11.4193 19.7788 -10.1684
      vertex -8.36413 21.4501 -9.28931
      vertex -7.15415 21.6778 -10.1684
    endloop
  endfacet
  facet normal -0.387031 0.869287 -0.307484
    outer loop
      vertex -10.1127 21.3585 -7.34731
      vertex -12.2268 21.1775 -5.19779
      vertex -11.3537 21.5662 -5.19779
    endloop
  endfacet
  facet normal -0.387032 0.869287 -0.307485
    outer loop
      vertex -11.4193 19.7788 -10.1684
      vertex -10.1127 21.3585 -7.34731
      vertex -9.63732 21.3834 -7.8753
    endloop
  endfacet
  facet normal -0.387032 0.869287 -0.307485
    outer loop
      vertex -10.1127 21.3585 -7.34731
      vertex -11.4193 19.7788 -10.1684
      vertex -12.2268 21.1775 -5.19779
    endloop
  endfacet
  facet normal -0.5846 0.804633 -0.103962
    outer loop
      vertex -16.3627 18.1726 -5.19779
      vertex -12.5 21.6506 -1.81471e-05
      vertex -12.2268 21.1775 -5.19779
    endloop
  endfacet
  facet normal -0.539377 0.828009 -0.153211
    outer loop
      vertex -12.5 21.6506 -1.81471e-05
      vertex -16.3627 18.1726 -5.19779
      vertex -12.5 21.6506 -1.12429e-06
    endloop
  endfacet
  facet normal -0.5846 0.804633 0.103962
    outer loop
      vertex -12.5 21.6506 1.81471e-05
      vertex -16.3627 18.1726 5.19779
      vertex -12.2268 21.1775 5.19779
    endloop
  endfacet
  facet normal -0.539377 0.828009 0.153211
    outer loop
      vertex -16.3627 18.1726 5.19779
      vertex -12.5 21.6506 1.81471e-05
      vertex -12.5 21.6506 1.12429e-06
    endloop
  endfacet
  facet normal -0.154888 0.72869 0.667099
    outer loop
      vertex -2.11413 20.1146 14.6946
      vertex -5.16932 15.9095 18.5786
      vertex -1.74858 16.6366 18.5786
    endloop
  endfacet
  facet normal -0.387031 0.869287 0.307485
    outer loop
      vertex -8.36413 21.4501 9.28931
      vertex -11.4193 19.7788 10.1684
      vertex -7.15415 21.6778 10.1684
    endloop
  endfacet
  facet normal -0.387032 0.869287 0.307485
    outer loop
      vertex -11.4193 19.7788 10.1684
      vertex -8.36413 21.4501 9.28931
      vertex -9.63732 21.3834 7.8753
    endloop
  endfacet
  facet normal -0.559309 0.769823 0.307485
    outer loop
      vertex -12.2268 21.1775 5.19779
      vertex -16.3627 18.1726 5.19779
      vertex -11.4193 19.7788 10.1684
    endloop
  endfacet
  facet normal 0.303006 -0.680563 -0.667099
    outer loop
      vertex 10.1127 -17.5157 -14.6946
      vertex 6.25 -19.2355 -14.6946
      vertex 8.36413 -14.4871 -18.5786
    endloop
  endfacet
  facet normal 0.509734 -0.701588 0.497942
    outer loop
      vertex 13.5335 -15.0304 14.6946
      vertex 10.1127 -17.5157 14.6946
      vertex 11.4193 -19.7788 10.1684
    endloop
  endfacet
  facet normal 0.437882 -0.602693 0.667099
    outer loop
      vertex 13.5335 -15.0304 14.6946
      vertex 8.36413 -14.4871 18.5786
      vertex 10.1127 -17.5157 14.6946
    endloop
  endfacet
  facet normal 0.180303 -0.84826 0.497941
    outer loop
      vertex 3.86271 -21.3904 11.8882
      vertex 2.11413 -20.1146 14.6946
      vertex 2.26298 -21.5308 12.2282
    endloop
  endfacet
  facet normal 0.180303 -0.84826 0.497941
    outer loop
      vertex 6.25 -21.5069 10.8253
      vertex 2.11413 -20.1146 14.6946
      vertex 3.86271 -21.3904 11.8882
    endloop
  endfacet
  facet normal 0.180314 -0.848253 0.49795
    outer loop
      vertex 2.11413 -20.1146 14.6946
      vertex 6.25 -21.5069 10.8253
      vertex 6.68623 -21.6002 10.5084
    endloop
  endfacet
  facet normal 0.180303 -0.84826 0.497942
    outer loop
      vertex 6.68623 -21.6002 10.5084
      vertex 6.25 -19.2355 14.6946
      vertex 2.11413 -20.1146 14.6946
    endloop
  endfacet
  facet normal 0.180301 -0.84826 0.497942
    outer loop
      vertex 6.25 -19.2355 14.6946
      vertex 6.68623 -21.6002 10.5084
      vertex 7.04314 -21.6765 10.2491
    endloop
  endfacet
  facet normal 0.43838 -0.394719 0.807478
    outer loop
      vertex 13.5335 -9.83263 18.5786
      vertex 10.1127 -7.34731 21.6506
      vertex 11.1934 -12.4315 18.5786
    endloop
  endfacet
  facet normal 0.122647 -0.577007 0.807478
    outer loop
      vertex 3.86271 -11.8882 21.6506
      vertex 1.30661 -12.4315 21.6506
      vertex 5.16932 -15.9095 18.5786
    endloop
  endfacet
  facet normal 0.644463 -0.580277 0.497942
    outer loop
      vertex 16.3627 -11.8882 14.6946
      vertex 13.5335 -15.0304 14.6946
      vertex 18.4768 -13.4242 10.1684
    endloop
  endfacet
  facet normal 0.55362 -0.498482 0.667099
    outer loop
      vertex 13.5335 -9.83263 18.5786
      vertex 13.5335 -15.0304 14.6946
      vertex 16.3627 -11.8882 14.6946
    endloop
  endfacet
  facet normal -0.824766 0.267983 0.497941
    outer loop
      vertex -21.5265 7.25083 10.1684
      vertex -18.4768 8.22642 14.6946
      vertex -20.8641 9.28931 10.1684
    endloop
  endfacet
  facet normal -0.824766 0.267982 0.497942
    outer loop
      vertex -18.4768 8.22642 14.6946
      vertex -21.5265 7.25083 10.1684
      vertex -21.4855 5.51743 11.1692
    endloop
  endfacet
  facet normal -0.824766 0.267983 0.497941
    outer loop
      vertex -21.4752 5.08421 11.4193
      vertex -18.4768 8.22642 14.6946
      vertex -21.4855 5.51743 11.1692
    endloop
  endfacet
  facet normal -0.824766 0.267984 0.497941
    outer loop
      vertex -19.7835 4.2051 14.6946
      vertex -21.4752 5.08421 11.4193
      vertex -21.5403 4.57854 11.5836
    endloop
  endfacet
  facet normal -0.824766 0.267983 0.497941
    outer loop
      vertex -21.4752 5.08421 11.4193
      vertex -19.7835 4.2051 14.6946
      vertex -18.4768 8.22642 14.6946
    endloop
  endfacet
  facet normal -0.559309 0.769822 0.307485
    outer loop
      vertex -11.4193 19.7788 10.1684
      vertex -16.3627 18.1726 5.19779
      vertex -15.282 16.9724 10.1684
    endloop
  endfacet
  facet normal -0.824069 0.475776 0.307485
    outer loop
      vertex -21.4394 10.8253 6.25
      vertex -19.7835 14.3735 5.19779
      vertex -21.5615 11.2938 5.19779
    endloop
  endfacet
  facet normal -0.824069 0.475776 0.307485
    outer loop
      vertex -19.7835 14.3735 5.19779
      vertex -21.4394 10.8253 6.25
      vertex -18.4768 13.4242 10.1684
    endloop
  endfacet
  facet normal -0.824069 0.475779 0.307482
    outer loop
      vertex -18.4768 13.4242 10.1684
      vertex -21.4394 10.8253 6.25
      vertex -21.444 10.7525 6.35017
    endloop
  endfacet
  facet normal -0.43838 0.394719 0.807478
    outer loop
      vertex -11.1934 12.4315 18.5786
      vertex -13.5335 9.83263 18.5786
      vertex -10.1127 7.34731 21.6506
    endloop
  endfacet
  facet normal -0.510867 0.294949 0.807478
    outer loop
      vertex -10.1127 7.34731 21.6506
      vertex -13.5335 9.83263 18.5786
      vertex -11.4193 5.08421 21.6506
    endloop
  endfacet
  facet normal 0.346734 -0.477238 0.807478
    outer loop
      vertex 8.36413 -9.28931 21.6506
      vertex 6.25 -10.8253 21.6506
      vertex 11.1934 -12.4315 18.5786
    endloop
  endfacet
  facet normal -0.707142 -0.636713 0.307485
    outer loop
      vertex -18.4768 -13.4242 10.1684
      vertex -19.7835 -14.3735 5.19779
      vertex -16.3627 -18.1726 5.19779
    endloop
  endfacet
  facet normal -0.739118 -0.665505 0.103962
    outer loop
      vertex -19.7835 -14.3735 5.19779
      vertex -20.2254 -14.6946 0
      vertex -16.3627 -18.1726 5.19779
    endloop
  endfacet
  facet normal -0.509734 -0.701588 -0.497942
    outer loop
      vertex -11.4193 -19.7788 -10.1684
      vertex -15.282 -16.9724 -10.1684
      vertex -13.5335 -15.0304 -14.6946
    endloop
  endfacet
  facet normal -0.559309 -0.769822 -0.307485
    outer loop
      vertex -11.4193 -19.7788 -10.1684
      vertex -16.3627 -18.1726 -5.19779
      vertex -15.282 -16.9724 -10.1684
    endloop
  endfacet
  facet normal 0.303006 -0.680563 -0.667099
    outer loop
      vertex 6.25 -19.2355 -14.6946
      vertex 5.16932 -15.9095 -18.5786
      vertex 8.36413 -14.4871 -18.5786
    endloop
  endfacet
  facet normal -0.43838 -0.394719 -0.807478
    outer loop
      vertex -11.1934 -12.4315 -18.5786
      vertex -13.5335 -9.83263 -18.5786
      vertex -10.1127 -7.34731 -21.6506
    endloop
  endfacet
  facet normal -0.55362 -0.498482 -0.667099
    outer loop
      vertex -11.1934 -12.4315 -18.5786
      vertex -13.5335 -15.0304 -14.6946
      vertex -13.5335 -9.83263 -18.5786
    endloop
  endfacet
  facet normal 0.239933 -0.538899 -0.807478
    outer loop
      vertex 8.36413 -14.4871 -18.5786
      vertex 5.16932 -15.9095 -18.5786
      vertex 6.25 -10.8253 -21.6506
    endloop
  endfacet
  facet normal 0.346734 -0.477238 -0.807478
    outer loop
      vertex 8.36413 -14.4871 -18.5786
      vertex 6.25 -10.8253 -21.6506
      vertex 11.1934 -12.4315 -18.5786
    endloop
  endfacet
  facet normal -0.437882 -0.602693 -0.667099
    outer loop
      vertex -10.1127 -17.5157 -14.6946
      vertex -13.5335 -15.0304 -14.6946
      vertex -8.36413 -14.4871 -18.5786
    endloop
  endfacet
  facet normal -0.154888 -0.72869 -0.667099
    outer loop
      vertex -2.11413 -20.1146 -14.6946
      vertex -6.25 -19.2355 -14.6946
      vertex -5.16932 -15.9095 -18.5786
    endloop
  endfacet
  facet normal -0.239933 -0.538899 -0.807478
    outer loop
      vertex -5.16932 -15.9095 -18.5786
      vertex -6.25 -10.8253 -21.6506
      vertex -3.86271 -11.8882 -21.6506
    endloop
  endfacet
  facet normal 0 -0.589898 -0.807478
    outer loop
      vertex 1.74858 -16.6366 -18.5786
      vertex -1.30661 -12.4315 -21.6506
      vertex 1.30661 -12.4315 -21.6506
    endloop
  endfacet
  facet normal 0 -0.589898 -0.807478
    outer loop
      vertex -1.30661 -12.4315 -21.6506
      vertex 1.74858 -16.6366 -18.5786
      vertex -1.74858 -16.6366 -18.5786
    endloop
  endfacet
  facet normal 0.43838 -0.394719 -0.807478
    outer loop
      vertex 11.1934 -12.4315 -18.5786
      vertex 8.36413 -9.28931 -21.6506
      vertex 10.1127 -7.34731 -21.6506
    endloop
  endfacet
  facet normal -0.303006 0.680563 0.667099
    outer loop
      vertex -6.25 19.2355 14.6946
      vertex -10.1127 17.5157 14.6946
      vertex -8.36413 14.4871 18.5786
    endloop
  endfacet
  facet normal -0.303006 0.680563 0.667099
    outer loop
      vertex -6.25 19.2355 14.6946
      vertex -8.36413 14.4871 18.5786
      vertex -5.16932 15.9095 18.5786
    endloop
  endfacet
  facet normal -0.587785 0.809017 0
    outer loop
      vertex -12.5 21.6506 -1.12429e-06
      vertex -16.7283 18.5786 0
      vertex -12.5 21.6506 0
    endloop
  endfacet
  facet normal -0.5846 0.804633 -0.103962
    outer loop
      vertex -16.7283 18.5786 0
      vertex -12.5 21.6506 -1.12429e-06
      vertex -16.3627 18.1726 -5.19779
    endloop
  endfacet
  facet normal -0.739118 0.665505 -0.103962
    outer loop
      vertex -16.3627 18.1726 -5.19779
      vertex -20.2254 14.6946 0
      vertex -16.7283 18.5786 0
    endloop
  endfacet
  facet normal -0.509734 0.701588 0.497942
    outer loop
      vertex -11.4193 19.7788 10.1684
      vertex -13.5335 15.0304 14.6946
      vertex -10.1127 17.5157 14.6946
    endloop
  endfacet
  facet normal -0.352733 0.792236 0.497937
    outer loop
      vertex -7.15415 21.6778 10.1684
      vertex -10.1127 17.5157 14.6946
      vertex -7.08932 21.6771 10.2155
    endloop
  endfacet
  facet normal -0.352726 0.792236 0.497942
    outer loop
      vertex -10.1127 17.5157 14.6946
      vertex -7.15415 21.6778 10.1684
      vertex -11.4193 19.7788 10.1684
    endloop
  endfacet
  facet normal 0.352714 -0.792237 -0.49795
    outer loop
      vertex 10.1127 -17.5157 -14.6946
      vertex 7.15415 -21.6778 -10.1684
      vertex 7.08932 -21.6771 -10.2155
    endloop
  endfacet
  facet normal 0.352726 -0.792236 -0.497942
    outer loop
      vertex 7.15415 -21.6778 -10.1684
      vertex 10.1127 -17.5157 -14.6946
      vertex 11.4193 -19.7788 -10.1684
    endloop
  endfacet
  facet normal 0.387031 -0.869287 -0.307484
    outer loop
      vertex 10.1127 -21.3585 -7.34731
      vertex 12.2268 -21.1775 -5.19779
      vertex 11.3537 -21.5662 -5.19779
    endloop
  endfacet
  facet normal 0.387032 -0.869287 -0.307485
    outer loop
      vertex 11.4193 -19.7788 -10.1684
      vertex 10.1127 -21.3585 -7.34731
      vertex 9.63732 -21.3834 -7.8753
    endloop
  endfacet
  facet normal 0.387032 -0.869287 -0.307485
    outer loop
      vertex 10.1127 -21.3585 -7.34731
      vertex 11.4193 -19.7788 -10.1684
      vertex 12.2268 -21.1775 -5.19779
    endloop
  endfacet
  facet normal 0.5846 -0.804633 0.103962
    outer loop
      vertex 16.3627 -18.1726 5.19779
      vertex 12.2268 -21.1775 5.19779
      vertex 12.5 -21.6506 0
    endloop
  endfacet
  facet normal 0.559309 -0.769823 0.307485
    outer loop
      vertex 16.3627 -18.1726 5.19779
      vertex 11.4193 -19.7788 10.1684
      vertex 12.2268 -21.1775 5.19779
    endloop
  endfacet
  facet normal 0.303006 -0.680563 0.667099
    outer loop
      vertex 8.36413 -14.4871 18.5786
      vertex 6.25 -19.2355 14.6946
      vertex 10.1127 -17.5157 14.6946
    endloop
  endfacet
  facet normal 0.352714 -0.792237 0.49795
    outer loop
      vertex 7.15415 -21.6778 10.1684
      vertex 10.1127 -17.5157 14.6946
      vertex 7.08932 -21.6771 10.2155
    endloop
  endfacet
  facet normal 0.352726 -0.792236 0.497942
    outer loop
      vertex 10.1127 -17.5157 14.6946
      vertex 7.15415 -21.6778 10.1684
      vertex 11.4193 -19.7788 10.1684
    endloop
  endfacet
  facet normal 0.43838 -0.394719 0.807478
    outer loop
      vertex 10.1127 -7.34731 21.6506
      vertex 8.36413 -9.28931 21.6506
      vertex 11.1934 -12.4315 18.5786
    endloop
  endfacet
  facet normal 0.437882 -0.602693 0.667099
    outer loop
      vertex 11.1934 -12.4315 18.5786
      vertex 8.36413 -14.4871 18.5786
      vertex 13.5335 -15.0304 14.6946
    endloop
  endfacet
  facet normal 0.55362 -0.498482 0.667099
    outer loop
      vertex 13.5335 -9.83263 18.5786
      vertex 11.1934 -12.4315 18.5786
      vertex 13.5335 -15.0304 14.6946
    endloop
  endfacet
  facet normal 0.122647 -0.577008 0.807478
    outer loop
      vertex 5.16932 -15.9095 18.5786
      vertex 1.30661 -12.4315 21.6506
      vertex 1.74858 -16.6366 18.5786
    endloop
  endfacet
  facet normal 0.154888 -0.72869 0.667099
    outer loop
      vertex 5.16932 -15.9095 18.5786
      vertex 1.74858 -16.6366 18.5786
      vertex 2.11413 -20.1146 14.6946
    endloop
  endfacet
  facet normal -0.180303 -0.84826 -0.497941
    outer loop
      vertex -3.86271 -21.3904 -11.8882
      vertex -2.11413 -20.1146 -14.6946
      vertex -2.26298 -21.5308 -12.2282
    endloop
  endfacet
  facet normal -0.180303 -0.84826 -0.497941
    outer loop
      vertex -6.25 -21.5069 -10.8253
      vertex -2.11413 -20.1146 -14.6946
      vertex -3.86271 -21.3904 -11.8882
    endloop
  endfacet
  facet normal -0.180314 -0.848253 -0.49795
    outer loop
      vertex -2.11413 -20.1146 -14.6946
      vertex -6.25 -21.5069 -10.8253
      vertex -6.68623 -21.6002 -10.5084
    endloop
  endfacet
  facet normal -0.707142 -0.636713 -0.307485
    outer loop
      vertex -16.3627 -18.1726 -5.19779
      vertex -18.4768 -13.4242 -10.1684
      vertex -15.282 -16.9724 -10.1684
    endloop
  endfacet
  facet normal -0.644463 -0.580277 -0.497942
    outer loop
      vertex -15.282 -16.9724 -10.1684
      vertex -18.4768 -13.4242 -10.1684
      vertex -13.5335 -15.0304 -14.6946
    endloop
  endfacet
  facet normal 0.122647 -0.577008 -0.807478
    outer loop
      vertex 1.74858 -16.6366 -18.5786
      vertex 1.30661 -12.4315 -21.6506
      vertex 5.16932 -15.9095 -18.5786
    endloop
  endfacet
  facet normal -0.645162 -0.372485 -0.667099
    outer loop
      vertex -13.5335 -9.83263 -18.5786
      vertex -18.4768 -8.22642 -14.6946
      vertex -15.282 -6.804 -18.5786
    endloop
  endfacet
  facet normal -0.708508 -0.230208 -0.667099
    outer loop
      vertex -15.282 -6.804 -18.5786
      vertex -18.4768 -8.22642 -14.6946
      vertex -16.3627 -3.478 -18.5786
    endloop
  endfacet
  facet normal 0.437882 -0.602693 -0.667099
    outer loop
      vertex 10.1127 -17.5157 -14.6946
      vertex 8.36413 -14.4871 -18.5786
      vertex 13.5335 -15.0304 -14.6946
    endloop
  endfacet
  facet normal 0.559309 -0.769823 -0.307485
    outer loop
      vertex 12.2268 -21.1775 -5.19779
      vertex 11.4193 -19.7788 -10.1684
      vertex 16.3627 -18.1726 -5.19779
    endloop
  endfacet
  facet normal -0.346734 -0.477238 -0.807478
    outer loop
      vertex -8.36413 -14.4871 -18.5786
      vertex -11.1934 -12.4315 -18.5786
      vertex -6.25 -10.8253 -21.6506
    endloop
  endfacet
  facet normal -0.346734 -0.477238 -0.807478
    outer loop
      vertex -6.25 -10.8253 -21.6506
      vertex -11.1934 -12.4315 -18.5786
      vertex -8.36413 -9.28931 -21.6506
    endloop
  endfacet
  facet normal -0.154888 -0.72869 -0.667099
    outer loop
      vertex -2.11413 -20.1146 -14.6946
      vertex -5.16932 -15.9095 -18.5786
      vertex -1.74858 -16.6366 -18.5786
    endloop
  endfacet
  facet normal -0.154888 0.72869 0.667099
    outer loop
      vertex -2.11413 20.1146 14.6946
      vertex -6.25 19.2355 14.6946
      vertex -5.16932 15.9095 18.5786
    endloop
  endfacet
  facet normal -0.180303 0.84826 0.497942
    outer loop
      vertex -6.68624 21.6002 10.5084
      vertex -6.25 19.2355 14.6946
      vertex -2.11413 20.1146 14.6946
    endloop
  endfacet
  facet normal -0.180304 0.84826 0.497942
    outer loop
      vertex -6.25 19.2355 14.6946
      vertex -6.68624 21.6002 10.5084
      vertex -7.04314 21.6765 10.2491
    endloop
  endfacet
  facet normal -0.587785 0.809017 0
    outer loop
      vertex -16.7283 18.5786 0
      vertex -12.5 21.6506 1.12429e-06
      vertex -12.5 21.6506 0
    endloop
  endfacet
  facet normal -0.5846 0.804633 0.103962
    outer loop
      vertex -12.5 21.6506 1.12429e-06
      vertex -16.7283 18.5786 0
      vertex -16.3627 18.1726 5.19779
    endloop
  endfacet
  facet normal -0.739118 0.665505 0.103962
    outer loop
      vertex -16.7283 18.5786 0
      vertex -20.2254 14.6946 0
      vertex -16.3627 18.1726 5.19779
    endloop
  endfacet
  facet normal 0 -0.744969 -0.667099
    outer loop
      vertex -1.74858 -16.6366 -18.5786
      vertex 2.11413 -20.1146 -14.6946
      vertex -2.11413 -20.1146 -14.6946
    endloop
  endfacet
  facet normal 0 -0.744969 -0.667099
    outer loop
      vertex 2.11413 -20.1146 -14.6946
      vertex -1.74858 -16.6366 -18.5786
      vertex 1.74858 -16.6366 -18.5786
    endloop
  endfacet
  facet normal 0.154888 -0.72869 -0.667099
    outer loop
      vertex 2.11413 -20.1146 -14.6946
      vertex 1.74858 -16.6366 -18.5786
      vertex 5.16932 -15.9095 -18.5786
    endloop
  endfacet
  facet normal 0.404532 -0.908595 -0.103962
    outer loop
      vertex 12.2268 -21.4749 -2.5989
      vertex 12.2268 -21.1775 -5.19779
      vertex 12.5 -21.6506 0
    endloop
  endfacet
  facet normal 0.404532 -0.908595 -0.103962
    outer loop
      vertex 11.4193 -21.55 -5.08421
      vertex 12.2268 -21.1775 -5.19779
      vertex 12.2268 -21.4749 -2.5989
    endloop
  endfacet
  facet normal 0.404532 -0.908595 -0.103962
    outer loop
      vertex 12.2268 -21.1775 -5.19779
      vertex 11.4193 -21.55 -5.08421
      vertex 11.3537 -21.5662 -5.19779
    endloop
  endfacet
  facet normal 0.510867 -0.294949 0.807478
    outer loop
      vertex 15.282 -6.804 18.5786
      vertex 11.4193 -5.08421 21.6506
      vertex 13.5335 -9.83263 18.5786
    endloop
  endfacet
  facet normal 0.561027 -0.182289 0.807478
    outer loop
      vertex 16.3627 -3.478 18.5786
      vertex 11.4193 -5.08421 21.6506
      vertex 15.282 -6.804 18.5786
    endloop
  endfacet
  facet normal 0.303006 -0.680563 0.667099
    outer loop
      vertex 8.36413 -14.4871 18.5786
      vertex 5.16932 -15.9095 18.5786
      vertex 6.25 -19.2355 14.6946
    endloop
  endfacet
  facet normal 0.154888 -0.72869 0.667099
    outer loop
      vertex 5.16932 -15.9095 18.5786
      vertex 2.11413 -20.1146 14.6946
      vertex 6.25 -19.2355 14.6946
    endloop
  endfacet
  facet normal 0.239933 -0.538899 0.807478
    outer loop
      vertex 6.25 -10.8253 21.6506
      vertex 3.86271 -11.8882 21.6506
      vertex 5.16932 -15.9095 18.5786
    endloop
  endfacet
  facet normal -0.352726 -0.792236 -0.497942
    outer loop
      vertex -6.25 -19.2355 -14.6946
      vertex -7.15415 -21.6778 -10.1684
      vertex -11.4193 -19.7788 -10.1684
    endloop
  endfacet
  facet normal -0.352724 -0.792237 -0.497942
    outer loop
      vertex -7.15415 -21.6778 -10.1684
      vertex -6.25 -19.2355 -14.6946
      vertex -7.04314 -21.6765 -10.2491
    endloop
  endfacet
  facet normal -0.303006 -0.680563 -0.667099
    outer loop
      vertex -6.25 -19.2355 -14.6946
      vertex -10.1127 -17.5157 -14.6946
      vertex -8.36413 -14.4871 -18.5786
    endloop
  endfacet
  facet normal 0 -0.867211 -0.497942
    outer loop
      vertex -1.30661 -21.4141 -12.4315
      vertex -2.11413 -20.1146 -14.6946
      vertex 1.30661 -21.4141 -12.4315
    endloop
  endfacet
  facet normal -5.36664e-07 -0.867211 -0.497941
    outer loop
      vertex -2.11413 -20.1146 -14.6946
      vertex -1.30661 -21.4141 -12.4315
      vertex -2.26298 -21.5308 -12.2282
    endloop
  endfacet
  facet normal -0 -0.867211 -0.497942
    outer loop
      vertex 2.11413 -20.1146 -14.6946
      vertex 1.30661 -21.4141 -12.4315
      vertex -2.11413 -20.1146 -14.6946
    endloop
  endfacet
  facet normal 5.36664e-07 -0.867211 -0.497941
    outer loop
      vertex 1.30661 -21.4141 -12.4315
      vertex 2.11413 -20.1146 -14.6946
      vertex 2.26298 -21.5308 -12.2282
    endloop
  endfacet
  facet normal 0.509734 -0.701588 -0.497942
    outer loop
      vertex 11.4193 -19.7788 -10.1684
      vertex 10.1127 -17.5157 -14.6946
      vertex 13.5335 -15.0304 -14.6946
    endloop
  endfacet
  facet normal 0.509734 -0.701588 -0.497942
    outer loop
      vertex 15.282 -16.9724 -10.1684
      vertex 11.4193 -19.7788 -10.1684
      vertex 13.5335 -15.0304 -14.6946
    endloop
  endfacet
  facet normal -0.387032 0.869287 0.307485
    outer loop
      vertex -10.1127 21.3585 7.34731
      vertex -11.4193 19.7788 10.1684
      vertex -9.63732 21.3834 7.8753
    endloop
  endfacet
  facet normal -0.387031 0.869287 0.307484
    outer loop
      vertex -12.2268 21.1775 5.19779
      vertex -10.1127 21.3585 7.34731
      vertex -11.3537 21.5662 5.19779
    endloop
  endfacet
  facet normal -0.387032 0.869287 0.307485
    outer loop
      vertex -10.1127 21.3585 7.34731
      vertex -12.2268 21.1775 5.19779
      vertex -11.4193 19.7788 10.1684
    endloop
  endfacet
  facet normal -0.404532 0.908595 0.103962
    outer loop
      vertex -11.4193 21.55 5.08421
      vertex -12.2268 21.1775 5.19779
      vertex -11.3537 21.5662 5.19779
    endloop
  endfacet
  facet normal -0.404532 0.908595 0.103962
    outer loop
      vertex -12.2268 21.4749 2.5989
      vertex -12.2268 21.1775 5.19779
      vertex -11.4193 21.55 5.08421
    endloop
  endfacet
  facet normal -0.404534 0.908595 0.103962
    outer loop
      vertex -12.2268 21.1775 5.19779
      vertex -12.2268 21.4749 2.5989
      vertex -12.5 21.6506 1.81471e-05
    endloop
  endfacet
  facet normal 0.180303 -0.84826 -0.497942
    outer loop
      vertex 6.25 -19.2355 -14.6946
      vertex 6.68623 -21.6002 -10.5084
      vertex 2.11413 -20.1146 -14.6946
    endloop
  endfacet
  facet normal 0.180301 -0.84826 -0.497942
    outer loop
      vertex 6.68623 -21.6002 -10.5084
      vertex 6.25 -19.2355 -14.6946
      vertex 7.04314 -21.6765 -10.2491
    endloop
  endfacet
  facet normal 0.154888 -0.72869 -0.667099
    outer loop
      vertex 6.25 -19.2355 -14.6946
      vertex 2.11413 -20.1146 -14.6946
      vertex 5.16932 -15.9095 -18.5786
    endloop
  endfacet
  facet normal -0.352726 -0.792236 -0.497942
    outer loop
      vertex -6.25 -19.2355 -14.6946
      vertex -11.4193 -19.7788 -10.1684
      vertex -10.1127 -17.5157 -14.6946
    endloop
  endfacet
  facet normal -0.509734 -0.701588 -0.497942
    outer loop
      vertex -11.4193 -19.7788 -10.1684
      vertex -13.5335 -15.0304 -14.6946
      vertex -10.1127 -17.5157 -14.6946
    endloop
  endfacet
  facet normal -0.122647 -0.577007 -0.807478
    outer loop
      vertex -1.30661 -12.4315 -21.6506
      vertex -5.16932 -15.9095 -18.5786
      vertex -3.86271 -11.8882 -21.6506
    endloop
  endfacet
  facet normal -0.866025 -0.5 0
    outer loop
      vertex 11.4193 5.08421 11.4193
      vertex 10.1127 7.34731 21.6506
      vertex 10.1127 7.34731 10.1127
    endloop
  endfacet
  facet normal -0.866025 -0.5 0
    outer loop
      vertex 10.1127 7.34731 21.6506
      vertex 11.4193 5.08421 11.4193
      vertex 11.4193 5.08421 21.6506
    endloop
  endfacet
  facet normal -0.866025 -0.5 -0
    outer loop
      vertex 11.4193 5.08421 -11.4193
      vertex 10.1127 7.34731 -21.6506
      vertex 11.4193 5.08421 -21.6506
    endloop
  endfacet
  facet normal -0.866025 -0.5 0
    outer loop
      vertex 10.1127 7.34731 -21.6506
      vertex 11.4193 5.08421 -11.4193
      vertex 10.1127 7.34731 -10.1127
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex -1.30661 12.4315 12.4315
      vertex 1.30661 12.4315 21.6506
      vertex -1.30661 12.4315 21.6506
    endloop
  endfacet
  facet normal 0 -1 -0
    outer loop
      vertex 1.30661 12.4315 21.6506
      vertex -1.30661 12.4315 12.4315
      vertex 1.30661 12.4315 12.4315
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex -1.30661 12.4315 -21.6506
      vertex 1.30661 12.4315 -12.4315
      vertex -1.30661 12.4315 -12.4315
    endloop
  endfacet
  facet normal 0 -1 -0
    outer loop
      vertex 1.30661 12.4315 -12.4315
      vertex -1.30661 12.4315 -21.6506
      vertex 1.30661 12.4315 -21.6506
    endloop
  endfacet
  facet normal -0.743145 -0.669131 -7.26004e-07
    outer loop
      vertex 10.1127 7.34731 10.1127
      vertex 8.36413 9.28931 9.28931
      vertex 8.80248 8.80248 8.80248
    endloop
  endfacet
  facet normal -0.743145 -0.669131 0
    outer loop
      vertex 8.36413 9.28931 9.28931
      vertex 10.1127 7.34731 10.1127
      vertex 8.36413 9.28931 21.6506
    endloop
  endfacet
  facet normal -0.743145 -0.669131 0
    outer loop
      vertex 8.36413 9.28931 21.6506
      vertex 10.1127 7.34731 10.1127
      vertex 10.1127 7.34731 21.6506
    endloop
  endfacet
  facet normal -0.743145 -0.669131 0
    outer loop
      vertex 10.1127 7.34731 -10.1127
      vertex 8.36413 9.28931 -9.28931
      vertex 8.36413 9.28931 -21.6506
    endloop
  endfacet
  facet normal -0.743145 -0.669131 -0
    outer loop
      vertex 10.1127 7.34731 -10.1127
      vertex 8.36413 9.28931 -21.6506
      vertex 10.1127 7.34731 -21.6506
    endloop
  endfacet
  facet normal -0.743145 -0.669131 7.26004e-07
    outer loop
      vertex 8.36413 9.28931 -9.28931
      vertex 10.1127 7.34731 -10.1127
      vertex 8.80248 8.80248 -8.80248
    endloop
  endfacet
  facet normal -0.207912 -0.978148 0
    outer loop
      vertex 3.86271 11.8882 -21.6506
      vertex 1.30661 12.4315 -12.4315
      vertex 1.30661 12.4315 -21.6506
    endloop
  endfacet
  facet normal -0.207912 -0.978148 0
    outer loop
      vertex 1.30661 12.4315 -12.4315
      vertex 3.86271 11.8882 -21.6506
      vertex 3.86271 11.8882 -11.8882
    endloop
  endfacet
  facet normal -0.207912 -0.978148 0
    outer loop
      vertex 1.30661 12.4315 12.4315
      vertex 3.86271 11.8882 21.6506
      vertex 1.30661 12.4315 21.6506
    endloop
  endfacet
  facet normal -0.207912 -0.978148 -0
    outer loop
      vertex 3.86271 11.8882 21.6506
      vertex 1.30661 12.4315 12.4315
      vertex 3.86271 11.8882 11.8882
    endloop
  endfacet
  facet normal 0.406736 -0.913546 0
    outer loop
      vertex -6.25 10.8253 21.6506
      vertex -3.86271 11.8882 11.8882
      vertex -3.86271 11.8882 21.6506
    endloop
  endfacet
  facet normal 0.406736 -0.913546 0
    outer loop
      vertex -3.86271 11.8882 11.8882
      vertex -6.25 10.8253 21.6506
      vertex -6.25 10.8253 10.8253
    endloop
  endfacet
  facet normal 0.406736 -0.913546 0
    outer loop
      vertex -6.25 10.8253 -21.6506
      vertex -3.86271 11.8882 -11.8882
      vertex -6.25 10.8253 -10.8253
    endloop
  endfacet
  facet normal 0.406736 -0.913546 0
    outer loop
      vertex -3.86271 11.8882 -11.8882
      vertex -6.25 10.8253 -21.6506
      vertex -3.86271 11.8882 -21.6506
    endloop
  endfacet
  facet normal 0.866025 -0.5 0
    outer loop
      vertex -11.4193 5.08421 -11.4193
      vertex -10.1127 7.34731 -21.6506
      vertex -10.1127 7.34731 -10.1127
    endloop
  endfacet
  facet normal 0.866025 -0.5 0
    outer loop
      vertex -10.1127 7.34731 -21.6506
      vertex -11.4193 5.08421 -11.4193
      vertex -11.4193 5.08421 -21.6506
    endloop
  endfacet
  facet normal 0.866025 -0.5 0
    outer loop
      vertex -11.4193 5.08421 11.4193
      vertex -10.1127 7.34731 21.6506
      vertex -11.4193 5.08421 21.6506
    endloop
  endfacet
  facet normal 0.866025 -0.5 0
    outer loop
      vertex -10.1127 7.34731 21.6506
      vertex -11.4193 5.08421 11.4193
      vertex -10.1127 7.34731 10.1127
    endloop
  endfacet
  facet normal -0.994522 0.104528 0
    outer loop
      vertex 12.4177 -0.783091 12.4177
      vertex 12.5 0 21.6506
      vertex 12.5 0 12.5
    endloop
  endfacet
  facet normal -0.994522 0.104529 0
    outer loop
      vertex 12.2268 -2.5989 21.6506
      vertex 12.4177 -0.783091 12.4177
      vertex 12.2268 -2.5989 12.2268
    endloop
  endfacet
  facet normal -0.994522 0.104529 -3.9711e-08
    outer loop
      vertex 12.4177 -0.783091 12.4177
      vertex 12.2268 -2.5989 21.6506
      vertex 12.5 0 21.6506
    endloop
  endfacet
  facet normal -0.994522 0.104529 0
    outer loop
      vertex 12.2268 -2.5989 -21.6506
      vertex 12.5 0 -12.5
      vertex 12.5 0 -21.6506
    endloop
  endfacet
  facet normal -0.994522 0.104528 4.94066e-08
    outer loop
      vertex 12.5 0 -12.5
      vertex 12.2268 -2.5989 -21.6506
      vertex 12.3096 -1.81131 -12.3096
    endloop
  endfacet
  facet normal -0.994522 0.104529 0
    outer loop
      vertex 12.3096 -1.81131 -12.3096
      vertex 12.2268 -2.5989 -21.6506
      vertex 12.2268 -2.5989 -12.2268
    endloop
  endfacet
  facet normal -0.207912 0.978148 0
    outer loop
      vertex 3.86271 -11.8882 21.6506
      vertex 1.30661 -12.4315 12.4315
      vertex 1.30661 -12.4315 21.6506
    endloop
  endfacet
  facet normal -0.207912 0.978148 0
    outer loop
      vertex 1.30661 -12.4315 12.4315
      vertex 3.86271 -11.8882 21.6506
      vertex 3.86271 -11.8882 11.8882
    endloop
  endfacet
  facet normal -0.207912 0.978148 0
    outer loop
      vertex 3.86271 -11.8882 -21.6506
      vertex 1.30661 -12.4315 -12.4315
      vertex 3.86271 -11.8882 -11.8882
    endloop
  endfacet
  facet normal -0.207912 0.978148 0
    outer loop
      vertex 1.30661 -12.4315 -12.4315
      vertex 3.86271 -11.8882 -21.6506
      vertex 1.30661 -12.4315 -21.6506
    endloop
  endfacet
  facet normal -0.587786 -0.809017 0
    outer loop
      vertex 6.25 10.8253 10.8253
      vertex 8.36413 9.28931 21.6506
      vertex 6.25 10.8253 21.6506
    endloop
  endfacet
  facet normal -0.587786 -0.809017 -0
    outer loop
      vertex 8.36413 9.28931 21.6506
      vertex 6.25 10.8253 10.8253
      vertex 8.36413 9.28931 9.28931
    endloop
  endfacet
  facet normal -0.587786 -0.809017 0
    outer loop
      vertex 8.36413 9.28931 -21.6506
      vertex 6.25 10.8253 -10.8253
      vertex 6.25 10.8253 -21.6506
    endloop
  endfacet
  facet normal -0.587786 -0.809017 0
    outer loop
      vertex 6.25 10.8253 -10.8253
      vertex 8.36413 9.28931 -21.6506
      vertex 8.36413 9.28931 -9.28931
    endloop
  endfacet
  facet normal -0.406736 -0.913546 0
    outer loop
      vertex 3.86271 11.8882 11.8882
      vertex 6.25 10.8253 21.6506
      vertex 3.86271 11.8882 21.6506
    endloop
  endfacet
  facet normal -0.406736 -0.913546 -0
    outer loop
      vertex 6.25 10.8253 21.6506
      vertex 3.86271 11.8882 11.8882
      vertex 6.25 10.8253 10.8253
    endloop
  endfacet
  facet normal -0.406736 -0.913546 0
    outer loop
      vertex 6.25 10.8253 -21.6506
      vertex 3.86271 11.8882 -11.8882
      vertex 3.86271 11.8882 -21.6506
    endloop
  endfacet
  facet normal -0.406736 -0.913546 0
    outer loop
      vertex 3.86271 11.8882 -11.8882
      vertex 6.25 10.8253 -21.6506
      vertex 6.25 10.8253 -10.8253
    endloop
  endfacet
  facet normal 0.994522 -0.104529 0
    outer loop
      vertex -12.4177 0.783091 -12.4177
      vertex -12.2268 2.5989 -21.6506
      vertex -12.2268 2.5989 -12.2268
    endloop
  endfacet
  facet normal 0.994522 -0.104528 0
    outer loop
      vertex -12.5 0 -21.6506
      vertex -12.4177 0.783091 -12.4177
      vertex -12.5 0 -12.5
    endloop
  endfacet
  facet normal 0.994522 -0.104529 3.9711e-08
    outer loop
      vertex -12.4177 0.783091 -12.4177
      vertex -12.5 0 -21.6506
      vertex -12.2268 2.5989 -21.6506
    endloop
  endfacet
  facet normal 0.994522 -0.104529 0
    outer loop
      vertex -12.2268 2.5989 21.6506
      vertex -12.3096 1.81131 12.3096
      vertex -12.2268 2.5989 12.2268
    endloop
  endfacet
  facet normal 0.994522 -0.104529 0
    outer loop
      vertex -12.5 0 12.5
      vertex -12.2268 2.5989 21.6506
      vertex -12.5 0 21.6506
    endloop
  endfacet
  facet normal 0.994522 -0.104528 -4.94066e-08
    outer loop
      vertex -12.2268 2.5989 21.6506
      vertex -12.5 0 12.5
      vertex -12.3096 1.81131 12.3096
    endloop
  endfacet
  facet normal 0.587785 -0.809017 0
    outer loop
      vertex -8.36413 9.28931 21.6506
      vertex -6.25 10.8253 10.8253
      vertex -6.25 10.8253 21.6506
    endloop
  endfacet
  facet normal 0.587785 -0.809017 0
    outer loop
      vertex -6.25 10.8253 10.8253
      vertex -8.36413 9.28931 21.6506
      vertex -8.36413 9.28931 9.28931
    endloop
  endfacet
  facet normal 0.587785 -0.809017 0
    outer loop
      vertex -8.36413 9.28931 -21.6506
      vertex -6.25 10.8253 -10.8253
      vertex -8.36413 9.28931 -9.28931
    endloop
  endfacet
  facet normal 0.587785 -0.809017 -1.94898e-08
    outer loop
      vertex -6.25 10.8253 -21.6506
      vertex -6.25 10.8253 -10.8253
      vertex -8.36413 9.28931 -21.6506
    endloop
  endfacet
  facet normal 0.207912 -0.978148 0
    outer loop
      vertex -3.86271 11.8882 21.6506
      vertex -1.30661 12.4315 12.4315
      vertex -1.30661 12.4315 21.6506
    endloop
  endfacet
  facet normal 0.207912 -0.978148 0
    outer loop
      vertex -1.30661 12.4315 12.4315
      vertex -3.86271 11.8882 21.6506
      vertex -3.86271 11.8882 11.8882
    endloop
  endfacet
  facet normal 0.207912 -0.978148 0
    outer loop
      vertex -3.86271 11.8882 -21.6506
      vertex -1.30661 12.4315 -12.4315
      vertex -3.86271 11.8882 -11.8882
    endloop
  endfacet
  facet normal 0.207912 -0.978148 0
    outer loop
      vertex -1.30661 12.4315 -12.4315
      vertex -3.86271 11.8882 -21.6506
      vertex -1.30661 12.4315 -21.6506
    endloop
  endfacet
  facet normal 0.743145 -0.669131 7.26004e-07
    outer loop
      vertex -10.1127 7.34731 -10.1127
      vertex -8.36413 9.28931 -9.28931
      vertex -8.80248 8.80248 -8.80248
    endloop
  endfacet
  facet normal 0.743145 -0.669131 0
    outer loop
      vertex -8.36413 9.28931 -9.28931
      vertex -10.1127 7.34731 -10.1127
      vertex -8.36413 9.28931 -21.6506
    endloop
  endfacet
  facet normal 0.743145 -0.669131 0
    outer loop
      vertex -8.36413 9.28931 -21.6506
      vertex -10.1127 7.34731 -10.1127
      vertex -10.1127 7.34731 -21.6506
    endloop
  endfacet
  facet normal 0.743145 -0.669131 0
    outer loop
      vertex -10.1127 7.34731 10.1127
      vertex -8.36413 9.28931 9.28931
      vertex -8.36413 9.28931 21.6506
    endloop
  endfacet
  facet normal 0.743145 -0.669131 0
    outer loop
      vertex -10.1127 7.34731 10.1127
      vertex -8.36413 9.28931 21.6506
      vertex -10.1127 7.34731 21.6506
    endloop
  endfacet
  facet normal 0.743145 -0.669131 -7.26004e-07
    outer loop
      vertex -8.36413 9.28931 9.28931
      vertex -10.1127 7.34731 10.1127
      vertex -8.80248 8.80248 8.80248
    endloop
  endfacet
  facet normal -0.994522 -0.104529 0
    outer loop
      vertex 12.4177 0.783091 12.4177
      vertex 12.2268 2.5989 21.6506
      vertex 12.2268 2.5989 12.2268
    endloop
  endfacet
  facet normal -0.994522 -0.104528 -0
    outer loop
      vertex 12.5 0 21.6506
      vertex 12.4177 0.783091 12.4177
      vertex 12.5 0 12.5
    endloop
  endfacet
  facet normal -0.994522 -0.104529 -3.9711e-08
    outer loop
      vertex 12.4177 0.783091 12.4177
      vertex 12.5 0 21.6506
      vertex 12.2268 2.5989 21.6506
    endloop
  endfacet
  facet normal -0.994522 -0.104529 0
    outer loop
      vertex 12.2268 2.5989 -21.6506
      vertex 12.3096 1.81131 -12.3096
      vertex 12.2268 2.5989 -12.2268
    endloop
  endfacet
  facet normal -0.994522 -0.104529 -0
    outer loop
      vertex 12.5 0 -12.5
      vertex 12.2268 2.5989 -21.6506
      vertex 12.5 0 -21.6506
    endloop
  endfacet
  facet normal -0.994522 -0.104528 4.94066e-08
    outer loop
      vertex 12.2268 2.5989 -21.6506
      vertex 12.5 0 -12.5
      vertex 12.3096 1.81131 -12.3096
    endloop
  endfacet
  facet normal -0.951057 0.309017 0
    outer loop
      vertex 11.4193 -5.08421 -21.6506
      vertex 12.2268 -2.5989 -12.2268
      vertex 12.2268 -2.5989 -21.6506
    endloop
  endfacet
  facet normal -0.951057 0.309017 0
    outer loop
      vertex 12.2268 -2.5989 -12.2268
      vertex 11.4193 -5.08421 -21.6506
      vertex 11.4193 -5.08421 -11.4193
    endloop
  endfacet
  facet normal -0.951057 0.309017 0
    outer loop
      vertex 11.4193 -5.08421 21.6506
      vertex 12.2268 -2.5989 12.2268
      vertex 11.4193 -5.08421 11.4193
    endloop
  endfacet
  facet normal -0.951057 0.309017 0
    outer loop
      vertex 12.2268 -2.5989 12.2268
      vertex 11.4193 -5.08421 21.6506
      vertex 12.2268 -2.5989 21.6506
    endloop
  endfacet
  facet normal 0.866025 0.5 -0
    outer loop
      vertex -10.1127 -7.34731 -21.6506
      vertex -11.4193 -5.08421 -11.4193
      vertex -10.1127 -7.34731 -10.1127
    endloop
  endfacet
  facet normal 0.866025 0.5 0
    outer loop
      vertex -11.4193 -5.08421 -11.4193
      vertex -10.1127 -7.34731 -21.6506
      vertex -11.4193 -5.08421 -21.6506
    endloop
  endfacet
  facet normal 0.866025 0.5 0
    outer loop
      vertex -10.1127 -7.34731 21.6506
      vertex -11.4193 -5.08421 11.4193
      vertex -11.4193 -5.08421 21.6506
    endloop
  endfacet
  facet normal 0.866025 0.5 0
    outer loop
      vertex -11.4193 -5.08421 11.4193
      vertex -10.1127 -7.34731 21.6506
      vertex -10.1127 -7.34731 10.1127
    endloop
  endfacet
  facet normal -0.587786 0.809017 0
    outer loop
      vertex 8.36413 -9.28931 -21.6506
      vertex 6.25 -10.8253 -10.8253
      vertex 8.36413 -9.28931 -9.28931
    endloop
  endfacet
  facet normal -0.587786 0.809017 0
    outer loop
      vertex 6.25 -10.8253 -10.8253
      vertex 8.36413 -9.28931 -21.6506
      vertex 6.25 -10.8253 -21.6506
    endloop
  endfacet
  facet normal -0.587786 0.809017 0
    outer loop
      vertex 8.36413 -9.28931 21.6506
      vertex 6.25 -10.8253 10.8253
      vertex 6.25 -10.8253 21.6506
    endloop
  endfacet
  facet normal -0.587786 0.809017 0
    outer loop
      vertex 6.25 -10.8253 10.8253
      vertex 8.36413 -9.28931 21.6506
      vertex 8.36413 -9.28931 9.28931
    endloop
  endfacet
  facet normal -0.951057 -0.309017 0
    outer loop
      vertex 12.2268 2.5989 12.2268
      vertex 11.4193 5.08421 21.6506
      vertex 11.4193 5.08421 11.4193
    endloop
  endfacet
  facet normal -0.951057 -0.309017 0
    outer loop
      vertex 11.4193 5.08421 21.6506
      vertex 12.2268 2.5989 12.2268
      vertex 12.2268 2.5989 21.6506
    endloop
  endfacet
  facet normal -0.951057 -0.309017 -0
    outer loop
      vertex 12.2268 2.5989 -12.2268
      vertex 11.4193 5.08421 -21.6506
      vertex 12.2268 2.5989 -21.6506
    endloop
  endfacet
  facet normal -0.951057 -0.309017 0
    outer loop
      vertex 11.4193 5.08421 -21.6506
      vertex 12.2268 2.5989 -12.2268
      vertex 11.4193 5.08421 -11.4193
    endloop
  endfacet
  facet normal 0.951057 -0.309017 0
    outer loop
      vertex -12.2268 2.5989 -12.2268
      vertex -11.4193 5.08421 -21.6506
      vertex -11.4193 5.08421 -11.4193
    endloop
  endfacet
  facet normal 0.951057 -0.309017 0
    outer loop
      vertex -11.4193 5.08421 -21.6506
      vertex -12.2268 2.5989 -12.2268
      vertex -12.2268 2.5989 -21.6506
    endloop
  endfacet
  facet normal 0.951057 -0.309017 0
    outer loop
      vertex -12.2268 2.5989 12.2268
      vertex -11.4193 5.08421 21.6506
      vertex -12.2268 2.5989 21.6506
    endloop
  endfacet
  facet normal 0.951057 -0.309017 0
    outer loop
      vertex -11.4193 5.08421 21.6506
      vertex -12.2268 2.5989 12.2268
      vertex -11.4193 5.08421 11.4193
    endloop
  endfacet
  facet normal -0.866025 0.5 0
    outer loop
      vertex 10.1127 -7.34731 -21.6506
      vertex 11.4193 -5.08421 -11.4193
      vertex 11.4193 -5.08421 -21.6506
    endloop
  endfacet
  facet normal -0.866025 0.5 0
    outer loop
      vertex 11.4193 -5.08421 -11.4193
      vertex 10.1127 -7.34731 -21.6506
      vertex 10.1127 -7.34731 -10.1127
    endloop
  endfacet
  facet normal -0.866025 0.5 0
    outer loop
      vertex 10.1127 -7.34731 21.6506
      vertex 11.4193 -5.08421 11.4193
      vertex 10.1127 -7.34731 10.1127
    endloop
  endfacet
  facet normal -0.866025 0.5 0
    outer loop
      vertex 11.4193 -5.08421 11.4193
      vertex 10.1127 -7.34731 21.6506
      vertex 11.4193 -5.08421 21.6506
    endloop
  endfacet
  facet normal 0 1 -0
    outer loop
      vertex 1.30661 -12.4315 12.4315
      vertex -1.30661 -12.4315 21.6506
      vertex 1.30661 -12.4315 21.6506
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex -1.30661 -12.4315 21.6506
      vertex 1.30661 -12.4315 12.4315
      vertex -1.30661 -12.4315 12.4315
    endloop
  endfacet
  facet normal 0 1 -0
    outer loop
      vertex 1.30661 -12.4315 -21.6506
      vertex -1.30661 -12.4315 -12.4315
      vertex 1.30661 -12.4315 -12.4315
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex -1.30661 -12.4315 -12.4315
      vertex 1.30661 -12.4315 -21.6506
      vertex -1.30661 -12.4315 -21.6506
    endloop
  endfacet
  facet normal 0.994522 0.104528 0
    outer loop
      vertex -12.4177 -0.783091 -12.4177
      vertex -12.5 0 -21.6506
      vertex -12.5 0 -12.5
    endloop
  endfacet
  facet normal 0.994522 0.104529 -0
    outer loop
      vertex -12.2268 -2.5989 -21.6506
      vertex -12.4177 -0.783091 -12.4177
      vertex -12.2268 -2.5989 -12.2268
    endloop
  endfacet
  facet normal 0.994522 0.104529 3.9711e-08
    outer loop
      vertex -12.4177 -0.783091 -12.4177
      vertex -12.2268 -2.5989 -21.6506
      vertex -12.5 0 -21.6506
    endloop
  endfacet
  facet normal 0.994522 0.104529 0
    outer loop
      vertex -12.2268 -2.5989 21.6506
      vertex -12.5 0 12.5
      vertex -12.5 0 21.6506
    endloop
  endfacet
  facet normal 0.994522 0.104528 -4.94066e-08
    outer loop
      vertex -12.5 0 12.5
      vertex -12.2268 -2.5989 21.6506
      vertex -12.3096 -1.81131 12.3096
    endloop
  endfacet
  facet normal 0.994522 0.104529 0
    outer loop
      vertex -12.3096 -1.81131 12.3096
      vertex -12.2268 -2.5989 21.6506
      vertex -12.2268 -2.5989 12.2268
    endloop
  endfacet
  facet normal -0.406736 0.913546 0
    outer loop
      vertex 6.25 -10.8253 21.6506
      vertex 3.86271 -11.8882 11.8882
      vertex 3.86271 -11.8882 21.6506
    endloop
  endfacet
  facet normal -0.406736 0.913546 0
    outer loop
      vertex 3.86271 -11.8882 11.8882
      vertex 6.25 -10.8253 21.6506
      vertex 6.25 -10.8253 10.8253
    endloop
  endfacet
  facet normal -0.406736 0.913546 0
    outer loop
      vertex 6.25 -10.8253 -21.6506
      vertex 3.86271 -11.8882 -11.8882
      vertex 6.25 -10.8253 -10.8253
    endloop
  endfacet
  facet normal -0.406736 0.913546 0
    outer loop
      vertex 3.86271 -11.8882 -11.8882
      vertex 6.25 -10.8253 -21.6506
      vertex 3.86271 -11.8882 -21.6506
    endloop
  endfacet
  facet normal 0.951057 0.309017 -0
    outer loop
      vertex -11.4193 -5.08421 -21.6506
      vertex -12.2268 -2.5989 -12.2268
      vertex -11.4193 -5.08421 -11.4193
    endloop
  endfacet
  facet normal 0.951057 0.309017 0
    outer loop
      vertex -12.2268 -2.5989 -12.2268
      vertex -11.4193 -5.08421 -21.6506
      vertex -12.2268 -2.5989 -21.6506
    endloop
  endfacet
  facet normal 0.951057 0.309017 0
    outer loop
      vertex -11.4193 -5.08421 21.6506
      vertex -12.2268 -2.5989 12.2268
      vertex -12.2268 -2.5989 21.6506
    endloop
  endfacet
  facet normal 0.951057 0.309017 0
    outer loop
      vertex -12.2268 -2.5989 12.2268
      vertex -11.4193 -5.08421 21.6506
      vertex -11.4193 -5.08421 11.4193
    endloop
  endfacet
  facet normal 0.743145 0.669131 0
    outer loop
      vertex -8.36413 -9.28931 21.6506
      vertex -10.1127 -7.34731 10.1127
      vertex -10.1127 -7.34731 21.6506
    endloop
  endfacet
  facet normal 0.743145 0.669131 0
    outer loop
      vertex -10.1127 -7.34731 10.1127
      vertex -8.36413 -9.28931 21.6506
      vertex -8.36413 -9.28931 9.28931
    endloop
  endfacet
  facet normal 0.743145 0.669131 -7.26004e-07
    outer loop
      vertex -10.1127 -7.34731 10.1127
      vertex -8.36413 -9.28931 9.28931
      vertex -8.80248 -8.80248 8.80248
    endloop
  endfacet
  facet normal 0.743145 0.669131 7.26004e-07
    outer loop
      vertex -8.36413 -9.28931 -9.28931
      vertex -10.1127 -7.34731 -10.1127
      vertex -8.80248 -8.80248 -8.80248
    endloop
  endfacet
  facet normal 0.743145 0.669131 -0
    outer loop
      vertex -8.36413 -9.28931 -21.6506
      vertex -10.1127 -7.34731 -10.1127
      vertex -8.36413 -9.28931 -9.28931
    endloop
  endfacet
  facet normal 0.743145 0.669131 0
    outer loop
      vertex -10.1127 -7.34731 -10.1127
      vertex -8.36413 -9.28931 -21.6506
      vertex -10.1127 -7.34731 -21.6506
    endloop
  endfacet
  facet normal -0.743145 0.669131 0
    outer loop
      vertex 8.36413 -9.28931 -21.6506
      vertex 10.1127 -7.34731 -10.1127
      vertex 10.1127 -7.34731 -21.6506
    endloop
  endfacet
  facet normal -0.743145 0.669131 0
    outer loop
      vertex 10.1127 -7.34731 -10.1127
      vertex 8.36413 -9.28931 -21.6506
      vertex 8.36413 -9.28931 -9.28931
    endloop
  endfacet
  facet normal -0.743145 0.669131 7.26004e-07
    outer loop
      vertex 10.1127 -7.34731 -10.1127
      vertex 8.36413 -9.28931 -9.28931
      vertex 8.80248 -8.80248 -8.80248
    endloop
  endfacet
  facet normal -0.743145 0.669131 -7.26004e-07
    outer loop
      vertex 8.36413 -9.28931 9.28931
      vertex 10.1127 -7.34731 10.1127
      vertex 8.80248 -8.80248 8.80248
    endloop
  endfacet
  facet normal -0.743145 0.669131 0
    outer loop
      vertex 8.36413 -9.28931 21.6506
      vertex 10.1127 -7.34731 10.1127
      vertex 8.36413 -9.28931 9.28931
    endloop
  endfacet
  facet normal -0.743145 0.669131 0
    outer loop
      vertex 10.1127 -7.34731 10.1127
      vertex 8.36413 -9.28931 21.6506
      vertex 10.1127 -7.34731 21.6506
    endloop
  endfacet
  facet normal 0.406736 0.913546 -0
    outer loop
      vertex -3.86271 -11.8882 11.8882
      vertex -6.25 -10.8253 21.6506
      vertex -3.86271 -11.8882 21.6506
    endloop
  endfacet
  facet normal 0.406736 0.913546 3.58321e-08
    outer loop
      vertex -6.25 -10.8253 10.8253
      vertex -6.25 -10.8253 21.6506
      vertex -3.86271 -11.8882 11.8882
    endloop
  endfacet
  facet normal 0.406736 0.913546 0
    outer loop
      vertex -6.25 -10.8253 -21.6506
      vertex -3.86271 -11.8882 -11.8882
      vertex -3.86271 -11.8882 -21.6506
    endloop
  endfacet
  facet normal 0.406736 0.913546 0
    outer loop
      vertex -3.86271 -11.8882 -11.8882
      vertex -6.25 -10.8253 -21.6506
      vertex -6.25 -10.8253 -10.8253
    endloop
  endfacet
  facet normal 0.587786 0.809017 -0
    outer loop
      vertex -6.25 -10.8253 10.8253
      vertex -8.36413 -9.28931 21.6506
      vertex -6.25 -10.8253 21.6506
    endloop
  endfacet
  facet normal 0.587786 0.809017 0
    outer loop
      vertex -8.36413 -9.28931 21.6506
      vertex -6.25 -10.8253 10.8253
      vertex -8.36413 -9.28931 9.28931
    endloop
  endfacet
  facet normal 0.587786 0.809017 0
    outer loop
      vertex -8.36413 -9.28931 -21.6506
      vertex -6.25 -10.8253 -10.8253
      vertex -6.25 -10.8253 -21.6506
    endloop
  endfacet
  facet normal 0.587786 0.809017 0
    outer loop
      vertex -6.25 -10.8253 -10.8253
      vertex -8.36413 -9.28931 -21.6506
      vertex -8.36413 -9.28931 -9.28931
    endloop
  endfacet
  facet normal 0.207912 0.978148 -0
    outer loop
      vertex -1.30661 -12.4315 12.4315
      vertex -3.86271 -11.8882 21.6506
      vertex -1.30661 -12.4315 21.6506
    endloop
  endfacet
  facet normal 0.207912 0.978148 0
    outer loop
      vertex -3.86271 -11.8882 21.6506
      vertex -1.30661 -12.4315 12.4315
      vertex -3.86271 -11.8882 11.8882
    endloop
  endfacet
  facet normal 0.207912 0.978148 0
    outer loop
      vertex -3.86271 -11.8882 -21.6506
      vertex -1.30661 -12.4315 -12.4315
      vertex -1.30661 -12.4315 -21.6506
    endloop
  endfacet
  facet normal 0.207912 0.978148 0
    outer loop
      vertex -1.30661 -12.4315 -12.4315
      vertex -3.86271 -11.8882 -21.6506
      vertex -3.86271 -11.8882 -11.8882
    endloop
  endfacet
  facet normal -0.994522 0 -0.104529
    outer loop
      vertex 12.5 -21.6506 0
      vertex 12.3627 -12.4315 1.30661
      vertex 12.5 -12.4315 0
    endloop
  endfacet
  facet normal -0.994522 -1.99482e-08 -0.104529
    outer loop
      vertex 12.3627 -12.4315 1.30661
      vertex 12.5 -21.6506 0
      vertex 12.2268 -21.4749 2.5989
    endloop
  endfacet
  facet normal -0.994522 -0 -0.104528
    outer loop
      vertex 12.2268 -12.1568 2.5989
      vertex 12.3627 -12.4315 1.30661
      vertex 12.2268 -21.4749 2.5989
    endloop
  endfacet
  facet normal -0.994522 0 -0.104529
    outer loop
      vertex 12.3627 12.4315 1.30661
      vertex 12.5 21.6506 0
      vertex 12.5 12.4315 0
    endloop
  endfacet
  facet normal -0.994522 1.99482e-08 -0.104529
    outer loop
      vertex 12.5 21.6506 0
      vertex 12.3627 12.4315 1.30661
      vertex 12.2268 21.4749 2.5989
    endloop
  endfacet
  facet normal -0.994522 -0 -0.104528
    outer loop
      vertex 12.2268 21.4749 2.5989
      vertex 12.3627 12.4315 1.30661
      vertex 12.2268 12.1568 2.5989
    endloop
  endfacet
  facet normal -0.951057 0 -0.309017
    outer loop
      vertex 12.2268 12.1568 2.5989
      vertex 11.4193 21.55 5.08421
      vertex 12.2268 21.4749 2.5989
    endloop
  endfacet
  facet normal -0.951056 3.27946e-08 -0.309017
    outer loop
      vertex 11.8162 11.8882 3.86271
      vertex 11.4193 21.55 5.08421
      vertex 12.2268 12.1568 2.5989
    endloop
  endfacet
  facet normal -0.951057 -0 -0.309017
    outer loop
      vertex 11.4193 21.55 5.08421
      vertex 11.8162 11.8882 3.86271
      vertex 11.4193 11.3444 5.08421
    endloop
  endfacet
  facet normal -0.951057 0 -0.309017
    outer loop
      vertex 11.4193 -21.55 5.08421
      vertex 12.2268 -12.1568 2.5989
      vertex 12.2268 -21.4749 2.5989
    endloop
  endfacet
  facet normal -0.951056 -3.27946e-08 -0.309017
    outer loop
      vertex 12.2268 -12.1568 2.5989
      vertex 11.4193 -21.55 5.08421
      vertex 11.8162 -11.8882 3.86271
    endloop
  endfacet
  facet normal -0.951057 0 -0.309017
    outer loop
      vertex 11.8162 -11.8882 3.86271
      vertex 11.4193 -21.55 5.08421
      vertex 11.4193 -11.3444 5.08421
    endloop
  endfacet
  facet normal 0.994522 0 -0.104528
    outer loop
      vertex -12.2268 -21.4749 2.5989
      vertex -12.3627 -12.4315 1.30661
      vertex -12.2268 -12.1568 2.5989
    endloop
  endfacet
  facet normal 0.994522 -1.99482e-08 -0.104529
    outer loop
      vertex -12.5 -21.6506 0
      vertex -12.3627 -12.4315 1.30661
      vertex -12.2268 -21.4749 2.5989
    endloop
  endfacet
  facet normal 0.994522 0 -0.104529
    outer loop
      vertex -12.3627 -12.4315 1.30661
      vertex -12.5 -21.6506 0
      vertex -12.5 -12.4315 0
    endloop
  endfacet
  facet normal 0.994522 0 -0.104528
    outer loop
      vertex -12.2268 21.4749 2.5989
      vertex -12.2268 12.1568 2.5989
      vertex -12.3627 12.4315 1.30661
    endloop
  endfacet
  facet normal 0.707107 0.707107 0
    outer loop
      vertex -12.5 21.6506 1.81471e-05
      vertex -12.5 21.6506 0
      vertex -12.5 21.6506 1.12429e-06
    endloop
  endfacet
  facet normal 0.994522 1.99478e-08 -0.104529
    outer loop
      vertex -12.3627 12.4315 1.30661
      vertex -12.5 21.6506 1.81471e-05
      vertex -12.2268 21.4749 2.5989
    endloop
  endfacet
  facet normal 0.994522 -4.41513e-13 -0.104529
    outer loop
      vertex -12.5 12.4315 0
      vertex -12.5 21.6506 1.81471e-05
      vertex -12.3627 12.4315 1.30661
    endloop
  endfacet
  facet normal 0.994522 0 -0.104529
    outer loop
      vertex -12.5 21.6506 1.81471e-05
      vertex -12.5 12.4315 0
      vertex -12.5 21.6506 0
    endloop
  endfacet
  facet normal 0 0 -1
    outer loop
      vertex -1.30661 12.4315 12.4315
      vertex 1.30661 21.4141 12.4315
      vertex 1.30661 12.4315 12.4315
    endloop
  endfacet
  facet normal -0 0 -1
    outer loop
      vertex 1.30661 21.4141 12.4315
      vertex -1.30661 12.4315 12.4315
      vertex -1.30661 21.4141 12.4315
    endloop
  endfacet
  facet normal 0 0 -1
    outer loop
      vertex -1.30661 -21.4141 12.4315
      vertex 1.30661 -12.4315 12.4315
      vertex 1.30661 -21.4141 12.4315
    endloop
  endfacet
  facet normal -0 0 -1
    outer loop
      vertex 1.30661 -12.4315 12.4315
      vertex -1.30661 -21.4141 12.4315
      vertex -1.30661 -12.4315 12.4315
    endloop
  endfacet
  facet normal -0.406736 -0 -0.913546
    outer loop
      vertex 3.86271 -11.8882 11.8882
      vertex 6.25 -21.5069 10.8253
      vertex 3.86271 -21.3904 11.8882
    endloop
  endfacet
  facet normal -0.406736 0 -0.913546
    outer loop
      vertex 6.25 -21.5069 10.8253
      vertex 3.86271 -11.8882 11.8882
      vertex 6.25 -10.8253 10.8253
    endloop
  endfacet
  facet normal -0.406736 0 -0.913546
    outer loop
      vertex 3.86271 11.8882 11.8882
      vertex 6.25 21.5069 10.8253
      vertex 6.25 10.8253 10.8253
    endloop
  endfacet
  facet normal -0.406736 0 -0.913546
    outer loop
      vertex 6.25 21.5069 10.8253
      vertex 3.86271 11.8882 11.8882
      vertex 3.86271 21.3904 11.8882
    endloop
  endfacet
  facet normal -0.587786 0 -0.809017
    outer loop
      vertex 6.25 10.8253 10.8253
      vertex 8.36413 21.4501 9.28931
      vertex 8.36413 9.28931 9.28931
    endloop
  endfacet
  facet normal -0.587786 -0 -0.809017
    outer loop
      vertex 6.25 21.5069 10.8253
      vertex 8.36413 21.4501 9.28931
      vertex 6.25 10.8253 10.8253
    endloop
  endfacet
  facet normal -0.587785 1.07046e-06 -0.809017
    outer loop
      vertex 8.36413 21.4501 9.28931
      vertex 6.68623 21.6002 10.5084
      vertex 7.15415 21.6778 10.1684
    endloop
  endfacet
  facet normal -0.587785 -1.0498e-06 -0.809017
    outer loop
      vertex 7.15415 21.6778 10.1684
      vertex 6.68623 21.6002 10.5084
      vertex 7.04314 21.6765 10.2491
    endloop
  endfacet
  facet normal -0.587786 -2.9741e-07 -0.809017
    outer loop
      vertex 8.36413 21.4501 9.28931
      vertex 6.25 21.5069 10.8253
      vertex 6.68623 21.6002 10.5084
    endloop
  endfacet
  facet normal -0.587785 -1.07046e-06 -0.809017
    outer loop
      vertex 6.68623 -21.6002 10.5084
      vertex 8.36413 -21.4501 9.28931
      vertex 7.15415 -21.6778 10.1684
    endloop
  endfacet
  facet normal -0.587784 1.48796e-05 -0.809018
    outer loop
      vertex 6.68623 -21.6002 10.5084
      vertex 7.15415 -21.6778 10.1684
      vertex 7.08932 -21.6771 10.2155
    endloop
  endfacet
  facet normal -0.587787 -1.37543e-05 -0.809016
    outer loop
      vertex 6.68623 -21.6002 10.5084
      vertex 7.08932 -21.6771 10.2155
      vertex 7.04314 -21.6765 10.2491
    endloop
  endfacet
  facet normal -0.587786 2.9741e-07 -0.809017
    outer loop
      vertex 6.25 -21.5069 10.8253
      vertex 8.36413 -21.4501 9.28931
      vertex 6.68623 -21.6002 10.5084
    endloop
  endfacet
  facet normal -0.587786 0 -0.809017
    outer loop
      vertex 8.36413 -21.4501 9.28931
      vertex 6.25 -21.5069 10.8253
      vertex 6.25 -10.8253 10.8253
    endloop
  endfacet
  facet normal -0.587786 0 -0.809017
    outer loop
      vertex 8.36413 -21.4501 9.28931
      vertex 6.25 -10.8253 10.8253
      vertex 8.36413 -9.28931 9.28931
    endloop
  endfacet
  facet normal -0.866025 -1.25483e-05 -0.500001
    outer loop
      vertex 11.4193 21.55 5.08421
      vertex 10.1127 21.3585 7.34731
      vertex 11.3537 21.5662 5.19779
    endloop
  endfacet
  facet normal -0.866025 0 -0.5
    outer loop
      vertex 11.4193 11.3444 5.08421
      vertex 10.1127 21.3585 7.34731
      vertex 11.4193 21.55 5.08421
    endloop
  endfacet
  facet normal -0.866025 -4.57631e-09 -0.5
    outer loop
      vertex 11.1887 11.1665 5.48372
      vertex 10.1127 21.3585 7.34731
      vertex 11.4193 11.3444 5.08421
    endloop
  endfacet
  facet normal -0.866025 4.64367e-08 -0.5
    outer loop
      vertex 10.7462 10.8253 6.25
      vertex 10.1127 21.3585 7.34731
      vertex 11.1887 11.1665 5.48372
    endloop
  endfacet
  facet normal -0.866026 -3.11324e-08 -0.5
    outer loop
      vertex 10.3378 10.3114 6.95738
      vertex 10.1127 21.3585 7.34731
      vertex 10.7462 10.8253 6.25
    endloop
  endfacet
  facet normal -0.866025 -0 -0.5
    outer loop
      vertex 10.1127 21.3585 7.34731
      vertex 10.3378 10.3114 6.95738
      vertex 10.1127 10.0281 7.34731
    endloop
  endfacet
  facet normal -0.866025 4.57631e-09 -0.5
    outer loop
      vertex 10.1127 -21.3585 7.34731
      vertex 11.1887 -11.1665 5.48372
      vertex 11.4193 -11.3444 5.08421
    endloop
  endfacet
  facet normal -0.866025 0 -0.5
    outer loop
      vertex 10.3378 -10.3114 6.95738
      vertex 10.1127 -21.3585 7.34731
      vertex 10.1127 -10.0281 7.34731
    endloop
  endfacet
  facet normal -0.866026 3.11324e-08 -0.5
    outer loop
      vertex 10.7462 -10.8253 6.25
      vertex 10.1127 -21.3585 7.34731
      vertex 10.3378 -10.3114 6.95738
    endloop
  endfacet
  facet normal -0.866025 -4.64367e-08 -0.5
    outer loop
      vertex 11.1887 -11.1665 5.48372
      vertex 10.1127 -21.3585 7.34731
      vertex 10.7462 -10.8253 6.25
    endloop
  endfacet
  facet normal -0.866025 0 -0.5
    outer loop
      vertex 11.4193 -21.55 5.08421
      vertex 10.1127 -21.3585 7.34731
      vertex 11.4193 -11.3444 5.08421
    endloop
  endfacet
  facet normal -0.866025 1.25483e-05 -0.500001
    outer loop
      vertex 10.1127 -21.3585 7.34731
      vertex 11.4193 -21.55 5.08421
      vertex 11.3537 -21.5662 5.19779
    endloop
  endfacet
  facet normal 0.743145 0 -0.669131
    outer loop
      vertex -9.63732 21.3834 7.8753
      vertex -10.1127 10.0281 7.34731
      vertex -10.1127 21.3585 7.34731
    endloop
  endfacet
  facet normal 0.743145 -6.32585e-09 -0.669131
    outer loop
      vertex -8.36413 21.4501 9.28931
      vertex -10.1127 10.0281 7.34731
      vertex -9.63732 21.3834 7.8753
    endloop
  endfacet
  facet normal 0.743145 0 -0.669131
    outer loop
      vertex -8.36413 9.28931 9.28931
      vertex -10.1127 10.0281 7.34731
      vertex -8.36413 21.4501 9.28931
    endloop
  endfacet
  facet normal 0.743145 5.94373e-07 -0.669131
    outer loop
      vertex -10.1127 10.0281 7.34731
      vertex -8.36413 9.28931 9.28931
      vertex -9.84898 9.81526 7.64022
    endloop
  endfacet
  facet normal 0.743145 2.08128e-07 -0.669131
    outer loop
      vertex -8.36413 9.28931 9.28931
      vertex -9.19717 9.28931 8.36413
      vertex -9.84898 9.81526 7.64022
    endloop
  endfacet
  facet normal 0.743145 -1.13675e-06 -0.669131
    outer loop
      vertex -9.19717 9.28931 8.36413
      vertex -8.36413 9.28931 9.28931
      vertex -8.80248 8.80248 8.80248
    endloop
  endfacet
  facet normal 0.743145 1.13675e-06 -0.669131
    outer loop
      vertex -8.36413 -9.28931 9.28931
      vertex -9.19717 -9.28931 8.36413
      vertex -8.80248 -8.80248 8.80248
    endloop
  endfacet
  facet normal 0.743145 -2.08128e-07 -0.669131
    outer loop
      vertex -8.36413 -9.28931 9.28931
      vertex -9.84898 -9.81526 7.64022
      vertex -9.19717 -9.28931 8.36413
    endloop
  endfacet
  facet normal 0.743145 -5.94373e-07 -0.669131
    outer loop
      vertex -8.36413 -9.28931 9.28931
      vertex -10.1127 -10.0281 7.34731
      vertex -9.84898 -9.81526 7.64022
    endloop
  endfacet
  facet normal 0.743145 0 -0.669131
    outer loop
      vertex -8.36413 -21.4501 9.28931
      vertex -10.1127 -10.0281 7.34731
      vertex -8.36413 -9.28931 9.28931
    endloop
  endfacet
  facet normal 0.743145 0 -0.669131
    outer loop
      vertex -10.1127 -10.0281 7.34731
      vertex -9.63732 -21.3834 7.8753
      vertex -10.1127 -21.3585 7.34731
    endloop
  endfacet
  facet normal 0.743145 6.32585e-09 -0.669131
    outer loop
      vertex -10.1127 -10.0281 7.34731
      vertex -8.36413 -21.4501 9.28931
      vertex -9.63732 -21.3834 7.8753
    endloop
  endfacet
  facet normal 0.406736 0 -0.913546
    outer loop
      vertex -6.25 21.5069 10.8253
      vertex -3.86271 11.8882 11.8882
      vertex -6.25 10.8253 10.8253
    endloop
  endfacet
  facet normal 0.406736 0 -0.913546
    outer loop
      vertex -3.86271 11.8882 11.8882
      vertex -6.25 21.5069 10.8253
      vertex -3.86271 21.3904 11.8882
    endloop
  endfacet
  facet normal 0.406736 0 -0.913546
    outer loop
      vertex -6.25 -21.5069 10.8253
      vertex -3.86271 -11.8882 11.8882
      vertex -3.86271 -21.3904 11.8882
    endloop
  endfacet
  facet normal 0.406736 0 -0.913546
    outer loop
      vertex -3.86271 -11.8882 11.8882
      vertex -6.25 -21.5069 10.8253
      vertex -6.25 -10.8253 10.8253
    endloop
  endfacet
  facet normal -0.866025 -1.25483e-05 0.500001
    outer loop
      vertex 10.1127 21.3585 -7.34731
      vertex 11.4193 21.55 -5.08421
      vertex 11.3537 21.5662 -5.19779
    endloop
  endfacet
  facet normal -0.866025 0 0.5
    outer loop
      vertex 10.1127 21.3585 -7.34731
      vertex 11.4193 11.3444 -5.08421
      vertex 11.4193 21.55 -5.08421
    endloop
  endfacet
  facet normal -0.866025 3.61429e-08 0.5
    outer loop
      vertex 10.1127 21.3585 -7.34731
      vertex 10.7462 10.8253 -6.25
      vertex 11.4193 11.3444 -5.08421
    endloop
  endfacet
  facet normal -0.866025 0 0.5
    outer loop
      vertex 10.7462 10.8253 -6.25
      vertex 10.1127 21.3585 -7.34731
      vertex 10.1127 10.0281 -7.34731
    endloop
  endfacet
  facet normal -0.866025 0 0.5
    outer loop
      vertex 10.1127 -21.3585 -7.34731
      vertex 10.7462 -10.8253 -6.25
      vertex 10.1127 -10.0281 -7.34731
    endloop
  endfacet
  facet normal -0.866025 -3.61429e-08 0.5
    outer loop
      vertex 10.1127 -21.3585 -7.34731
      vertex 11.4193 -11.3444 -5.08421
      vertex 10.7462 -10.8253 -6.25
    endloop
  endfacet
  facet normal -0.866025 0 0.5
    outer loop
      vertex 10.1127 -21.3585 -7.34731
      vertex 11.4193 -21.55 -5.08421
      vertex 11.4193 -11.3444 -5.08421
    endloop
  endfacet
  facet normal -0.866025 1.25483e-05 0.500001
    outer loop
      vertex 11.4193 -21.55 -5.08421
      vertex 10.1127 -21.3585 -7.34731
      vertex 11.3537 -21.5662 -5.19779
    endloop
  endfacet
  facet normal -0.207912 0 -0.978148
    outer loop
      vertex 1.30661 12.4315 12.4315
      vertex 3.86271 21.3904 11.8882
      vertex 3.86271 11.8882 11.8882
    endloop
  endfacet
  facet normal -0.207912 1.75986e-08 -0.978148
    outer loop
      vertex 3.86271 21.3904 11.8882
      vertex 1.30661 12.4315 12.4315
      vertex 2.26298 21.5308 12.2282
    endloop
  endfacet
  facet normal -0.207912 0 -0.978148
    outer loop
      vertex 2.26298 21.5308 12.2282
      vertex 1.30661 12.4315 12.4315
      vertex 1.30661 21.4141 12.4315
    endloop
  endfacet
  facet normal -0.207912 -1.75986e-08 -0.978148
    outer loop
      vertex 1.30661 -12.4315 12.4315
      vertex 3.86271 -21.3904 11.8882
      vertex 2.26298 -21.5308 12.2282
    endloop
  endfacet
  facet normal -0.207912 -0 -0.978148
    outer loop
      vertex 1.30661 -12.4315 12.4315
      vertex 2.26298 -21.5308 12.2282
      vertex 1.30661 -21.4141 12.4315
    endloop
  endfacet
  facet normal -0.207912 0 -0.978148
    outer loop
      vertex 3.86271 -21.3904 11.8882
      vertex 1.30661 -12.4315 12.4315
      vertex 3.86271 -11.8882 11.8882
    endloop
  endfacet
  facet normal 0.866025 -1.25483e-05 -0.500001
    outer loop
      vertex -10.1127 21.3585 7.34731
      vertex -11.4193 21.55 5.08421
      vertex -11.3537 21.5662 5.19779
    endloop
  endfacet
  facet normal 0.866025 0 -0.5
    outer loop
      vertex -10.1127 21.3585 7.34731
      vertex -11.4193 11.3444 5.08421
      vertex -11.4193 21.55 5.08421
    endloop
  endfacet
  facet normal 0.866025 3.61429e-08 -0.5
    outer loop
      vertex -10.1127 21.3585 7.34731
      vertex -10.7462 10.8253 6.25
      vertex -11.4193 11.3444 5.08421
    endloop
  endfacet
  facet normal 0.866025 0 -0.5
    outer loop
      vertex -10.7462 10.8253 6.25
      vertex -10.1127 21.3585 7.34731
      vertex -10.1127 10.0281 7.34731
    endloop
  endfacet
  facet normal 0.866025 0 -0.5
    outer loop
      vertex -10.1127 -21.3585 7.34731
      vertex -10.7462 -10.8253 6.25
      vertex -10.1127 -10.0281 7.34731
    endloop
  endfacet
  facet normal 0.866025 -3.61429e-08 -0.5
    outer loop
      vertex -10.1127 -21.3585 7.34731
      vertex -11.4193 -11.3444 5.08421
      vertex -10.7462 -10.8253 6.25
    endloop
  endfacet
  facet normal 0.866025 0 -0.5
    outer loop
      vertex -10.1127 -21.3585 7.34731
      vertex -11.4193 -21.55 5.08421
      vertex -11.4193 -11.3444 5.08421
    endloop
  endfacet
  facet normal 0.866025 1.25483e-05 -0.500001
    outer loop
      vertex -11.4193 -21.55 5.08421
      vertex -10.1127 -21.3585 7.34731
      vertex -11.3537 -21.5662 5.19779
    endloop
  endfacet
  facet normal 0.207912 0 -0.978148
    outer loop
      vertex -2.26298 -21.5308 12.2282
      vertex -1.30661 -12.4315 12.4315
      vertex -1.30661 -21.4141 12.4315
    endloop
  endfacet
  facet normal 0.207912 -1.75986e-08 -0.978148
    outer loop
      vertex -3.86271 -21.3904 11.8882
      vertex -1.30661 -12.4315 12.4315
      vertex -2.26298 -21.5308 12.2282
    endloop
  endfacet
  facet normal 0.207912 0 -0.978148
    outer loop
      vertex -1.30661 -12.4315 12.4315
      vertex -3.86271 -21.3904 11.8882
      vertex -3.86271 -11.8882 11.8882
    endloop
  endfacet
  facet normal 0.207912 0 -0.978148
    outer loop
      vertex -1.30661 12.4315 12.4315
      vertex -2.26298 21.5308 12.2282
      vertex -1.30661 21.4141 12.4315
    endloop
  endfacet
  facet normal 0.207912 0 -0.978148
    outer loop
      vertex -3.86271 21.3904 11.8882
      vertex -1.30661 12.4315 12.4315
      vertex -3.86271 11.8882 11.8882
    endloop
  endfacet
  facet normal 0.207912 1.75986e-08 -0.978148
    outer loop
      vertex -1.30661 12.4315 12.4315
      vertex -3.86271 21.3904 11.8882
      vertex -2.26298 21.5308 12.2282
    endloop
  endfacet
  facet normal -0 0 1
    outer loop
      vertex -1.30661 21.4141 -12.4315
      vertex 1.30661 12.4315 -12.4315
      vertex 1.30661 21.4141 -12.4315
    endloop
  endfacet
  facet normal 0 0 1
    outer loop
      vertex 1.30661 12.4315 -12.4315
      vertex -1.30661 21.4141 -12.4315
      vertex -1.30661 12.4315 -12.4315
    endloop
  endfacet
  facet normal -0 0 1
    outer loop
      vertex -1.30661 -12.4315 -12.4315
      vertex 1.30661 -21.4141 -12.4315
      vertex 1.30661 -12.4315 -12.4315
    endloop
  endfacet
  facet normal 0 0 1
    outer loop
      vertex 1.30661 -21.4141 -12.4315
      vertex -1.30661 -12.4315 -12.4315
      vertex -1.30661 -21.4141 -12.4315
    endloop
  endfacet
  facet normal -0.406736 0 0.913546
    outer loop
      vertex 3.86271 -11.8882 -11.8882
      vertex 6.25 -21.5069 -10.8253
      vertex 6.25 -10.8253 -10.8253
    endloop
  endfacet
  facet normal -0.406736 0 0.913546
    outer loop
      vertex 6.25 -21.5069 -10.8253
      vertex 3.86271 -11.8882 -11.8882
      vertex 3.86271 -21.3904 -11.8882
    endloop
  endfacet
  facet normal -0.406736 0 0.913546
    outer loop
      vertex 3.86271 11.8882 -11.8882
      vertex 6.25 21.5069 -10.8253
      vertex 3.86271 21.3904 -11.8882
    endloop
  endfacet
  facet normal -0.406736 0 0.913546
    outer loop
      vertex 6.25 21.5069 -10.8253
      vertex 3.86271 11.8882 -11.8882
      vertex 6.25 10.8253 -10.8253
    endloop
  endfacet
  facet normal -0.743145 1.13675e-06 -0.669131
    outer loop
      vertex 9.19717 -9.28931 8.36413
      vertex 8.36413 -9.28931 9.28931
      vertex 8.80248 -8.80248 8.80248
    endloop
  endfacet
  facet normal -0.743145 -2.70548e-07 -0.669131
    outer loop
      vertex 10.1127 -10.0281 7.34731
      vertex 8.36413 -9.28931 9.28931
      vertex 9.19717 -9.28931 8.36413
    endloop
  endfacet
  facet normal -0.743145 0 -0.669131
    outer loop
      vertex 9.63732 -21.3834 7.8753
      vertex 10.1127 -10.0281 7.34731
      vertex 10.1127 -21.3585 7.34731
    endloop
  endfacet
  facet normal -0.743145 6.32585e-09 -0.669131
    outer loop
      vertex 8.36413 -21.4501 9.28931
      vertex 10.1127 -10.0281 7.34731
      vertex 9.63732 -21.3834 7.8753
    endloop
  endfacet
  facet normal -0.743145 0 -0.669131
    outer loop
      vertex 10.1127 -10.0281 7.34731
      vertex 8.36413 -21.4501 9.28931
      vertex 8.36413 -9.28931 9.28931
    endloop
  endfacet
  facet normal -0.743145 0 -0.669131
    outer loop
      vertex 10.1127 10.0281 7.34731
      vertex 9.63732 21.3834 7.8753
      vertex 10.1127 21.3585 7.34731
    endloop
  endfacet
  facet normal -0.743145 -6.32585e-09 -0.669131
    outer loop
      vertex 10.1127 10.0281 7.34731
      vertex 8.36413 21.4501 9.28931
      vertex 9.63732 21.3834 7.8753
    endloop
  endfacet
  facet normal -0.743145 2.70548e-07 -0.669131
    outer loop
      vertex 8.36413 9.28931 9.28931
      vertex 10.1127 10.0281 7.34731
      vertex 9.19717 9.28931 8.36413
    endloop
  endfacet
  facet normal -0.743145 0 -0.669131
    outer loop
      vertex 10.1127 10.0281 7.34731
      vertex 8.36413 9.28931 9.28931
      vertex 8.36413 21.4501 9.28931
    endloop
  endfacet
  facet normal -0.743145 -1.13675e-06 -0.669131
    outer loop
      vertex 8.36413 9.28931 9.28931
      vertex 9.19717 9.28931 8.36413
      vertex 8.80248 8.80248 8.80248
    endloop
  endfacet
  facet normal 0.951056 -3.27946e-08 -0.309017
    outer loop
      vertex -11.4193 -21.55 5.08421
      vertex -12.2268 -12.1568 2.5989
      vertex -11.8162 -11.8882 3.86271
    endloop
  endfacet
  facet normal 0.951056 -4.34793e-08 -0.309017
    outer loop
      vertex -11.4193 -21.55 5.08421
      vertex -11.8162 -11.8882 3.86271
      vertex -11.572 -11.5536 4.61435
    endloop
  endfacet
  facet normal 0.951057 0 -0.309016
    outer loop
      vertex -11.4193 -21.55 5.08421
      vertex -11.572 -11.5536 4.61435
      vertex -11.4193 -11.3444 5.08421
    endloop
  endfacet
  facet normal 0.951057 0 -0.309017
    outer loop
      vertex -12.2268 -12.1568 2.5989
      vertex -11.4193 -21.55 5.08421
      vertex -12.2268 -21.4749 2.5989
    endloop
  endfacet
  facet normal 0.951057 0 -0.309017
    outer loop
      vertex -11.4193 21.55 5.08421
      vertex -12.2268 12.1568 2.5989
      vertex -12.2268 21.4749 2.5989
    endloop
  endfacet
  facet normal 0.951056 3.27946e-08 -0.309017
    outer loop
      vertex -11.4193 21.55 5.08421
      vertex -11.8162 11.8882 3.86271
      vertex -12.2268 12.1568 2.5989
    endloop
  endfacet
  facet normal 0.951056 4.34793e-08 -0.309017
    outer loop
      vertex -11.4193 21.55 5.08421
      vertex -11.572 11.5536 4.61435
      vertex -11.8162 11.8882 3.86271
    endloop
  endfacet
  facet normal 0.951057 0 -0.309016
    outer loop
      vertex -11.572 11.5536 4.61435
      vertex -11.4193 21.55 5.08421
      vertex -11.4193 11.3444 5.08421
    endloop
  endfacet
  facet normal 0.587785 0 -0.809017
    outer loop
      vertex -8.36413 21.4501 9.28931
      vertex -6.25 21.5069 10.8253
      vertex -6.25 10.8253 10.8253
    endloop
  endfacet
  facet normal 0.587785 -2.08759e-06 -0.809017
    outer loop
      vertex -6.25 21.5069 10.8253
      vertex -8.36413 21.4501 9.28931
      vertex -6.68624 21.6002 10.5084
    endloop
  endfacet
  facet normal 0.587785 1.9914e-07 -0.809017
    outer loop
      vertex -7.15415 21.6778 10.1684
      vertex -6.68624 21.6002 10.5084
      vertex -8.36413 21.4501 9.28931
    endloop
  endfacet
  facet normal 0.587776 -7.06651e-05 -0.809024
    outer loop
      vertex -6.68624 21.6002 10.5084
      vertex -7.08932 21.6771 10.2155
      vertex -7.04314 21.6765 10.2491
    endloop
  endfacet
  facet normal 0.587785 0 -0.809017
    outer loop
      vertex -8.36413 21.4501 9.28931
      vertex -6.25 10.8253 10.8253
      vertex -8.36413 9.28931 9.28931
    endloop
  endfacet
  facet normal 0.58779 4.21007e-05 -0.809014
    outer loop
      vertex -6.68624 21.6002 10.5084
      vertex -7.15415 21.6778 10.1684
      vertex -7.08932 21.6771 10.2155
    endloop
  endfacet
  facet normal 0.587785 2.08759e-06 -0.809017
    outer loop
      vertex -8.36413 -21.4501 9.28931
      vertex -6.25 -21.5069 10.8253
      vertex -6.68624 -21.6002 10.5084
    endloop
  endfacet
  facet normal 0.587784 1.23527e-05 -0.809018
    outer loop
      vertex -7.15415 -21.6778 10.1684
      vertex -6.68624 -21.6002 10.5084
      vertex -7.04314 -21.6765 10.2491
    endloop
  endfacet
  facet normal 0.587785 -1.9914e-07 -0.809017
    outer loop
      vertex -6.68624 -21.6002 10.5084
      vertex -7.15415 -21.6778 10.1684
      vertex -8.36413 -21.4501 9.28931
    endloop
  endfacet
  facet normal 0.587785 -1.97521e-08 -0.809017
    outer loop
      vertex -6.25 -21.5069 10.8253
      vertex -8.36413 -21.4501 9.28931
      vertex -6.25 -10.8253 10.8253
    endloop
  endfacet
  facet normal 0.587785 0 -0.809017
    outer loop
      vertex -6.25 -10.8253 10.8253
      vertex -8.36413 -21.4501 9.28931
      vertex -8.36413 -9.28931 9.28931
    endloop
  endfacet
  facet normal -0.743145 1.13675e-06 0.669131
    outer loop
      vertex 8.36413 -9.28931 -9.28931
      vertex 9.19717 -9.28931 -8.36413
      vertex 8.80248 -8.80248 -8.80248
    endloop
  endfacet
  facet normal -0.743145 -2.08128e-07 0.669131
    outer loop
      vertex 8.36413 -9.28931 -9.28931
      vertex 9.84898 -9.81526 -7.64022
      vertex 9.19717 -9.28931 -8.36413
    endloop
  endfacet
  facet normal -0.743145 -5.94373e-07 0.669131
    outer loop
      vertex 8.36413 -9.28931 -9.28931
      vertex 10.1127 -10.0281 -7.34731
      vertex 9.84898 -9.81526 -7.64022
    endloop
  endfacet
  facet normal -0.743145 0 0.669131
    outer loop
      vertex 8.36413 -21.4501 -9.28931
      vertex 10.1127 -10.0281 -7.34731
      vertex 8.36413 -9.28931 -9.28931
    endloop
  endfacet
  facet normal -0.743145 0 0.669131
    outer loop
      vertex 10.1127 -10.0281 -7.34731
      vertex 9.63732 -21.3834 -7.8753
      vertex 10.1127 -21.3585 -7.34731
    endloop
  endfacet
  facet normal -0.743145 6.32585e-09 0.669131
    outer loop
      vertex 10.1127 -10.0281 -7.34731
      vertex 8.36413 -21.4501 -9.28931
      vertex 9.63732 -21.3834 -7.8753
    endloop
  endfacet
  facet normal -0.743145 0 0.669131
    outer loop
      vertex 9.63732 21.3834 -7.8753
      vertex 10.1127 10.0281 -7.34731
      vertex 10.1127 21.3585 -7.34731
    endloop
  endfacet
  facet normal -0.743145 -6.32585e-09 0.669131
    outer loop
      vertex 8.36413 21.4501 -9.28931
      vertex 10.1127 10.0281 -7.34731
      vertex 9.63732 21.3834 -7.8753
    endloop
  endfacet
  facet normal -0.743145 0 0.669131
    outer loop
      vertex 8.36413 9.28931 -9.28931
      vertex 10.1127 10.0281 -7.34731
      vertex 8.36413 21.4501 -9.28931
    endloop
  endfacet
  facet normal -0.743145 5.94373e-07 0.669131
    outer loop
      vertex 10.1127 10.0281 -7.34731
      vertex 8.36413 9.28931 -9.28931
      vertex 9.84898 9.81526 -7.64022
    endloop
  endfacet
  facet normal -0.743145 2.08128e-07 0.669131
    outer loop
      vertex 8.36413 9.28931 -9.28931
      vertex 9.19717 9.28931 -8.36413
      vertex 9.84898 9.81526 -7.64022
    endloop
  endfacet
  facet normal -0.743145 -1.13675e-06 0.669131
    outer loop
      vertex 9.19717 9.28931 -8.36413
      vertex 8.36413 9.28931 -9.28931
      vertex 8.80248 8.80248 -8.80248
    endloop
  endfacet
  facet normal -0.951057 0 0.309017
    outer loop
      vertex 11.4193 21.55 -5.08421
      vertex 12.2268 12.1568 -2.5989
      vertex 12.2268 21.4749 -2.5989
    endloop
  endfacet
  facet normal -0.951056 3.27946e-08 0.309017
    outer loop
      vertex 11.4193 21.55 -5.08421
      vertex 11.8162 11.8882 -3.86271
      vertex 12.2268 12.1568 -2.5989
    endloop
  endfacet
  facet normal -0.951056 4.34793e-08 0.309017
    outer loop
      vertex 11.4193 21.55 -5.08421
      vertex 11.572 11.5536 -4.61435
      vertex 11.8162 11.8882 -3.86271
    endloop
  endfacet
  facet normal -0.951057 0 0.309016
    outer loop
      vertex 11.572 11.5536 -4.61435
      vertex 11.4193 21.55 -5.08421
      vertex 11.4193 11.3444 -5.08421
    endloop
  endfacet
  facet normal -0.951056 -3.27946e-08 0.309017
    outer loop
      vertex 11.4193 -21.55 -5.08421
      vertex 12.2268 -12.1568 -2.5989
      vertex 11.8162 -11.8882 -3.86271
    endloop
  endfacet
  facet normal -0.951056 -4.34793e-08 0.309017
    outer loop
      vertex 11.4193 -21.55 -5.08421
      vertex 11.8162 -11.8882 -3.86271
      vertex 11.572 -11.5536 -4.61435
    endloop
  endfacet
  facet normal -0.951057 0 0.309016
    outer loop
      vertex 11.4193 -21.55 -5.08421
      vertex 11.572 -11.5536 -4.61435
      vertex 11.4193 -11.3444 -5.08421
    endloop
  endfacet
  facet normal -0.951057 0 0.309017
    outer loop
      vertex 12.2268 -12.1568 -2.5989
      vertex 11.4193 -21.55 -5.08421
      vertex 12.2268 -21.4749 -2.5989
    endloop
  endfacet
  facet normal -0.994522 0 0.104528
    outer loop
      vertex 12.2268 -21.4749 -2.5989
      vertex 12.3627 -12.4315 -1.30661
      vertex 12.2268 -12.1568 -2.5989
    endloop
  endfacet
  facet normal -0.994522 -1.99482e-08 0.104529
    outer loop
      vertex 12.5 -21.6506 0
      vertex 12.3627 -12.4315 -1.30661
      vertex 12.2268 -21.4749 -2.5989
    endloop
  endfacet
  facet normal -0.994522 0 0.104529
    outer loop
      vertex 12.3627 -12.4315 -1.30661
      vertex 12.5 -21.6506 0
      vertex 12.5 -12.4315 0
    endloop
  endfacet
  facet normal -0.994522 0 0.104528
    outer loop
      vertex 12.2268 21.4749 -2.5989
      vertex 12.2268 12.1568 -2.5989
      vertex 12.3627 12.4315 -1.30661
    endloop
  endfacet
  facet normal -0.994522 1.99482e-08 0.104529
    outer loop
      vertex 12.3627 12.4315 -1.30661
      vertex 12.5 21.6506 0
      vertex 12.2268 21.4749 -2.5989
    endloop
  endfacet
  facet normal -0.994522 0 0.104529
    outer loop
      vertex 12.5 21.6506 0
      vertex 12.3627 12.4315 -1.30661
      vertex 12.5 12.4315 0
    endloop
  endfacet
  facet normal 0.866025 4.57631e-09 0.5
    outer loop
      vertex -10.1127 -21.3585 -7.34731
      vertex -11.1887 -11.1665 -5.48372
      vertex -11.4193 -11.3444 -5.08421
    endloop
  endfacet
  facet normal 0.866025 0 0.5
    outer loop
      vertex -10.3378 -10.3114 -6.95738
      vertex -10.1127 -21.3585 -7.34731
      vertex -10.1127 -10.0281 -7.34731
    endloop
  endfacet
  facet normal 0.866026 3.11324e-08 0.5
    outer loop
      vertex -10.7462 -10.8253 -6.25
      vertex -10.1127 -21.3585 -7.34731
      vertex -10.3378 -10.3114 -6.95738
    endloop
  endfacet
  facet normal 0.866025 -4.64367e-08 0.5
    outer loop
      vertex -11.1887 -11.1665 -5.48372
      vertex -10.1127 -21.3585 -7.34731
      vertex -10.7462 -10.8253 -6.25
    endloop
  endfacet
  facet normal 0.866025 -0 0.5
    outer loop
      vertex -11.4193 -21.55 -5.08421
      vertex -10.1127 -21.3585 -7.34731
      vertex -11.4193 -11.3444 -5.08421
    endloop
  endfacet
  facet normal 0.866025 1.25483e-05 0.500001
    outer loop
      vertex -10.1127 -21.3585 -7.34731
      vertex -11.4193 -21.55 -5.08421
      vertex -11.3537 -21.5662 -5.19779
    endloop
  endfacet
  facet normal 0.866025 -1.25483e-05 0.500001
    outer loop
      vertex -11.4193 21.55 -5.08421
      vertex -10.1127 21.3585 -7.34731
      vertex -11.3537 21.5662 -5.19779
    endloop
  endfacet
  facet normal 0.866025 -0 0.5
    outer loop
      vertex -11.4193 11.3444 -5.08421
      vertex -10.1127 21.3585 -7.34731
      vertex -11.4193 21.55 -5.08421
    endloop
  endfacet
  facet normal 0.866025 -4.57631e-09 0.5
    outer loop
      vertex -11.1887 11.1665 -5.48372
      vertex -10.1127 21.3585 -7.34731
      vertex -11.4193 11.3444 -5.08421
    endloop
  endfacet
  facet normal 0.866025 4.64367e-08 0.5
    outer loop
      vertex -10.7462 10.8253 -6.25
      vertex -10.1127 21.3585 -7.34731
      vertex -11.1887 11.1665 -5.48372
    endloop
  endfacet
  facet normal 0.866026 -3.11324e-08 0.5
    outer loop
      vertex -10.3378 10.3114 -6.95738
      vertex -10.1127 21.3585 -7.34731
      vertex -10.7462 10.8253 -6.25
    endloop
  endfacet
  facet normal 0.866025 0 0.5
    outer loop
      vertex -10.1127 21.3585 -7.34731
      vertex -10.3378 10.3114 -6.95738
      vertex -10.1127 10.0281 -7.34731
    endloop
  endfacet
  facet normal 0.207912 0 0.978148
    outer loop
      vertex -2.26298 21.5308 -12.2282
      vertex -1.30661 12.4315 -12.4315
      vertex -1.30661 21.4141 -12.4315
    endloop
  endfacet
  facet normal 0.207912 1.75986e-08 0.978148
    outer loop
      vertex -3.86271 21.3904 -11.8882
      vertex -1.30661 12.4315 -12.4315
      vertex -2.26298 21.5308 -12.2282
    endloop
  endfacet
  facet normal 0.207912 0 0.978148
    outer loop
      vertex -1.30661 12.4315 -12.4315
      vertex -3.86271 21.3904 -11.8882
      vertex -3.86271 11.8882 -11.8882
    endloop
  endfacet
  facet normal 0.207912 0 0.978148
    outer loop
      vertex -1.30661 -12.4315 -12.4315
      vertex -2.26298 -21.5308 -12.2282
      vertex -1.30661 -21.4141 -12.4315
    endloop
  endfacet
  facet normal 0.207912 -0 0.978148
    outer loop
      vertex -3.86271 -21.3904 -11.8882
      vertex -1.30661 -12.4315 -12.4315
      vertex -3.86271 -11.8882 -11.8882
    endloop
  endfacet
  facet normal 0.207912 -1.75986e-08 0.978148
    outer loop
      vertex -1.30661 -12.4315 -12.4315
      vertex -3.86271 -21.3904 -11.8882
      vertex -2.26298 -21.5308 -12.2282
    endloop
  endfacet
  facet normal 0.994522 -0 0.104529
    outer loop
      vertex -12.5 -21.6506 0
      vertex -12.3627 -12.4315 -1.30661
      vertex -12.5 -12.4315 0
    endloop
  endfacet
  facet normal 0.994522 -1.99482e-08 0.104529
    outer loop
      vertex -12.3627 -12.4315 -1.30661
      vertex -12.5 -21.6506 0
      vertex -12.2268 -21.4749 -2.5989
    endloop
  endfacet
  facet normal 0.994522 0 0.104528
    outer loop
      vertex -12.2268 -12.1568 -2.5989
      vertex -12.3627 -12.4315 -1.30661
      vertex -12.2268 -21.4749 -2.5989
    endloop
  endfacet
  facet normal 0.707107 0.707107 0
    outer loop
      vertex -12.5 21.6506 0
      vertex -12.5 21.6506 -1.81471e-05
      vertex -12.5 21.6506 -1.12429e-06
    endloop
  endfacet
  facet normal 0.994522 -0 0.104529
    outer loop
      vertex -12.5 12.4315 0
      vertex -12.5 21.6506 -1.81471e-05
      vertex -12.5 21.6506 0
    endloop
  endfacet
  facet normal 0.994522 -4.41513e-13 0.104529
    outer loop
      vertex -12.3627 12.4315 -1.30661
      vertex -12.5 21.6506 -1.81471e-05
      vertex -12.5 12.4315 0
    endloop
  endfacet
  facet normal 0.994522 1.99478e-08 0.104529
    outer loop
      vertex -12.5 21.6506 -1.81471e-05
      vertex -12.3627 12.4315 -1.30661
      vertex -12.2268 21.4749 -2.5989
    endloop
  endfacet
  facet normal 0.994522 0 0.104528
    outer loop
      vertex -12.2268 21.4749 -2.5989
      vertex -12.3627 12.4315 -1.30661
      vertex -12.2268 12.1568 -2.5989
    endloop
  endfacet
  facet normal -0.207912 1.75986e-08 0.978148
    outer loop
      vertex 1.30661 12.4315 -12.4315
      vertex 3.86271 21.3904 -11.8882
      vertex 2.26298 21.5308 -12.2282
    endloop
  endfacet
  facet normal -0.207912 0 0.978148
    outer loop
      vertex 1.30661 12.4315 -12.4315
      vertex 2.26298 21.5308 -12.2282
      vertex 1.30661 21.4141 -12.4315
    endloop
  endfacet
  facet normal -0.207912 0 0.978148
    outer loop
      vertex 3.86271 21.3904 -11.8882
      vertex 1.30661 12.4315 -12.4315
      vertex 3.86271 11.8882 -11.8882
    endloop
  endfacet
  facet normal -0.207912 0 0.978148
    outer loop
      vertex 1.30661 -12.4315 -12.4315
      vertex 3.86271 -21.3904 -11.8882
      vertex 3.86271 -11.8882 -11.8882
    endloop
  endfacet
  facet normal -0.207912 -1.75986e-08 0.978148
    outer loop
      vertex 3.86271 -21.3904 -11.8882
      vertex 1.30661 -12.4315 -12.4315
      vertex 2.26298 -21.5308 -12.2282
    endloop
  endfacet
  facet normal -0.207912 0 0.978148
    outer loop
      vertex 2.26298 -21.5308 -12.2282
      vertex 1.30661 -12.4315 -12.4315
      vertex 1.30661 -21.4141 -12.4315
    endloop
  endfacet
  facet normal -0.587786 0 0.809017
    outer loop
      vertex 6.25 -10.8253 -10.8253
      vertex 8.36413 -21.4501 -9.28931
      vertex 8.36413 -9.28931 -9.28931
    endloop
  endfacet
  facet normal -0.587786 0 0.809017
    outer loop
      vertex 6.25 -21.5069 -10.8253
      vertex 8.36413 -21.4501 -9.28931
      vertex 6.25 -10.8253 -10.8253
    endloop
  endfacet
  facet normal -0.587785 -1.07046e-06 0.809017
    outer loop
      vertex 8.36413 -21.4501 -9.28931
      vertex 6.68623 -21.6002 -10.5084
      vertex 7.15415 -21.6778 -10.1684
    endloop
  endfacet
  facet normal -0.587784 1.48796e-05 0.809018
    outer loop
      vertex 7.15415 -21.6778 -10.1684
      vertex 6.68623 -21.6002 -10.5084
      vertex 7.08932 -21.6771 -10.2155
    endloop
  endfacet
  facet normal -0.587787 -1.37543e-05 0.809016
    outer loop
      vertex 7.08932 -21.6771 -10.2155
      vertex 6.68623 -21.6002 -10.5084
      vertex 7.04314 -21.6765 -10.2491
    endloop
  endfacet
  facet normal -0.587786 2.9741e-07 0.809017
    outer loop
      vertex 8.36413 -21.4501 -9.28931
      vertex 6.25 -21.5069 -10.8253
      vertex 6.68623 -21.6002 -10.5084
    endloop
  endfacet
  facet normal -0.587785 1.07046e-06 0.809017
    outer loop
      vertex 6.68623 21.6002 -10.5084
      vertex 8.36413 21.4501 -9.28931
      vertex 7.15415 21.6778 -10.1684
    endloop
  endfacet
  facet normal -0.587785 -1.0498e-06 0.809017
    outer loop
      vertex 6.68623 21.6002 -10.5084
      vertex 7.15415 21.6778 -10.1684
      vertex 7.04314 21.6765 -10.2491
    endloop
  endfacet
  facet normal -0.587786 -2.9741e-07 0.809017
    outer loop
      vertex 6.25 21.5069 -10.8253
      vertex 8.36413 21.4501 -9.28931
      vertex 6.68623 21.6002 -10.5084
    endloop
  endfacet
  facet normal -0.587786 0 0.809017
    outer loop
      vertex 8.36413 21.4501 -9.28931
      vertex 6.25 21.5069 -10.8253
      vertex 6.25 10.8253 -10.8253
    endloop
  endfacet
  facet normal -0.587786 0 0.809017
    outer loop
      vertex 8.36413 21.4501 -9.28931
      vertex 6.25 10.8253 -10.8253
      vertex 8.36413 9.28931 -9.28931
    endloop
  endfacet
  facet normal 0.951057 0 0.309017
    outer loop
      vertex -11.4193 -21.55 -5.08421
      vertex -12.2268 -12.1568 -2.5989
      vertex -12.2268 -21.4749 -2.5989
    endloop
  endfacet
  facet normal 0.951056 -3.27946e-08 0.309017
    outer loop
      vertex -12.2268 -12.1568 -2.5989
      vertex -11.4193 -21.55 -5.08421
      vertex -11.8162 -11.8882 -3.86271
    endloop
  endfacet
  facet normal 0.951057 0 0.309017
    outer loop
      vertex -11.8162 -11.8882 -3.86271
      vertex -11.4193 -21.55 -5.08421
      vertex -11.4193 -11.3444 -5.08421
    endloop
  endfacet
  facet normal 0.951057 -0 0.309017
    outer loop
      vertex -12.2268 12.1568 -2.5989
      vertex -11.4193 21.55 -5.08421
      vertex -12.2268 21.4749 -2.5989
    endloop
  endfacet
  facet normal 0.951056 3.27946e-08 0.309017
    outer loop
      vertex -11.8162 11.8882 -3.86271
      vertex -11.4193 21.55 -5.08421
      vertex -12.2268 12.1568 -2.5989
    endloop
  endfacet
  facet normal 0.951057 0 0.309017
    outer loop
      vertex -11.4193 21.55 -5.08421
      vertex -11.8162 11.8882 -3.86271
      vertex -11.4193 11.3444 -5.08421
    endloop
  endfacet
  facet normal 0.406736 -0 0.913546
    outer loop
      vertex -6.25 -21.5069 -10.8253
      vertex -3.86271 -11.8882 -11.8882
      vertex -6.25 -10.8253 -10.8253
    endloop
  endfacet
  facet normal 0.406736 0 0.913546
    outer loop
      vertex -3.86271 -11.8882 -11.8882
      vertex -6.25 -21.5069 -10.8253
      vertex -3.86271 -21.3904 -11.8882
    endloop
  endfacet
  facet normal 0.406736 0 0.913546
    outer loop
      vertex -6.25 21.5069 -10.8253
      vertex -3.86271 11.8882 -11.8882
      vertex -3.86271 21.3904 -11.8882
    endloop
  endfacet
  facet normal 0.406736 3.63144e-08 0.913546
    outer loop
      vertex -3.86271 11.8882 -11.8882
      vertex -6.25 21.5069 -10.8253
      vertex -6.25 10.8253 -10.8253
    endloop
  endfacet
  facet normal 0.743145 1.13675e-06 0.669131
    outer loop
      vertex -9.19717 -9.28931 -8.36413
      vertex -8.36413 -9.28931 -9.28931
      vertex -8.80248 -8.80248 -8.80248
    endloop
  endfacet
  facet normal 0.743145 -2.70548e-07 0.669131
    outer loop
      vertex -10.1127 -10.0281 -7.34731
      vertex -8.36413 -9.28931 -9.28931
      vertex -9.19717 -9.28931 -8.36413
    endloop
  endfacet
  facet normal 0.743145 0 0.669131
    outer loop
      vertex -9.63732 -21.3834 -7.8753
      vertex -10.1127 -10.0281 -7.34731
      vertex -10.1127 -21.3585 -7.34731
    endloop
  endfacet
  facet normal 0.743145 6.32585e-09 0.669131
    outer loop
      vertex -8.36413 -21.4501 -9.28931
      vertex -10.1127 -10.0281 -7.34731
      vertex -9.63732 -21.3834 -7.8753
    endloop
  endfacet
  facet normal 0.743145 0 0.669131
    outer loop
      vertex -10.1127 -10.0281 -7.34731
      vertex -8.36413 -21.4501 -9.28931
      vertex -8.36413 -9.28931 -9.28931
    endloop
  endfacet
  facet normal 0.743145 -0 0.669131
    outer loop
      vertex -10.1127 10.0281 -7.34731
      vertex -9.63732 21.3834 -7.8753
      vertex -10.1127 21.3585 -7.34731
    endloop
  endfacet
  facet normal 0.743145 -6.32585e-09 0.669131
    outer loop
      vertex -10.1127 10.0281 -7.34731
      vertex -8.36413 21.4501 -9.28931
      vertex -9.63732 21.3834 -7.8753
    endloop
  endfacet
  facet normal 0.743145 2.70548e-07 0.669131
    outer loop
      vertex -8.36413 9.28931 -9.28931
      vertex -10.1127 10.0281 -7.34731
      vertex -9.19717 9.28931 -8.36413
    endloop
  endfacet
  facet normal 0.743145 0 0.669131
    outer loop
      vertex -10.1127 10.0281 -7.34731
      vertex -8.36413 9.28931 -9.28931
      vertex -8.36413 21.4501 -9.28931
    endloop
  endfacet
  facet normal 0.743145 -1.13675e-06 0.669131
    outer loop
      vertex -8.36413 9.28931 -9.28931
      vertex -9.19717 9.28931 -8.36413
      vertex -8.80248 8.80248 -8.80248
    endloop
  endfacet
  facet normal 0.587786 0 0.809017
    outer loop
      vertex -8.36413 -21.4501 -9.28931
      vertex -6.25 -21.5069 -10.8253
      vertex -6.25 -10.8253 -10.8253
    endloop
  endfacet
  facet normal 0.587786 2.9741e-07 0.809017
    outer loop
      vertex -6.25 -21.5069 -10.8253
      vertex -8.36413 -21.4501 -9.28931
      vertex -6.68623 -21.6002 -10.5084
    endloop
  endfacet
  facet normal 0.587785 -1.07046e-06 0.809017
    outer loop
      vertex -7.15415 -21.6778 -10.1684
      vertex -6.68623 -21.6002 -10.5084
      vertex -8.36413 -21.4501 -9.28931
    endloop
  endfacet
  facet normal 0.587786 -0 0.809017
    outer loop
      vertex -8.36413 -21.4501 -9.28931
      vertex -6.25 -10.8253 -10.8253
      vertex -8.36413 -9.28931 -9.28931
    endloop
  endfacet
  facet normal 0.587785 1.0498e-06 0.809017
    outer loop
      vertex -6.68623 -21.6002 -10.5084
      vertex -7.15415 -21.6778 -10.1684
      vertex -7.04314 -21.6765 -10.2491
    endloop
  endfacet
  facet normal 0.587786 -2.9741e-07 0.809017
    outer loop
      vertex -8.36413 21.4501 -9.28931
      vertex -6.25 21.5069 -10.8253
      vertex -6.68623 21.6002 -10.5084
    endloop
  endfacet
  facet normal 0.587787 1.37543e-05 0.809016
    outer loop
      vertex -7.08932 21.6771 -10.2155
      vertex -6.68623 21.6002 -10.5084
      vertex -7.04314 21.6765 -10.2491
    endloop
  endfacet
  facet normal 0.587784 -1.48796e-05 0.809018
    outer loop
      vertex -7.15415 21.6778 -10.1684
      vertex -6.68623 21.6002 -10.5084
      vertex -7.08932 21.6771 -10.2155
    endloop
  endfacet
  facet normal 0.587785 1.07046e-06 0.809017
    outer loop
      vertex -6.68623 21.6002 -10.5084
      vertex -7.15415 21.6778 -10.1684
      vertex -8.36413 21.4501 -9.28931
    endloop
  endfacet
  facet normal 0.587786 0 0.809017
    outer loop
      vertex -6.25 21.5069 -10.8253
      vertex -8.36413 21.4501 -9.28931
      vertex -6.25 10.8253 -10.8253
    endloop
  endfacet
  facet normal 0.587786 0 0.809017
    outer loop
      vertex -6.25 10.8253 -10.8253
      vertex -8.36413 21.4501 -9.28931
      vertex -8.36413 9.28931 -9.28931
    endloop
  endfacet
  facet normal 0 -0.104529 0.994522
    outer loop
      vertex -21.377 2.5989 -12.2268
      vertex -12.4177 0.783091 -12.4177
      vertex -12.2268 2.5989 -12.2268
    endloop
  endfacet
  facet normal -6.56705e-08 -0.104529 0.994522
    outer loop
      vertex -21.4048 1.97347 -12.2926
      vertex -12.4177 0.783091 -12.4177
      vertex -21.377 2.5989 -12.2268
    endloop
  endfacet
  facet normal 9.19611e-09 -0.104529 0.994522
    outer loop
      vertex -12.4177 0.783091 -12.4177
      vertex -21.4048 1.97347 -12.2926
      vertex -21.4744 0.406491 -12.4573
    endloop
  endfacet
  facet normal -7.54111e-08 -0.104528 0.994522
    outer loop
      vertex 21.377 2.5989 -12.2268
      vertex 12.3096 1.81131 -12.3096
      vertex 21.3953 2.18911 -12.2699
    endloop
  endfacet
  facet normal 0 -0.104529 0.994522
    outer loop
      vertex 12.3096 1.81131 -12.3096
      vertex 21.377 2.5989 -12.2268
      vertex 12.2268 2.5989 -12.2268
    endloop
  endfacet
  facet normal -8.67737e-09 -0.104528 0.994522
    outer loop
      vertex -21.4744 0.406491 -12.4573
      vertex -12.5 0 -12.5
      vertex -12.4177 0.783091 -12.4177
    endloop
  endfacet
  facet normal 0 -0.104528 0.994522
    outer loop
      vertex -12.5 0 -12.5
      vertex -21.4744 0.406491 -12.4573
      vertex -21.4925 0 -12.5
    endloop
  endfacet
  facet normal 0 -0.104528 0.994522
    outer loop
      vertex 21.4048 1.97347 -12.2926
      vertex 12.5 0 -12.5
      vertex 21.4925 0 -12.5
    endloop
  endfacet
  facet normal 4.3569e-08 -0.104531 0.994522
    outer loop
      vertex 12.3096 1.81131 -12.3096
      vertex 21.4048 1.97347 -12.2926
      vertex 21.3953 2.18911 -12.2699
    endloop
  endfacet
  facet normal -1.18183e-08 -0.104528 0.994522
    outer loop
      vertex 21.4048 1.97347 -12.2926
      vertex 12.3096 1.81131 -12.3096
      vertex 12.5 0 -12.5
    endloop
  endfacet
  facet normal -2.62111e-08 0.809016 0.587787
    outer loop
      vertex 21.444 -10.7525 -6.35017
      vertex 10.7462 -10.8253 -6.25
      vertex 21.4394 -10.8253 -6.25
    endloop
  endfacet
  facet normal -4.79067e-08 0.809017 0.587785
    outer loop
      vertex 21.5188 -9.58077 -7.96296
      vertex 10.7462 -10.8253 -6.25
      vertex 21.444 -10.7525 -6.35017
    endloop
  endfacet
  facet normal -1.57207e-06 0.809021 0.587779
    outer loop
      vertex 21.5113 -9.52865 -8.03471
      vertex 10.7462 -10.8253 -6.25
      vertex 21.5188 -9.58077 -7.96296
    endloop
  endfacet
  facet normal 5.65188e-08 0.809017 0.587786
    outer loop
      vertex 10.7462 -10.8253 -6.25
      vertex 21.5113 -9.52865 -8.03471
      vertex 10.1127 -10.0281 -7.34731
    endloop
  endfacet
  facet normal -4.29974e-08 0.809018 0.587784
    outer loop
      vertex 10.1127 -10.0281 -7.34731
      vertex 21.5113 -9.52865 -8.03471
      vertex 9.84898 -9.81526 -7.64022
    endloop
  endfacet
  facet normal -2.62109e-08 0.809018 0.587784
    outer loop
      vertex -10.7462 -10.8253 -6.25
      vertex -21.444 -10.7525 -6.35017
      vertex -21.4394 -10.8253 -6.25
    endloop
  endfacet
  facet normal -6.8015e-08 0.809016 0.587787
    outer loop
      vertex -10.7462 -10.8253 -6.25
      vertex -21.4547 -10.5846 -6.58134
      vertex -21.444 -10.7525 -6.35017
    endloop
  endfacet
  facet normal -4.77206e-09 0.809017 0.587785
    outer loop
      vertex -21.4547 -10.5846 -6.58134
      vertex -10.7462 -10.8253 -6.25
      vertex -10.3378 -10.3114 -6.95738
    endloop
  endfacet
  facet normal -5.59099e-08 0.809017 -0.587785
    outer loop
      vertex 10.3378 -10.3114 6.95738
      vertex 21.4772 -9.28931 8.36413
      vertex 21.5188 -9.58077 7.96297
    endloop
  endfacet
  facet normal 0 0.809017 -0.587785
    outer loop
      vertex 21.4772 -9.28931 8.36413
      vertex 10.1127 -10.0281 7.34731
      vertex 9.19717 -9.28931 8.36413
    endloop
  endfacet
  facet normal 1.04275e-07 0.809016 -0.587786
    outer loop
      vertex 21.4772 -9.28931 8.36413
      vertex 10.3378 -10.3114 6.95738
      vertex 10.1127 -10.0281 7.34731
    endloop
  endfacet
  facet normal -9.12501e-09 0.809017 -0.587786
    outer loop
      vertex 10.3378 -10.3114 6.95738
      vertex 21.5188 -9.58077 7.96297
      vertex 21.4547 -10.5846 6.58134
    endloop
  endfacet
  facet normal 0 0.809017 -0.587786
    outer loop
      vertex -9.84898 -9.81526 7.64022
      vertex -21.4772 -9.28931 8.36413
      vertex -9.19717 -9.28931 8.36413
    endloop
  endfacet
  facet normal -3.61669e-08 0.809016 -0.587786
    outer loop
      vertex -21.4772 -9.28931 8.36413
      vertex -9.84898 -9.81526 7.64022
      vertex -21.5113 -9.52865 8.03471
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex -12.5 12.4315 0
      vertex -21.3743 12.4315 1.30661
      vertex -21.532 12.4315 0
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex -21.3743 12.4315 1.30661
      vertex -12.5 12.4315 0
      vertex -12.3627 12.4315 1.30661
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex -21.3743 12.4315 -1.30661
      vertex -12.5 12.4315 0
      vertex -21.532 12.4315 0
    endloop
  endfacet
  facet normal 0 -1 -0
    outer loop
      vertex -12.5 12.4315 0
      vertex -21.3743 12.4315 -1.30661
      vertex -12.3627 12.4315 -1.30661
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex 12.5 12.4315 0
      vertex 21.3743 12.4315 1.30661
      vertex 12.3627 12.4315 1.30661
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex 12.5 12.4315 0
      vertex 21.532 12.4315 0
      vertex 21.3743 12.4315 1.30661
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex 21.3743 12.4315 -1.30661
      vertex 12.5 12.4315 0
      vertex 12.3627 12.4315 -1.30661
    endloop
  endfacet
  facet normal 0 -1 0
    outer loop
      vertex 12.5 12.4315 0
      vertex 21.3743 12.4315 -1.30661
      vertex 21.532 12.4315 0
    endloop
  endfacet
  facet normal 0 0.309017 0.951057
    outer loop
      vertex -21.5403 -4.57854 -11.5836
      vertex -12.2268 -2.5989 -12.2268
      vertex -21.377 -2.5989 -12.2268
    endloop
  endfacet
  facet normal 7.23093e-08 0.309017 0.951057
    outer loop
      vertex -21.4752 -5.08421 -11.4193
      vertex -12.2268 -2.5989 -12.2268
      vertex -21.5403 -4.57854 -11.5836
    endloop
  endfacet
  facet normal 0 0.309017 0.951057
    outer loop
      vertex -12.2268 -2.5989 -12.2268
      vertex -21.4752 -5.08421 -11.4193
      vertex -11.4193 -5.08421 -11.4193
    endloop
  endfacet
  facet normal -0 0.309017 0.951057
    outer loop
      vertex 12.2268 -2.5989 -12.2268
      vertex 21.5403 -4.57854 -11.5836
      vertex 21.377 -2.5989 -12.2268
    endloop
  endfacet
  facet normal -7.23093e-08 0.309017 0.951057
    outer loop
      vertex 21.5403 -4.57854 -11.5836
      vertex 12.2268 -2.5989 -12.2268
      vertex 21.4752 -5.08421 -11.4193
    endloop
  endfacet
  facet normal 0 0.309017 0.951057
    outer loop
      vertex 21.4752 -5.08421 -11.4193
      vertex 12.2268 -2.5989 -12.2268
      vertex 11.4193 -5.08421 -11.4193
    endloop
  endfacet
  facet normal -2.62111e-08 -0.809016 0.587787
    outer loop
      vertex 10.7462 10.8253 -6.25
      vertex 21.444 10.7525 -6.35017
      vertex 21.4394 10.8253 -6.25
    endloop
  endfacet
  facet normal 5.65188e-08 -0.809017 0.587786
    outer loop
      vertex 21.5113 9.52865 -8.03471
      vertex 10.7462 10.8253 -6.25
      vertex 10.1127 10.0281 -7.34731
    endloop
  endfacet
  facet normal -4.29974e-08 -0.809018 0.587784
    outer loop
      vertex 21.5113 9.52865 -8.03471
      vertex 10.1127 10.0281 -7.34731
      vertex 9.84898 9.81526 -7.64022
    endloop
  endfacet
  facet normal -4.79067e-08 -0.809017 0.587785
    outer loop
      vertex 10.7462 10.8253 -6.25
      vertex 21.5188 9.58077 -7.96296
      vertex 21.444 10.7525 -6.35017
    endloop
  endfacet
  facet normal -1.57207e-06 -0.809021 0.587779
    outer loop
      vertex 10.7462 10.8253 -6.25
      vertex 21.5113 9.52865 -8.03471
      vertex 21.5188 9.58077 -7.96296
    endloop
  endfacet
  facet normal -2.62109e-08 -0.809018 0.587784
    outer loop
      vertex -21.444 10.7525 -6.35017
      vertex -10.7462 10.8253 -6.25
      vertex -21.4394 10.8253 -6.25
    endloop
  endfacet
  facet normal -6.8015e-08 -0.809016 0.587787
    outer loop
      vertex -21.4547 10.5846 -6.58134
      vertex -10.7462 10.8253 -6.25
      vertex -21.444 10.7525 -6.35017
    endloop
  endfacet
  facet normal -4.77206e-09 -0.809017 0.587785
    outer loop
      vertex -10.7462 10.8253 -6.25
      vertex -21.4547 10.5846 -6.58134
      vertex -10.3378 10.3114 -6.95738
    endloop
  endfacet
  facet normal 0 -0.309017 0.951057
    outer loop
      vertex -21.4752 5.08421 -11.4193
      vertex -12.2268 2.5989 -12.2268
      vertex -11.4193 5.08421 -11.4193
    endloop
  endfacet
  facet normal 7.23093e-08 -0.309017 0.951057
    outer loop
      vertex -12.2268 2.5989 -12.2268
      vertex -21.4752 5.08421 -11.4193
      vertex -21.5403 4.57854 -11.5836
    endloop
  endfacet
  facet normal 0 -0.309017 0.951057
    outer loop
      vertex -12.2268 2.5989 -12.2268
      vertex -21.5403 4.57854 -11.5836
      vertex -21.377 2.5989 -12.2268
    endloop
  endfacet
  facet normal -7.23093e-08 -0.309017 0.951057
    outer loop
      vertex 12.2268 2.5989 -12.2268
      vertex 21.5403 4.57854 -11.5836
      vertex 21.4752 5.08421 -11.4193
    endloop
  endfacet
  facet normal 0 -0.309017 0.951057
    outer loop
      vertex 21.5403 4.57854 -11.5836
      vertex 12.2268 2.5989 -12.2268
      vertex 21.377 2.5989 -12.2268
    endloop
  endfacet
  facet normal 0 -0.309017 0.951057
    outer loop
      vertex 12.2268 2.5989 -12.2268
      vertex 21.4752 5.08421 -11.4193
      vertex 11.4193 5.08421 -11.4193
    endloop
  endfacet
  facet normal 8.06309e-07 -0.669131 -0.743145
    outer loop
      vertex -10.1127 7.34731 10.1127
      vertex -9.19717 9.28931 8.36413
      vertex -8.80248 8.80248 8.80248
    endloop
  endfacet
  facet normal 0 -0.669131 -0.743145
    outer loop
      vertex -10.1127 7.34731 10.1127
      vertex -21.4772 9.28931 8.36413
      vertex -9.19717 9.28931 8.36413
    endloop
  endfacet
  facet normal 5.2907e-09 -0.669131 -0.743145
    outer loop
      vertex -10.1127 7.34731 10.1127
      vertex -21.4936 8.42289 9.14426
      vertex -21.4772 9.28931 8.36413
    endloop
  endfacet
  facet normal 0 -0.669131 -0.743145
    outer loop
      vertex -21.4936 8.42289 9.14426
      vertex -10.1127 7.34731 10.1127
      vertex -21.5141 7.34731 10.1127
    endloop
  endfacet
  facet normal 0 -0.669131 -0.743145
    outer loop
      vertex 10.1127 7.34731 10.1127
      vertex 21.4936 8.42289 9.14426
      vertex 21.5141 7.34731 10.1127
    endloop
  endfacet
  facet normal -5.2907e-09 -0.669131 -0.743145
    outer loop
      vertex 10.1127 7.34731 10.1127
      vertex 21.4772 9.28931 8.36413
      vertex 21.4936 8.42289 9.14426
    endloop
  endfacet
  facet normal 0 -0.669131 -0.743145
    outer loop
      vertex 10.1127 7.34731 10.1127
      vertex 9.19717 9.28931 8.36413
      vertex 21.4772 9.28931 8.36413
    endloop
  endfacet
  facet normal -8.06309e-07 -0.669131 -0.743145
    outer loop
      vertex 9.19717 9.28931 8.36413
      vertex 10.1127 7.34731 10.1127
      vertex 8.80248 8.80248 8.80248
    endloop
  endfacet
  facet normal 0 0.978148 -0.207912
    outer loop
      vertex -12.2268 -12.1568 2.5989
      vertex -21.3795 -11.8882 3.86271
      vertex -11.8162 -11.8882 3.86271
    endloop
  endfacet
  facet normal 8.79501e-09 0.978148 -0.207912
    outer loop
      vertex -12.2268 -12.1568 2.5989
      vertex -21.3767 -12.1806 2.48718
      vertex -21.3795 -11.8882 3.86271
    endloop
  endfacet
  facet normal 4.93991e-09 0.978148 -0.207912
    outer loop
      vertex -12.3627 -12.4315 1.30661
      vertex -21.3767 -12.1806 2.48718
      vertex -12.2268 -12.1568 2.5989
    endloop
  endfacet
  facet normal 0 0.978148 -0.207912
    outer loop
      vertex -21.3767 -12.1806 2.48718
      vertex -12.3627 -12.4315 1.30661
      vertex -21.3743 -12.4315 1.30661
    endloop
  endfacet
  facet normal 0 0.978148 -0.207912
    outer loop
      vertex 21.3795 -11.8882 3.86271
      vertex 12.2268 -12.1568 2.5989
      vertex 11.8162 -11.8882 3.86271
    endloop
  endfacet
  facet normal -8.79501e-09 0.978148 -0.207912
    outer loop
      vertex 21.3767 -12.1806 2.48718
      vertex 12.2268 -12.1568 2.5989
      vertex 21.3795 -11.8882 3.86271
    endloop
  endfacet
  facet normal 0 0.978148 -0.207912
    outer loop
      vertex 12.3627 -12.4315 1.30661
      vertex 21.3767 -12.1806 2.48718
      vertex 21.3743 -12.4315 1.30661
    endloop
  endfacet
  facet normal -4.93991e-09 0.978148 -0.207912
    outer loop
      vertex 21.3767 -12.1806 2.48718
      vertex 12.3627 -12.4315 1.30661
      vertex 12.2268 -12.1568 2.5989
    endloop
  endfacet
  facet normal -4.2482e-08 0.913546 0.406737
    outer loop
      vertex 21.4394 -10.8253 -6.25
      vertex 11.572 -11.5536 -4.61435
      vertex 21.4306 -11.7212 -4.23778
    endloop
  endfacet
  facet normal -4.45784e-07 0.913546 0.406737
    outer loop
      vertex 21.4394 -10.8253 -6.25
      vertex 21.4306 -11.7212 -4.23778
      vertex 21.5615 -11.2938 -5.19779
    endloop
  endfacet
  facet normal 3.64177e-09 0.913545 0.406737
    outer loop
      vertex 11.572 -11.5536 -4.61435
      vertex 21.4394 -10.8253 -6.25
      vertex 11.4193 -11.3444 -5.08421
    endloop
  endfacet
  facet normal -1.81375e-08 0.913545 0.406737
    outer loop
      vertex 11.4193 -11.3444 -5.08421
      vertex 21.4394 -10.8253 -6.25
      vertex 10.7462 -10.8253 -6.25
    endloop
  endfacet
  facet normal -1.81375e-08 0.913546 0.406737
    outer loop
      vertex -21.4394 -10.8253 -6.25
      vertex -11.1887 -11.1665 -5.48372
      vertex -10.7462 -10.8253 -6.25
    endloop
  endfacet
  facet normal 8.82484e-08 0.913546 0.406735
    outer loop
      vertex -11.1887 -11.1665 -5.48372
      vertex -21.4394 -10.8253 -6.25
      vertex -21.4827 -10.9914 -5.87692
    endloop
  endfacet
  facet normal 8.06309e-07 0.669131 -0.743145
    outer loop
      vertex -9.19717 -9.28931 8.36413
      vertex -10.1127 -7.34731 10.1127
      vertex -8.80248 -8.80248 8.80248
    endloop
  endfacet
  facet normal 0 0.669131 -0.743145
    outer loop
      vertex -21.4772 -9.28931 8.36413
      vertex -10.1127 -7.34731 10.1127
      vertex -9.19717 -9.28931 8.36413
    endloop
  endfacet
  facet normal 5.2907e-09 0.669131 -0.743145
    outer loop
      vertex -21.4936 -8.42289 9.14426
      vertex -10.1127 -7.34731 10.1127
      vertex -21.4772 -9.28931 8.36413
    endloop
  endfacet
  facet normal 0 0.669131 -0.743145
    outer loop
      vertex -10.1127 -7.34731 10.1127
      vertex -21.4936 -8.42289 9.14426
      vertex -21.5141 -7.34731 10.1127
    endloop
  endfacet
  facet normal 0 0.669131 -0.743145
    outer loop
      vertex 21.4936 -8.42289 9.14426
      vertex 10.1127 -7.34731 10.1127
      vertex 21.5141 -7.34731 10.1127
    endloop
  endfacet
  facet normal -5.2907e-09 0.669131 -0.743145
    outer loop
      vertex 21.4772 -9.28931 8.36413
      vertex 10.1127 -7.34731 10.1127
      vertex 21.4936 -8.42289 9.14426
    endloop
  endfacet
  facet normal 0 0.669131 -0.743145
    outer loop
      vertex 9.19717 -9.28931 8.36413
      vertex 10.1127 -7.34731 10.1127
      vertex 21.4772 -9.28931 8.36413
    endloop
  endfacet
  facet normal -8.06309e-07 0.669131 -0.743145
    outer loop
      vertex 10.1127 -7.34731 10.1127
      vertex 9.19717 -9.28931 8.36413
      vertex 8.80248 -8.80248 8.80248
    endloop
  endfacet
  facet normal 0 0.104529 0.994522
    outer loop
      vertex -12.4177 -0.783091 -12.4177
      vertex -21.377 -2.5989 -12.2268
      vertex -12.2268 -2.5989 -12.2268
    endloop
  endfacet
  facet normal -6.56705e-08 0.104529 0.994522
    outer loop
      vertex -12.4177 -0.783091 -12.4177
      vertex -21.4048 -1.97347 -12.2926
      vertex -21.377 -2.5989 -12.2268
    endloop
  endfacet
  facet normal 9.19611e-09 0.104529 0.994522
    outer loop
      vertex -21.4048 -1.97347 -12.2926
      vertex -12.4177 -0.783091 -12.4177
      vertex -21.4744 -0.406491 -12.4573
    endloop
  endfacet
  facet normal -7.54111e-08 0.104528 0.994522
    outer loop
      vertex 12.3096 -1.81131 -12.3096
      vertex 21.377 -2.5989 -12.2268
      vertex 21.3953 -2.18911 -12.2699
    endloop
  endfacet
  facet normal 0 0.104529 0.994522
    outer loop
      vertex 21.377 -2.5989 -12.2268
      vertex 12.3096 -1.81131 -12.3096
      vertex 12.2268 -2.5989 -12.2268
    endloop
  endfacet
  facet normal -8.67737e-09 0.104528 0.994522
    outer loop
      vertex -12.5 0 -12.5
      vertex -21.4744 -0.406491 -12.4573
      vertex -12.4177 -0.783091 -12.4177
    endloop
  endfacet
  facet normal 0 0.104528 0.994522
    outer loop
      vertex -21.4744 -0.406491 -12.4573
      vertex -12.5 0 -12.5
      vertex -21.4925 0 -12.5
    endloop
  endfacet
  facet normal -0 0.104528 0.994522
    outer loop
      vertex 12.5 0 -12.5
      vertex 21.4048 -1.97347 -12.2926
      vertex 21.4925 0 -12.5
    endloop
  endfacet
  facet normal -1.18183e-08 0.104528 0.994522
    outer loop
      vertex 12.3096 -1.81131 -12.3096
      vertex 21.4048 -1.97347 -12.2926
      vertex 12.5 0 -12.5
    endloop
  endfacet
  facet normal 4.3569e-08 0.104531 0.994522
    outer loop
      vertex 21.4048 -1.97347 -12.2926
      vertex 12.3096 -1.81131 -12.3096
      vertex 21.3953 -2.18911 -12.2699
    endloop
  endfacet
  facet normal 0 -0.5 0.866025
    outer loop
      vertex -21.5141 7.34731 -10.1127
      vertex -11.4193 5.08421 -11.4193
      vertex -10.1127 7.34731 -10.1127
    endloop
  endfacet
  facet normal 0 -0.5 0.866025
    outer loop
      vertex -11.4193 5.08421 -11.4193
      vertex -21.4855 5.51743 -11.1692
      vertex -21.4752 5.08421 -11.4193
    endloop
  endfacet
  facet normal -3.73796e-08 -0.5 0.866025
    outer loop
      vertex -21.5265 7.25083 -10.1684
      vertex -11.4193 5.08421 -11.4193
      vertex -21.5141 7.34731 -10.1127
    endloop
  endfacet
  facet normal 1.17677e-08 -0.5 0.866025
    outer loop
      vertex -11.4193 5.08421 -11.4193
      vertex -21.5265 7.25083 -10.1684
      vertex -21.4855 5.51743 -11.1692
    endloop
  endfacet
  facet normal 3.73796e-08 -0.5 0.866025
    outer loop
      vertex 11.4193 5.08421 -11.4193
      vertex 21.5265 7.25083 -10.1684
      vertex 21.5141 7.34731 -10.1127
    endloop
  endfacet
  facet normal -1.17677e-08 -0.5 0.866025
    outer loop
      vertex 21.5265 7.25083 -10.1684
      vertex 11.4193 5.08421 -11.4193
      vertex 21.4855 5.51743 -11.1692
    endloop
  endfacet
  facet normal 0 -0.5 0.866025
    outer loop
      vertex 21.4855 5.51743 -11.1692
      vertex 11.4193 5.08421 -11.4193
      vertex 21.4752 5.08421 -11.4193
    endloop
  endfacet
  facet normal 0 -0.5 0.866025
    outer loop
      vertex 11.4193 5.08421 -11.4193
      vertex 21.5141 7.34731 -10.1127
      vertex 10.1127 7.34731 -10.1127
    endloop
  endfacet
  facet normal 8.06309e-07 -0.669131 0.743145
    outer loop
      vertex -9.19717 9.28931 -8.36413
      vertex -10.1127 7.34731 -10.1127
      vertex -8.80248 8.80248 -8.80248
    endloop
  endfacet
  facet normal 0 -0.669131 0.743145
    outer loop
      vertex -21.4772 9.28931 -8.36413
      vertex -10.1127 7.34731 -10.1127
      vertex -9.19717 9.28931 -8.36413
    endloop
  endfacet
  facet normal 5.2907e-09 -0.669131 0.743145
    outer loop
      vertex -21.4936 8.42289 -9.14426
      vertex -10.1127 7.34731 -10.1127
      vertex -21.4772 9.28931 -8.36413
    endloop
  endfacet
  facet normal 0 -0.669131 0.743145
    outer loop
      vertex -10.1127 7.34731 -10.1127
      vertex -21.4936 8.42289 -9.14426
      vertex -21.5141 7.34731 -10.1127
    endloop
  endfacet
  facet normal 0 -0.669131 0.743145
    outer loop
      vertex 21.4936 8.42289 -9.14426
      vertex 10.1127 7.34731 -10.1127
      vertex 21.5141 7.34731 -10.1127
    endloop
  endfacet
  facet normal -5.2907e-09 -0.669131 0.743145
    outer loop
      vertex 21.4772 9.28931 -8.36413
      vertex 10.1127 7.34731 -10.1127
      vertex 21.4936 8.42289 -9.14426
    endloop
  endfacet
  facet normal 0 -0.669131 0.743145
    outer loop
      vertex 9.19717 9.28931 -8.36413
      vertex 10.1127 7.34731 -10.1127
      vertex 21.4772 9.28931 -8.36413
    endloop
  endfacet
  facet normal -8.06309e-07 -0.669131 0.743145
    outer loop
      vertex 10.1127 7.34731 -10.1127
      vertex 9.19717 9.28931 -8.36413
      vertex 8.80248 8.80248 -8.80248
    endloop
  endfacet
  facet normal 9.12501e-09 -0.809017 0.587786
    outer loop
      vertex -21.5188 9.58077 -7.96297
      vertex -10.3378 10.3114 -6.95738
      vertex -21.4547 10.5846 -6.58134
    endloop
  endfacet
  facet normal 5.59099e-08 -0.809017 0.587785
    outer loop
      vertex -21.4772 9.28931 -8.36413
      vertex -10.3378 10.3114 -6.95738
      vertex -21.5188 9.58077 -7.96297
    endloop
  endfacet
  facet normal -1.04275e-07 -0.809016 0.587786
    outer loop
      vertex -10.3378 10.3114 -6.95738
      vertex -21.4772 9.28931 -8.36413
      vertex -10.1127 10.0281 -7.34731
    endloop
  endfacet
  facet normal 0 -0.809017 0.587785
    outer loop
      vertex -10.1127 10.0281 -7.34731
      vertex -21.4772 9.28931 -8.36413
      vertex -9.19717 9.28931 -8.36413
    endloop
  endfacet
  facet normal 0 -0.809017 0.587786
    outer loop
      vertex 21.4772 9.28931 -8.36413
      vertex 9.84898 9.81526 -7.64022
      vertex 9.19717 9.28931 -8.36413
    endloop
  endfacet
  facet normal 3.61669e-08 -0.809016 0.587786
    outer loop
      vertex 9.84898 9.81526 -7.64022
      vertex 21.4772 9.28931 -8.36413
      vertex 21.5113 9.52865 -8.03471
    endloop
  endfacet
  facet normal 3.64177e-09 -0.913545 0.406737
    outer loop
      vertex 21.4394 10.8253 -6.25
      vertex 11.572 11.5536 -4.61435
      vertex 11.4193 11.3444 -5.08421
    endloop
  endfacet
  facet normal -4.2482e-08 -0.913546 0.406737
    outer loop
      vertex 11.572 11.5536 -4.61435
      vertex 21.4394 10.8253 -6.25
      vertex 21.4306 11.7212 -4.23778
    endloop
  endfacet
  facet normal -4.45784e-07 -0.913546 0.406737
    outer loop
      vertex 21.4306 11.7212 -4.23778
      vertex 21.4394 10.8253 -6.25
      vertex 21.5615 11.2938 -5.19779
    endloop
  endfacet
  facet normal -1.81375e-08 -0.913545 0.406737
    outer loop
      vertex 21.4394 10.8253 -6.25
      vertex 11.4193 11.3444 -5.08421
      vertex 10.7462 10.8253 -6.25
    endloop
  endfacet
  facet normal 8.82484e-08 -0.913546 0.406735
    outer loop
      vertex -21.4394 10.8253 -6.25
      vertex -11.1887 11.1665 -5.48372
      vertex -21.4827 10.9914 -5.87692
    endloop
  endfacet
  facet normal -1.81375e-08 -0.913546 0.406737
    outer loop
      vertex -11.1887 11.1665 -5.48372
      vertex -21.4394 10.8253 -6.25
      vertex -10.7462 10.8253 -6.25
    endloop
  endfacet
  facet normal 4.45784e-07 -0.913546 -0.406737
    outer loop
      vertex -21.4306 11.7212 4.23778
      vertex -21.4394 10.8253 6.25
      vertex -21.5615 11.2938 5.19779
    endloop
  endfacet
  facet normal 1.81375e-08 -0.913545 -0.406737
    outer loop
      vertex -21.4394 10.8253 6.25
      vertex -11.4193 11.3444 5.08421
      vertex -10.7462 10.8253 6.25
    endloop
  endfacet
  facet normal -3.64177e-09 -0.913545 -0.406737
    outer loop
      vertex -21.4394 10.8253 6.25
      vertex -11.572 11.5536 4.61435
      vertex -11.4193 11.3444 5.08421
    endloop
  endfacet
  facet normal 4.2482e-08 -0.913546 -0.406737
    outer loop
      vertex -21.4394 10.8253 6.25
      vertex -21.4306 11.7212 4.23778
      vertex -11.572 11.5536 4.61435
    endloop
  endfacet
  facet normal 1.81375e-08 -0.913546 -0.406737
    outer loop
      vertex 11.1887 11.1665 5.48372
      vertex 21.4394 10.8253 6.25
      vertex 10.7462 10.8253 6.25
    endloop
  endfacet
  facet normal -8.82484e-08 -0.913546 -0.406735
    outer loop
      vertex 21.4394 10.8253 6.25
      vertex 11.1887 11.1665 5.48372
      vertex 21.4827 10.9914 5.87692
    endloop
  endfacet
  facet normal -4.3569e-08 0.104531 -0.994522
    outer loop
      vertex -21.4048 -1.97347 12.2926
      vertex -12.3096 -1.81131 12.3096
      vertex -21.3953 -2.18911 12.2699
    endloop
  endfacet
  facet normal 1.18183e-08 0.104528 -0.994522
    outer loop
      vertex -12.3096 -1.81131 12.3096
      vertex -21.4048 -1.97347 12.2926
      vertex -12.5 0 12.5
    endloop
  endfacet
  facet normal 0 0.104528 -0.994522
    outer loop
      vertex -12.5 0 12.5
      vertex -21.4048 -1.97347 12.2926
      vertex -21.4925 0 12.5
    endloop
  endfacet
  facet normal 0 0.104528 -0.994522
    outer loop
      vertex 21.4744 -0.406491 12.4573
      vertex 12.5 0 12.5
      vertex 21.4925 0 12.5
    endloop
  endfacet
  facet normal 8.67737e-09 0.104528 -0.994522
    outer loop
      vertex 12.5 0 12.5
      vertex 21.4744 -0.406491 12.4573
      vertex 12.4177 -0.783091 12.4177
    endloop
  endfacet
  facet normal 1.81375e-08 0.913545 -0.406737
    outer loop
      vertex -11.4193 -11.3444 5.08421
      vertex -21.4394 -10.8253 6.25
      vertex -10.7462 -10.8253 6.25
    endloop
  endfacet
  facet normal -3.64177e-09 0.913545 -0.406737
    outer loop
      vertex -11.572 -11.5536 4.61435
      vertex -21.4394 -10.8253 6.25
      vertex -11.4193 -11.3444 5.08421
    endloop
  endfacet
  facet normal 4.2482e-08 0.913546 -0.406737
    outer loop
      vertex -21.4306 -11.7212 4.23778
      vertex -21.4394 -10.8253 6.25
      vertex -11.572 -11.5536 4.61435
    endloop
  endfacet
  facet normal 4.45784e-07 0.913546 -0.406737
    outer loop
      vertex -21.4394 -10.8253 6.25
      vertex -21.4306 -11.7212 4.23778
      vertex -21.5615 -11.2938 5.19779
    endloop
  endfacet
  facet normal -8.82484e-08 0.913546 -0.406735
    outer loop
      vertex 11.1887 -11.1665 5.48372
      vertex 21.4394 -10.8253 6.25
      vertex 21.4827 -10.9914 5.87692
    endloop
  endfacet
  facet normal 1.81375e-08 0.913546 -0.406737
    outer loop
      vertex 21.4394 -10.8253 6.25
      vertex 11.1887 -11.1665 5.48372
      vertex 10.7462 -10.8253 6.25
    endloop
  endfacet
  facet normal 0 0.5 0.866025
    outer loop
      vertex -21.4855 -5.51743 -11.1692
      vertex -11.4193 -5.08421 -11.4193
      vertex -21.4752 -5.08421 -11.4193
    endloop
  endfacet
  facet normal 1.17677e-08 0.5 0.866025
    outer loop
      vertex -21.5265 -7.25083 -10.1684
      vertex -11.4193 -5.08421 -11.4193
      vertex -21.4855 -5.51743 -11.1692
    endloop
  endfacet
  facet normal 0 0.5 0.866025
    outer loop
      vertex -11.4193 -5.08421 -11.4193
      vertex -21.5141 -7.34731 -10.1127
      vertex -10.1127 -7.34731 -10.1127
    endloop
  endfacet
  facet normal -3.73796e-08 0.5 0.866025
    outer loop
      vertex -11.4193 -5.08421 -11.4193
      vertex -21.5265 -7.25083 -10.1684
      vertex -21.5141 -7.34731 -10.1127
    endloop
  endfacet
  facet normal -1.17677e-08 0.5 0.866025
    outer loop
      vertex 11.4193 -5.08421 -11.4193
      vertex 21.5265 -7.25083 -10.1684
      vertex 21.4855 -5.51743 -11.1692
    endloop
  endfacet
  facet normal -0 0.5 0.866025
    outer loop
      vertex 11.4193 -5.08421 -11.4193
      vertex 21.4855 -5.51743 -11.1692
      vertex 21.4752 -5.08421 -11.4193
    endloop
  endfacet
  facet normal 3.73796e-08 0.5 0.866025
    outer loop
      vertex 21.5265 -7.25083 -10.1684
      vertex 11.4193 -5.08421 -11.4193
      vertex 21.5141 -7.34731 -10.1127
    endloop
  endfacet
  facet normal 0 0.5 0.866025
    outer loop
      vertex 21.5141 -7.34731 -10.1127
      vertex 11.4193 -5.08421 -11.4193
      vertex 10.1127 -7.34731 -10.1127
    endloop
  endfacet
  facet normal 0 -0.978148 0.207912
    outer loop
      vertex -21.3767 12.1806 -2.48718
      vertex -12.3627 12.4315 -1.30661
      vertex -21.3743 12.4315 -1.30661
    endloop
  endfacet
  facet normal 4.93991e-09 -0.978148 0.207912
    outer loop
      vertex -21.3767 12.1806 -2.48718
      vertex -12.2268 12.1568 -2.5989
      vertex -12.3627 12.4315 -1.30661
    endloop
  endfacet
  facet normal 8.79501e-09 -0.978148 0.207912
    outer loop
      vertex -21.3795 11.8882 -3.86271
      vertex -12.2268 12.1568 -2.5989
      vertex -21.3767 12.1806 -2.48718
    endloop
  endfacet
  facet normal 0 -0.978148 0.207912
    outer loop
      vertex -12.2268 12.1568 -2.5989
      vertex -21.3795 11.8882 -3.86271
      vertex -11.8162 11.8882 -3.86271
    endloop
  endfacet
  facet normal 0 -0.978148 0.207912
    outer loop
      vertex 12.3627 12.4315 -1.30661
      vertex 21.3767 12.1806 -2.48718
      vertex 21.3743 12.4315 -1.30661
    endloop
  endfacet
  facet normal -4.93991e-09 -0.978148 0.207912
    outer loop
      vertex 12.2268 12.1568 -2.5989
      vertex 21.3767 12.1806 -2.48718
      vertex 12.3627 12.4315 -1.30661
    endloop
  endfacet
  facet normal 0 -0.978148 0.207912
    outer loop
      vertex 21.3795 11.8882 -3.86271
      vertex 12.2268 12.1568 -2.5989
      vertex 11.8162 11.8882 -3.86271
    endloop
  endfacet
  facet normal -8.79501e-09 -0.978148 0.207912
    outer loop
      vertex 12.2268 12.1568 -2.5989
      vertex 21.3795 11.8882 -3.86271
      vertex 21.3767 12.1806 -2.48718
    endloop
  endfacet
  facet normal 0 -0.913546 0.406737
    outer loop
      vertex -21.4827 10.9914 -5.87692
      vertex -11.8162 11.8882 -3.86271
      vertex -21.3795 11.8882 -3.86271
    endloop
  endfacet
  facet normal -7.04599e-07 -0.913546 0.406737
    outer loop
      vertex -21.4827 10.9914 -5.87692
      vertex -21.3795 11.8882 -3.86271
      vertex -21.5615 11.2938 -5.19779
    endloop
  endfacet
  facet normal 6.75376e-08 -0.913546 0.406736
    outer loop
      vertex -11.8162 11.8882 -3.86271
      vertex -21.4827 10.9914 -5.87692
      vertex -11.4193 11.3444 -5.08421
    endloop
  endfacet
  facet normal 2.5519e-08 -0.913545 0.406737
    outer loop
      vertex -11.4193 11.3444 -5.08421
      vertex -21.4827 10.9914 -5.87692
      vertex -11.1887 11.1665 -5.48372
    endloop
  endfacet
  facet normal 0 -0.913546 0.406736
    outer loop
      vertex 11.8162 11.8882 -3.86271
      vertex 21.4306 11.7212 -4.23778
      vertex 21.3795 11.8882 -3.86271
    endloop
  endfacet
  facet normal -1.53552e-08 -0.913546 0.406736
    outer loop
      vertex 21.4306 11.7212 -4.23778
      vertex 11.8162 11.8882 -3.86271
      vertex 11.572 11.5536 -4.61435
    endloop
  endfacet
  facet normal -2.5519e-08 -0.913545 -0.406737
    outer loop
      vertex 11.4193 11.3444 5.08421
      vertex 21.4827 10.9914 5.87692
      vertex 11.1887 11.1665 5.48372
    endloop
  endfacet
  facet normal -6.75376e-08 -0.913546 -0.406736
    outer loop
      vertex 11.8162 11.8882 3.86271
      vertex 21.4827 10.9914 5.87692
      vertex 11.4193 11.3444 5.08421
    endloop
  endfacet
  facet normal -0 -0.913546 -0.406737
    outer loop
      vertex 21.3795 11.8882 3.86271
      vertex 21.4827 10.9914 5.87692
      vertex 11.8162 11.8882 3.86271
    endloop
  endfacet
  facet normal 7.04599e-07 -0.913546 -0.406737
    outer loop
      vertex 21.4827 10.9914 5.87692
      vertex 21.3795 11.8882 3.86271
      vertex 21.5615 11.2938 5.19779
    endloop
  endfacet
  facet normal -0 -0.913546 -0.406736
    outer loop
      vertex -11.8162 11.8882 3.86271
      vertex -21.4306 11.7212 4.23778
      vertex -21.3795 11.8882 3.86271
    endloop
  endfacet
  facet normal 1.53552e-08 -0.913546 -0.406736
    outer loop
      vertex -21.4306 11.7212 4.23778
      vertex -11.8162 11.8882 3.86271
      vertex -11.572 11.5536 4.61435
    endloop
  endfacet
  facet normal 0 -0.978148 -0.207912
    outer loop
      vertex -21.3795 11.8882 3.86271
      vertex -12.2268 12.1568 2.5989
      vertex -11.8162 11.8882 3.86271
    endloop
  endfacet
  facet normal 8.79501e-09 -0.978148 -0.207912
    outer loop
      vertex -21.3767 12.1806 2.48718
      vertex -12.2268 12.1568 2.5989
      vertex -21.3795 11.8882 3.86271
    endloop
  endfacet
  facet normal -0 -0.978148 -0.207912
    outer loop
      vertex -12.3627 12.4315 1.30661
      vertex -21.3767 12.1806 2.48718
      vertex -21.3743 12.4315 1.30661
    endloop
  endfacet
  facet normal 4.93991e-09 -0.978148 -0.207912
    outer loop
      vertex -21.3767 12.1806 2.48718
      vertex -12.3627 12.4315 1.30661
      vertex -12.2268 12.1568 2.5989
    endloop
  endfacet
  facet normal 0 -0.978148 -0.207912
    outer loop
      vertex 12.2268 12.1568 2.5989
      vertex 21.3795 11.8882 3.86271
      vertex 11.8162 11.8882 3.86271
    endloop
  endfacet
  facet normal -8.79501e-09 -0.978148 -0.207912
    outer loop
      vertex 12.2268 12.1568 2.5989
      vertex 21.3767 12.1806 2.48718
      vertex 21.3795 11.8882 3.86271
    endloop
  endfacet
  facet normal -4.93991e-09 -0.978148 -0.207912
    outer loop
      vertex 12.3627 12.4315 1.30661
      vertex 21.3767 12.1806 2.48718
      vertex 12.2268 12.1568 2.5989
    endloop
  endfacet
  facet normal 0 -0.978148 -0.207912
    outer loop
      vertex 21.3767 12.1806 2.48718
      vertex 12.3627 12.4315 1.30661
      vertex 21.3743 12.4315 1.30661
    endloop
  endfacet
  facet normal 0 -0.104529 -0.994522
    outer loop
      vertex -12.3096 1.81131 12.3096
      vertex -21.377 2.5989 12.2268
      vertex -12.2268 2.5989 12.2268
    endloop
  endfacet
  facet normal 7.54111e-08 -0.104528 -0.994522
    outer loop
      vertex -21.377 2.5989 12.2268
      vertex -12.3096 1.81131 12.3096
      vertex -21.3953 2.18911 12.2699
    endloop
  endfacet
  facet normal -9.19611e-09 -0.104529 -0.994522
    outer loop
      vertex 12.4177 0.783091 12.4177
      vertex 21.4048 1.97347 12.2926
      vertex 21.4744 0.406491 12.4573
    endloop
  endfacet
  facet normal 6.56705e-08 -0.104529 -0.994522
    outer loop
      vertex 12.4177 0.783091 12.4177
      vertex 21.377 2.5989 12.2268
      vertex 21.4048 1.97347 12.2926
    endloop
  endfacet
  facet normal -0 -0.104529 -0.994522
    outer loop
      vertex 21.377 2.5989 12.2268
      vertex 12.4177 0.783091 12.4177
      vertex 12.2268 2.5989 12.2268
    endloop
  endfacet
  facet normal 0 -0.309017 -0.951057
    outer loop
      vertex -21.5403 4.57854 11.5836
      vertex -12.2268 2.5989 12.2268
      vertex -21.377 2.5989 12.2268
    endloop
  endfacet
  facet normal 7.23093e-08 -0.309017 -0.951057
    outer loop
      vertex -21.4752 5.08421 11.4193
      vertex -12.2268 2.5989 12.2268
      vertex -21.5403 4.57854 11.5836
    endloop
  endfacet
  facet normal 0 -0.309017 -0.951057
    outer loop
      vertex -12.2268 2.5989 12.2268
      vertex -21.4752 5.08421 11.4193
      vertex -11.4193 5.08421 11.4193
    endloop
  endfacet
  facet normal 0 -0.309017 -0.951057
    outer loop
      vertex 12.2268 2.5989 12.2268
      vertex 21.5403 4.57854 11.5836
      vertex 21.377 2.5989 12.2268
    endloop
  endfacet
  facet normal -7.23093e-08 -0.309017 -0.951057
    outer loop
      vertex 21.5403 4.57854 11.5836
      vertex 12.2268 2.5989 12.2268
      vertex 21.4752 5.08421 11.4193
    endloop
  endfacet
  facet normal -0 -0.309017 -0.951057
    outer loop
      vertex 21.4752 5.08421 11.4193
      vertex 12.2268 2.5989 12.2268
      vertex 11.4193 5.08421 11.4193
    endloop
  endfacet
  facet normal 0 -0.5 -0.866025
    outer loop
      vertex -21.4855 5.51743 11.1692
      vertex -11.4193 5.08421 11.4193
      vertex -21.4752 5.08421 11.4193
    endloop
  endfacet
  facet normal 1.17677e-08 -0.5 -0.866025
    outer loop
      vertex -21.5265 7.25083 10.1684
      vertex -11.4193 5.08421 11.4193
      vertex -21.4855 5.51743 11.1692
    endloop
  endfacet
  facet normal 0 -0.5 -0.866025
    outer loop
      vertex -11.4193 5.08421 11.4193
      vertex -21.5141 7.34731 10.1127
      vertex -10.1127 7.34731 10.1127
    endloop
  endfacet
  facet normal -3.73796e-08 -0.5 -0.866025
    outer loop
      vertex -11.4193 5.08421 11.4193
      vertex -21.5265 7.25083 10.1684
      vertex -21.5141 7.34731 10.1127
    endloop
  endfacet
  facet normal -1.17677e-08 -0.5 -0.866025
    outer loop
      vertex 11.4193 5.08421 11.4193
      vertex 21.5265 7.25083 10.1684
      vertex 21.4855 5.51743 11.1692
    endloop
  endfacet
  facet normal 0 -0.5 -0.866025
    outer loop
      vertex 11.4193 5.08421 11.4193
      vertex 21.4855 5.51743 11.1692
      vertex 21.4752 5.08421 11.4193
    endloop
  endfacet
  facet normal 3.73796e-08 -0.5 -0.866025
    outer loop
      vertex 21.5265 7.25083 10.1684
      vertex 11.4193 5.08421 11.4193
      vertex 21.5141 7.34731 10.1127
    endloop
  endfacet
  facet normal -0 -0.5 -0.866025
    outer loop
      vertex 21.5141 7.34731 10.1127
      vertex 11.4193 5.08421 11.4193
      vertex 10.1127 7.34731 10.1127
    endloop
  endfacet
  facet normal 0 -0.809017 -0.587785
    outer loop
      vertex 10.1127 10.0281 7.34731
      vertex 21.4772 9.28931 8.36413
      vertex 9.19717 9.28931 8.36413
    endloop
  endfacet
  facet normal 1.04275e-07 -0.809016 -0.587786
    outer loop
      vertex 10.3378 10.3114 6.95738
      vertex 21.4772 9.28931 8.36413
      vertex 10.1127 10.0281 7.34731
    endloop
  endfacet
  facet normal -5.59099e-08 -0.809017 -0.587785
    outer loop
      vertex 21.4772 9.28931 8.36413
      vertex 10.3378 10.3114 6.95738
      vertex 21.5188 9.58077 7.96297
    endloop
  endfacet
  facet normal -9.12501e-09 -0.809017 -0.587786
    outer loop
      vertex 21.5188 9.58077 7.96297
      vertex 10.3378 10.3114 6.95738
      vertex 21.4547 10.5846 6.58134
    endloop
  endfacet
  facet normal -3.61669e-08 -0.809016 -0.587786
    outer loop
      vertex -9.84898 9.81526 7.64022
      vertex -21.4772 9.28931 8.36413
      vertex -21.5113 9.52865 8.03471
    endloop
  endfacet
  facet normal 0 -0.809017 -0.587786
    outer loop
      vertex -21.4772 9.28931 8.36413
      vertex -9.84898 9.81526 7.64022
      vertex -9.19717 9.28931 8.36413
    endloop
  endfacet
  facet normal 0 0.309017 -0.951057
    outer loop
      vertex -21.4752 -5.08421 11.4193
      vertex -12.2268 -2.5989 12.2268
      vertex -11.4193 -5.08421 11.4193
    endloop
  endfacet
  facet normal 7.23093e-08 0.309017 -0.951057
    outer loop
      vertex -12.2268 -2.5989 12.2268
      vertex -21.4752 -5.08421 11.4193
      vertex -21.5403 -4.57854 11.5836
    endloop
  endfacet
  facet normal 0 0.309017 -0.951057
    outer loop
      vertex -12.2268 -2.5989 12.2268
      vertex -21.5403 -4.57854 11.5836
      vertex -21.377 -2.5989 12.2268
    endloop
  endfacet
  facet normal -7.23093e-08 0.309017 -0.951057
    outer loop
      vertex 12.2268 -2.5989 12.2268
      vertex 21.5403 -4.57854 11.5836
      vertex 21.4752 -5.08421 11.4193
    endloop
  endfacet
  facet normal 0 0.309017 -0.951057
    outer loop
      vertex 21.5403 -4.57854 11.5836
      vertex 12.2268 -2.5989 12.2268
      vertex 21.377 -2.5989 12.2268
    endloop
  endfacet
  facet normal 0 0.309017 -0.951057
    outer loop
      vertex 12.2268 -2.5989 12.2268
      vertex 21.4752 -5.08421 11.4193
      vertex 11.4193 -5.08421 11.4193
    endloop
  endfacet
  facet normal 0 0.104529 -0.994522
    outer loop
      vertex -21.377 -2.5989 12.2268
      vertex -12.3096 -1.81131 12.3096
      vertex -12.2268 -2.5989 12.2268
    endloop
  endfacet
  facet normal 7.54111e-08 0.104528 -0.994522
    outer loop
      vertex -12.3096 -1.81131 12.3096
      vertex -21.377 -2.5989 12.2268
      vertex -21.3953 -2.18911 12.2699
    endloop
  endfacet
  facet normal -9.19611e-09 0.104529 -0.994522
    outer loop
      vertex 21.4048 -1.97347 12.2926
      vertex 12.4177 -0.783091 12.4177
      vertex 21.4744 -0.406491 12.4573
    endloop
  endfacet
  facet normal 6.56705e-08 0.104529 -0.994522
    outer loop
      vertex 21.377 -2.5989 12.2268
      vertex 12.4177 -0.783091 12.4177
      vertex 21.4048 -1.97347 12.2926
    endloop
  endfacet
  facet normal 0 0.104529 -0.994522
    outer loop
      vertex 12.4177 -0.783091 12.4177
      vertex 21.377 -2.5989 12.2268
      vertex 12.2268 -2.5989 12.2268
    endloop
  endfacet
  facet normal 4.29974e-08 0.809018 -0.587784
    outer loop
      vertex -10.1127 -10.0281 7.34731
      vertex -21.5113 -9.52865 8.03471
      vertex -9.84898 -9.81526 7.64022
    endloop
  endfacet
  facet normal -5.65188e-08 0.809017 -0.587786
    outer loop
      vertex -10.7462 -10.8253 6.25
      vertex -21.5113 -9.52865 8.03471
      vertex -10.1127 -10.0281 7.34731
    endloop
  endfacet
  facet normal 1.57207e-06 0.809021 -0.587779
    outer loop
      vertex -21.5113 -9.52865 8.03471
      vertex -10.7462 -10.8253 6.25
      vertex -21.5188 -9.58077 7.96296
    endloop
  endfacet
  facet normal 4.79067e-08 0.809017 -0.587785
    outer loop
      vertex -21.5188 -9.58077 7.96296
      vertex -10.7462 -10.8253 6.25
      vertex -21.444 -10.7525 6.35017
    endloop
  endfacet
  facet normal 2.62111e-08 0.809016 -0.587787
    outer loop
      vertex -21.444 -10.7525 6.35017
      vertex -10.7462 -10.8253 6.25
      vertex -21.4394 -10.8253 6.25
    endloop
  endfacet
  facet normal 6.8015e-08 0.809016 -0.587787
    outer loop
      vertex 10.7462 -10.8253 6.25
      vertex 21.4547 -10.5846 6.58134
      vertex 21.444 -10.7525 6.35017
    endloop
  endfacet
  facet normal 2.62109e-08 0.809018 -0.587784
    outer loop
      vertex 10.7462 -10.8253 6.25
      vertex 21.444 -10.7525 6.35017
      vertex 21.4394 -10.8253 6.25
    endloop
  endfacet
  facet normal 4.77206e-09 0.809017 -0.587785
    outer loop
      vertex 21.4547 -10.5846 6.58134
      vertex 10.7462 -10.8253 6.25
      vertex 10.3378 -10.3114 6.95738
    endloop
  endfacet
  facet normal 0 1 -0
    outer loop
      vertex -12.5 -12.4315 0
      vertex -21.3743 -12.4315 1.30661
      vertex -12.3627 -12.4315 1.30661
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex -12.5 -12.4315 0
      vertex -21.532 -12.4315 0
      vertex -21.3743 -12.4315 1.30661
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex -21.3743 -12.4315 -1.30661
      vertex -12.5 -12.4315 0
      vertex -12.3627 -12.4315 -1.30661
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex -12.5 -12.4315 0
      vertex -21.3743 -12.4315 -1.30661
      vertex -21.532 -12.4315 0
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex 12.5 -12.4315 0
      vertex 21.3743 -12.4315 1.30661
      vertex 21.532 -12.4315 0
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex 21.3743 -12.4315 1.30661
      vertex 12.5 -12.4315 0
      vertex 12.3627 -12.4315 1.30661
    endloop
  endfacet
  facet normal 0 1 -0
    outer loop
      vertex 21.3743 -12.4315 -1.30661
      vertex 12.5 -12.4315 0
      vertex 21.532 -12.4315 0
    endloop
  endfacet
  facet normal 0 1 0
    outer loop
      vertex 12.5 -12.4315 0
      vertex 21.3743 -12.4315 -1.30661
      vertex 12.3627 -12.4315 -1.30661
    endloop
  endfacet
  facet normal 0 0.978148 0.207912
    outer loop
      vertex -12.3627 -12.4315 -1.30661
      vertex -21.3767 -12.1806 -2.48718
      vertex -21.3743 -12.4315 -1.30661
    endloop
  endfacet
  facet normal 4.93991e-09 0.978148 0.207912
    outer loop
      vertex -12.2268 -12.1568 -2.5989
      vertex -21.3767 -12.1806 -2.48718
      vertex -12.3627 -12.4315 -1.30661
    endloop
  endfacet
  facet normal -0 0.978148 0.207912
    outer loop
      vertex -21.3795 -11.8882 -3.86271
      vertex -12.2268 -12.1568 -2.5989
      vertex -11.8162 -11.8882 -3.86271
    endloop
  endfacet
  facet normal 8.79501e-09 0.978148 0.207912
    outer loop
      vertex -12.2268 -12.1568 -2.5989
      vertex -21.3795 -11.8882 -3.86271
      vertex -21.3767 -12.1806 -2.48718
    endloop
  endfacet
  facet normal 0 0.978148 0.207912
    outer loop
      vertex 21.3767 -12.1806 -2.48718
      vertex 12.3627 -12.4315 -1.30661
      vertex 21.3743 -12.4315 -1.30661
    endloop
  endfacet
  facet normal -4.93991e-09 0.978148 0.207912
    outer loop
      vertex 21.3767 -12.1806 -2.48718
      vertex 12.2268 -12.1568 -2.5989
      vertex 12.3627 -12.4315 -1.30661
    endloop
  endfacet
  facet normal -8.79501e-09 0.978148 0.207912
    outer loop
      vertex 21.3795 -11.8882 -3.86271
      vertex 12.2268 -12.1568 -2.5989
      vertex 21.3767 -12.1806 -2.48718
    endloop
  endfacet
  facet normal 0 0.978148 0.207912
    outer loop
      vertex 12.2268 -12.1568 -2.5989
      vertex 21.3795 -11.8882 -3.86271
      vertex 11.8162 -11.8882 -3.86271
    endloop
  endfacet
  facet normal 8.06309e-07 0.669131 0.743145
    outer loop
      vertex -10.1127 -7.34731 -10.1127
      vertex -9.19717 -9.28931 -8.36413
      vertex -8.80248 -8.80248 -8.80248
    endloop
  endfacet
  facet normal 0 0.669131 0.743145
    outer loop
      vertex -10.1127 -7.34731 -10.1127
      vertex -21.4772 -9.28931 -8.36413
      vertex -9.19717 -9.28931 -8.36413
    endloop
  endfacet
  facet normal 5.2907e-09 0.669131 0.743145
    outer loop
      vertex -10.1127 -7.34731 -10.1127
      vertex -21.4936 -8.42289 -9.14426
      vertex -21.4772 -9.28931 -8.36413
    endloop
  endfacet
  facet normal 0 0.669131 0.743145
    outer loop
      vertex -21.4936 -8.42289 -9.14426
      vertex -10.1127 -7.34731 -10.1127
      vertex -21.5141 -7.34731 -10.1127
    endloop
  endfacet
  facet normal -0 0.669131 0.743145
    outer loop
      vertex 10.1127 -7.34731 -10.1127
      vertex 21.4936 -8.42289 -9.14426
      vertex 21.5141 -7.34731 -10.1127
    endloop
  endfacet
  facet normal -5.2907e-09 0.669131 0.743145
    outer loop
      vertex 10.1127 -7.34731 -10.1127
      vertex 21.4772 -9.28931 -8.36413
      vertex 21.4936 -8.42289 -9.14426
    endloop
  endfacet
  facet normal 0 0.669131 0.743145
    outer loop
      vertex 10.1127 -7.34731 -10.1127
      vertex 9.19717 -9.28931 -8.36413
      vertex 21.4772 -9.28931 -8.36413
    endloop
  endfacet
  facet normal -8.06309e-07 0.669131 0.743145
    outer loop
      vertex 9.19717 -9.28931 -8.36413
      vertex 10.1127 -7.34731 -10.1127
      vertex 8.80248 -8.80248 -8.80248
    endloop
  endfacet
  facet normal 1.57207e-06 -0.809021 -0.587779
    outer loop
      vertex -10.7462 10.8253 6.25
      vertex -21.5113 9.52865 8.03471
      vertex -21.5188 9.58077 7.96296
    endloop
  endfacet
  facet normal 4.29974e-08 -0.809018 -0.587784
    outer loop
      vertex -21.5113 9.52865 8.03471
      vertex -10.1127 10.0281 7.34731
      vertex -9.84898 9.81526 7.64022
    endloop
  endfacet
  facet normal 4.79067e-08 -0.809017 -0.587785
    outer loop
      vertex -10.7462 10.8253 6.25
      vertex -21.5188 9.58077 7.96296
      vertex -21.444 10.7525 6.35017
    endloop
  endfacet
  facet normal -5.65188e-08 -0.809017 -0.587786
    outer loop
      vertex -21.5113 9.52865 8.03471
      vertex -10.7462 10.8253 6.25
      vertex -10.1127 10.0281 7.34731
    endloop
  endfacet
  facet normal 2.62111e-08 -0.809016 -0.587787
    outer loop
      vertex -10.7462 10.8253 6.25
      vertex -21.444 10.7525 6.35017
      vertex -21.4394 10.8253 6.25
    endloop
  endfacet
  facet normal 4.77206e-09 -0.809017 -0.587785
    outer loop
      vertex 10.7462 10.8253 6.25
      vertex 21.4547 10.5846 6.58134
      vertex 10.3378 10.3114 6.95738
    endloop
  endfacet
  facet normal 6.8015e-08 -0.809016 -0.587787
    outer loop
      vertex 21.4547 10.5846 6.58134
      vertex 10.7462 10.8253 6.25
      vertex 21.444 10.7525 6.35017
    endloop
  endfacet
  facet normal 2.62109e-08 -0.809018 -0.587784
    outer loop
      vertex 21.444 10.7525 6.35017
      vertex 10.7462 10.8253 6.25
      vertex 21.4394 10.8253 6.25
    endloop
  endfacet
  facet normal 1.18183e-08 -0.104528 -0.994522
    outer loop
      vertex -21.4048 1.97347 12.2926
      vertex -12.3096 1.81131 12.3096
      vertex -12.5 0 12.5
    endloop
  endfacet
  facet normal -4.3569e-08 -0.104531 -0.994522
    outer loop
      vertex -12.3096 1.81131 12.3096
      vertex -21.4048 1.97347 12.2926
      vertex -21.3953 2.18911 12.2699
    endloop
  endfacet
  facet normal 0 -0.104528 -0.994522
    outer loop
      vertex -21.4048 1.97347 12.2926
      vertex -12.5 0 12.5
      vertex -21.4925 0 12.5
    endloop
  endfacet
  facet normal 0 -0.104528 -0.994522
    outer loop
      vertex 12.5 0 12.5
      vertex 21.4744 0.406491 12.4573
      vertex 21.4925 0 12.5
    endloop
  endfacet
  facet normal 8.67737e-09 -0.104528 -0.994522
    outer loop
      vertex 21.4744 0.406491 12.4573
      vertex 12.5 0 12.5
      vertex 12.4177 0.783091 12.4177
    endloop
  endfacet
  facet normal 0 0.5 -0.866025
    outer loop
      vertex -21.5141 -7.34731 10.1127
      vertex -11.4193 -5.08421 11.4193
      vertex -10.1127 -7.34731 10.1127
    endloop
  endfacet
  facet normal 0 0.5 -0.866025
    outer loop
      vertex -11.4193 -5.08421 11.4193
      vertex -21.4855 -5.51743 11.1692
      vertex -21.4752 -5.08421 11.4193
    endloop
  endfacet
  facet normal -3.73796e-08 0.5 -0.866025
    outer loop
      vertex -21.5265 -7.25083 10.1684
      vertex -11.4193 -5.08421 11.4193
      vertex -21.5141 -7.34731 10.1127
    endloop
  endfacet
  facet normal 1.17677e-08 0.5 -0.866025
    outer loop
      vertex -11.4193 -5.08421 11.4193
      vertex -21.5265 -7.25083 10.1684
      vertex -21.4855 -5.51743 11.1692
    endloop
  endfacet
  facet normal 3.73796e-08 0.5 -0.866025
    outer loop
      vertex 11.4193 -5.08421 11.4193
      vertex 21.5265 -7.25083 10.1684
      vertex 21.5141 -7.34731 10.1127
    endloop
  endfacet
  facet normal -1.17677e-08 0.5 -0.866025
    outer loop
      vertex 21.5265 -7.25083 10.1684
      vertex 11.4193 -5.08421 11.4193
      vertex 21.4855 -5.51743 11.1692
    endloop
  endfacet
  facet normal 0 0.5 -0.866025
    outer loop
      vertex 21.4855 -5.51743 11.1692
      vertex 11.4193 -5.08421 11.4193
      vertex 21.4752 -5.08421 11.4193
    endloop
  endfacet
  facet normal 0 0.5 -0.866025
    outer loop
      vertex 11.4193 -5.08421 11.4193
      vertex 21.5141 -7.34731 10.1127
      vertex 10.1127 -7.34731 10.1127
    endloop
  endfacet
  facet normal 7.04599e-07 0.913546 -0.406737
    outer loop
      vertex 21.3795 -11.8882 3.86271
      vertex 21.4827 -10.9914 5.87692
      vertex 21.5615 -11.2938 5.19779
    endloop
  endfacet
  facet normal -2.5519e-08 0.913545 -0.406737
    outer loop
      vertex 21.4827 -10.9914 5.87692
      vertex 11.4193 -11.3444 5.08421
      vertex 11.1887 -11.1665 5.48372
    endloop
  endfacet
  facet normal 0 0.913546 -0.406737
    outer loop
      vertex 21.4827 -10.9914 5.87692
      vertex 21.3795 -11.8882 3.86271
      vertex 11.8162 -11.8882 3.86271
    endloop
  endfacet
  facet normal -6.75376e-08 0.913546 -0.406736
    outer loop
      vertex 21.4827 -10.9914 5.87692
      vertex 11.8162 -11.8882 3.86271
      vertex 11.4193 -11.3444 5.08421
    endloop
  endfacet
  facet normal 1.53552e-08 0.913546 -0.406736
    outer loop
      vertex -11.8162 -11.8882 3.86271
      vertex -21.4306 -11.7212 4.23778
      vertex -11.572 -11.5536 4.61435
    endloop
  endfacet
  facet normal 0 0.913546 -0.406736
    outer loop
      vertex -21.4306 -11.7212 4.23778
      vertex -11.8162 -11.8882 3.86271
      vertex -21.3795 -11.8882 3.86271
    endloop
  endfacet
  facet normal 6.75376e-08 0.913546 0.406736
    outer loop
      vertex -21.4827 -10.9914 -5.87692
      vertex -11.8162 -11.8882 -3.86271
      vertex -11.4193 -11.3444 -5.08421
    endloop
  endfacet
  facet normal 0 0.913546 0.406737
    outer loop
      vertex -11.8162 -11.8882 -3.86271
      vertex -21.4827 -10.9914 -5.87692
      vertex -21.3795 -11.8882 -3.86271
    endloop
  endfacet
  facet normal 2.5519e-08 0.913545 0.406737
    outer loop
      vertex -21.4827 -10.9914 -5.87692
      vertex -11.4193 -11.3444 -5.08421
      vertex -11.1887 -11.1665 -5.48372
    endloop
  endfacet
  facet normal -7.04599e-07 0.913546 0.406737
    outer loop
      vertex -21.3795 -11.8882 -3.86271
      vertex -21.4827 -10.9914 -5.87692
      vertex -21.5615 -11.2938 -5.19779
    endloop
  endfacet
  facet normal 0 0.913546 0.406736
    outer loop
      vertex 21.4306 -11.7212 -4.23778
      vertex 11.8162 -11.8882 -3.86271
      vertex 21.3795 -11.8882 -3.86271
    endloop
  endfacet
  facet normal -1.53552e-08 0.913546 0.406736
    outer loop
      vertex 11.8162 -11.8882 -3.86271
      vertex 21.4306 -11.7212 -4.23778
      vertex 11.572 -11.5536 -4.61435
    endloop
  endfacet
  facet normal -1.04275e-07 0.809016 0.587786
    outer loop
      vertex -21.4772 -9.28931 -8.36413
      vertex -10.3378 -10.3114 -6.95738
      vertex -10.1127 -10.0281 -7.34731
    endloop
  endfacet
  facet normal 9.12501e-09 0.809017 0.587786
    outer loop
      vertex -10.3378 -10.3114 -6.95738
      vertex -21.5188 -9.58077 -7.96297
      vertex -21.4547 -10.5846 -6.58134
    endloop
  endfacet
  facet normal -0 0.809017 0.587785
    outer loop
      vertex -21.4772 -9.28931 -8.36413
      vertex -10.1127 -10.0281 -7.34731
      vertex -9.19717 -9.28931 -8.36413
    endloop
  endfacet
  facet normal 5.59099e-08 0.809017 0.587785
    outer loop
      vertex -10.3378 -10.3114 -6.95738
      vertex -21.4772 -9.28931 -8.36413
      vertex -21.5188 -9.58077 -7.96297
    endloop
  endfacet
  facet normal 3.61669e-08 0.809016 0.587786
    outer loop
      vertex 21.4772 -9.28931 -8.36413
      vertex 9.84898 -9.81526 -7.64022
      vertex 21.5113 -9.52865 -8.03471
    endloop
  endfacet
  facet normal 0 0.809017 0.587786
    outer loop
      vertex 9.84898 -9.81526 -7.64022
      vertex 21.4772 -9.28931 -8.36413
      vertex 9.19717 -9.28931 -8.36413
    endloop
  endfacet
endsolid OpenSCAD_Model
)";

