#include <shv/cp.h>

void cpdecnorm(struct cpdecimal *v) {
	if (v->mantissa == 0) {
		v->exponent = 0;
		return;
	}
	while (v->mantissa % 10 == 0) {
		v->mantissa /= 10;
		v->exponent++;
	}
}

bool cpdecexp(struct cpdecimal *v, int exponent) {
	int neg = v->exponent < exponent ? 1 : -1;
	while (v->exponent != exponent && v->mantissa) {
		long long int mantissa;
		if (neg < 0) {
			if (__builtin_smulll_overflow(v->mantissa, 10, &mantissa))
				return false;
		} else
			mantissa = v->mantissa / 10;
		v->mantissa = mantissa;
		v->exponent += neg;
	}
	return true;
}

double cpdectod(const struct cpdecimal v) {
	double res = v.mantissa;
	if (v.exponent >= 0) {
		for (long i = 0; i < v.exponent; i++)
			res *= 10;
	} else {
		for (long i = v.exponent; i < 0; i++)
			res /= 10;
	}
	return res;
}
