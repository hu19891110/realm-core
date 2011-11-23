#include "Column.h"
#include <UnitTest++.h>
#include <vector>
#include <algorithm>
#include "../testsettings.h"
#include "verified_integer.h"

// Support functions for monkey test

static uint64_t rand2(int bitwidth = 64) {
	uint64_t i = (int64_t)rand() * (int64_t)rand() * (int64_t)rand() * (int64_t)rand() * (int64_t)rand();
	if(bitwidth < 64) {
		const uint64_t mask = ((1ULL << bitwidth) - 1ULL);
		i &= mask;
	}
	return i;
}


TEST(Column_monkeytest2) {
	const uint64_t ITER_PER_BITWIDTH = 1 * 1000;
	const uint64_t SEED = 123;

	VerifiedInteger a;
	Column res;

	srand(SEED);
	size_t current_bitwidth = 0;
	unsigned int trend = 5;

	for(current_bitwidth = 0; current_bitwidth < 65; current_bitwidth++) {
		for(size_t iter = 0; iter < ITER_PER_BITWIDTH; iter++) {

//			if(rand() % 10 == 0) printf("Input bitwidth around ~%d, , a.Size()=%d\n", (int)current_bitwidth, (int)a.Size());

			if (!(rand2() % (ITER_PER_BITWIDTH / 100))) {
				trend = (unsigned int)rand2() % 10;
				a.Find(rand2(current_bitwidth));
				a.FindAll(res, rand2(current_bitwidth));
			}

			if (rand2() % 10 > trend && a.Size() < ITER_PER_BITWIDTH / 100) {
				uint64_t l = rand2(current_bitwidth);
				if(rand2() % 2 == 0) {
					// Insert
					const size_t pos = rand2() % (a.Size() + 1);
					a.Insert(pos, l);
				}
				else {
					// Add
					a.Add(l);
				}
			}
			else if(a.Size() > 0) {
				// Delete
				const size_t i = rand2() % a.Size();
				a.Delete(i);
			}
		}
	}

	// Cleanup
	a.Destroy();
	res.Destroy();
}
