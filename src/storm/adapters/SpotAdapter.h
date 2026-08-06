#pragma once
#include "storm-config.h"

#ifdef STORM_HAVE_SPOT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "spot/tl/formula.hh"
#include "spot/tl/parse.hh"
#include "spot/twaalgos/dot.hh"
#include "spot/twaalgos/hoa.hh"
#include "spot/twaalgos/totgba.hh"
#include "spot/twaalgos/translate.hh"

#pragma GCC diagnostic pop
#endif
