// SPWM Look up table generator for project AlexMod.
// Select the suitable divider for your BLDC motor.
// Mine is 1.7
// Coded by TinLethax 2022/11/20 +7

#include <stdio.h>
#include <stdint.h>
#include <string.h>
// Sine with 3rd Harmonic injected LUT
// sin(2piF) + 0.33sin(2piF * 2)
// sin(2*pi*t/T) + 0.33*sin(6*pi*t/T)
const uint8_t lut[256] = {
127,133,139,146,152,158,164,169,175,181,186,191,196,
201,206,210,214,218,222,225,229,231,234,237,239,241,
242,243,245,245,246,246,246,246,246,246,245,244,243,
242,241,239,238,236,235,233,232,230,228,227,225,223,
222,221,219,218,217,216,215,214,213,213,212,212,212,
212,212,213,213,214,215,216,217,218,219,221,222,223,
225,227,228,230,232,233,235,236,238,239,241,242,243,
244,245,246,246,246,246,246,246,245,245,243,242,241,
239,237,234,231,229,225,222,218,214,210,206,201,196,
191,186,181,175,169,164,158,152,146,139,133,127,121,
115,108,102, 96, 90, 85, 79, 73, 68, 63, 58, 53, 48,
 44, 40, 36, 32, 29, 25, 23, 20, 17, 15, 13, 12, 11,
  9,  9,  8,  8,  8,  8,  8,  8,  9, 10, 11, 12, 13,
 15, 16, 18, 19, 21, 22, 24, 26, 27, 29, 31, 32, 33,
 35, 36, 37, 38, 39, 40, 41, 41, 42, 42, 42, 42, 42,
 41, 41, 40, 39, 38, 37, 36, 35, 33, 32, 31, 29, 27,
 26, 24, 22, 21, 19, 18, 16, 15, 13, 12, 11, 10,  9,
  8,  8,  8,  8,  8,  8,  9,  9, 11, 12, 13, 15, 17,
 20, 23, 25, 29, 32, 36, 40, 44, 48, 53, 58, 63, 68,
 73, 79, 85, 90, 96,102,108,115,121 };

float motorconst = 0;
uint8_t cnt = 0;
int main(){
	FILE *fplut;// for LUT .h file.

	printf("SPWM LUT generator for project AlexMod\n");
	printf("Please enter your BLDC motor constant E.G. 1.7 :");
	scanf("%f", &motorconst);

	// Create and open lut file
	fplut = fopen("lut.h", "w");
	fprintf(fplut, "// THSPWM LUT for AlexMod\n");
	fprintf(fplut, "const uint8_t PROGMEM lut[256] = {\n");

	for(int i = 0; i < 16; i++){
		fprintf(fplut," ");
		for(int j = 0; j < 16; j++){
			fprintf(fplut, " %d,", (uint8_t)(lut[cnt++]*1.0 / motorconst));
		}
		fprintf(fplut, "\n");
	}

	fprintf(fplut, "};");

	printf("DONE!");

	fclose(fplut);
	return 0;
}
