#include "storm/adapters/MathsatExpressionAdapter.h"

#ifdef STORM_HAVE_MATHSAT
bool operator==(msat_decl decl1, msat_decl decl2) {
    return decl1.repr == decl2.repr;
}
#endif
