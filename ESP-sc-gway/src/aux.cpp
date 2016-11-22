#include <Arduino.h>


// ----------------------------------------------------------------------------
// Convert a float to string for printing
// f is value to convert
// p is precision in decimal digits
// val is character array for results
// ----------------------------------------------------------------------------
void ftoa(float f, char *val, int p) {
	int j=1;
	int ival, fval;
	char b[6];

	for (int i=0; i< p; i++) { j= j*10; }

	ival = (int) f;								// Make integer part
	fval = (int) ((f- ival)*j);					// Make fraction. Has same sign as integer part
	if (fval<0) fval = -fval;					// So if it is negative make fraction positive again.
												// sprintf does NOT fit in memory
	strcat(val,itoa(ival,b,10));
	strcat(val,".");							// decimal point

	itoa(fval,b,10);
	for (int i=0; i<(p-strlen(b)); i++) strcat(val,"0");
	// Fraction can be anything from 0 to 10^p , so can have less digits
	strcat(val,b);
}
