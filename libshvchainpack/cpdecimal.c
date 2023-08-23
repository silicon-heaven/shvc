#include <shv/cp.h>
#include <math.h>

double cpdectod(struct cpdecimal v) {
	double res = v.mantisa;
	if (v.exponent >= 0) {
		for (long i = 0; i < v.exponent; i++)
			res *= 10;
	} else {
		for (long i = v.exponent; i < 0; i++)
			res /= 10;
	}
	return res;
}

#if 0
struct cpdecimal cpdtodec(double v) {
	// TODO
	double i;
	double d = modf(v, &i);
	printf("That would be %f and %f\n", i, d);
	int exp;
	double s = frexp(v, &exp);
	printf("frexp %d for %lf\n", exp, s);
	return (struct cpdecimal) {.mantisa = i, .exponent = d};
}
#endif
