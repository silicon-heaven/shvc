#include <shv/cp.h>

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

bool cpdecexp(struct cpdecimal *v, int exponent) {
	int neg = v->exponent < exponent ? 1 : -1;
	while (v->exponent != exponent && v->mantisa) {
		long long int mantisa;
		if (neg < 0) {
			if (__builtin_smulll_overflow(v->mantisa, 10, &mantisa))
				return false;
		} else
			mantisa = v->mantisa / 10;
		v->mantisa = mantisa;
		v->exponent += neg;
	}
	return true;
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
