#include <shv/cp.h>
#include <math.h>

void cpdecnorm(struct cpdecimal *v) {
	if (v->mantisa == 0) {
		v->exponent = 0;
		return;
	}
	while (v->mantisa % 10 == 0) {
		v->mantisa /= 10;
		v->exponent++;
	}
}

double cpdectod(const struct cpdecimal v) {
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
