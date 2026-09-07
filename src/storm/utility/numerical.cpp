#include "numerical.h"

#include <boost/math/constants/constants.hpp>
#include <cmath>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/PrecisionExceededException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace utility {
namespace numerical {

template<typename ValueType>
FoxGlynnResult<ValueType>::FoxGlynnResult() : left(0), right(0), totalWeight(storm::utility::zero<ValueType>()) {
    // Intentionally left empty.
}

/*!
 * The following implementation of Fox and Glynn's algorithm is taken from David Jansen's patched version
 * in MRMC, which is based on his paper:
 *
 * https://pms.cs.ru.nl/iris-diglib/src/getContent.php?id=2011-Jansen-UnderstandingFoxGlynn
 *
 * We have only adapted the code to match more of C++'s and our coding guidelines.
 */

template<typename ValueType>
FoxGlynnResult<ValueType> foxGlynnFinder(ValueType lambda, ValueType epsilon) {
    ValueType tau = std::numeric_limits<ValueType>::min();
    ValueType omega = std::numeric_limits<ValueType>::max();
    ValueType const sqrt_2_pi = boost::math::constants::root_two_pi<ValueType>();
    ValueType const log10_e = std::log10(boost::math::constants::e<ValueType>());

    uint64_t m = static_cast<uint64_t>(lambda);

    int64_t left = 0;
    int64_t right = 0;

    // tau is only used in underflow checks, which we are going to do in the logarithm domain.
    tau = log(tau);

    // In error bound comparisons, we always compare with epsilon*sqrt_2_pi.
    epsilon *= sqrt_2_pi;

    // Compute left truncation point.
    if (m < 25) {
        // For lambda below 25 the exponential can be smaller than tau. If that is the case we expect
        // underflows and warn the user.
        if (-lambda <= tau) {
            STORM_LOG_WARN("Fox-Glynn: 0 < lambda < 25, underflow near Poi(" << lambda << ", 0) = " << std::exp(-lambda) << ". The results are unreliable.");
        }

        // Zero is used as left truncation point for lambda <= 25.
        left = 0;
    } else {
        // Compute the left truncation point for lambda >= 25 (for lambda < 25 we use zero as left truncation point).

        ValueType const bl = (1 + 1 / lambda) * std::exp((1 / lambda) * 0.125);
        ValueType const sqrt_lambda = std::sqrt(lambda);
        int64_t k;

        // Start looking for the left truncation point:
        // * start search at k=4 (taken from original Fox-Glynn paper)
        // * increase the left truncation point until we fulfil the error condition

        for (k = 4;; ++k) {
            ValueType max_err;

            left = m - static_cast<int64_t>(std::ceil(k * sqrt_lambda + 0.5));

            // For small lambda the above calculation can yield negative truncation points, crop them here.
            if (left <= 0) {
                left = 0;
                break;
            }

            // Note that Propositions 2-4 in Fox--Glynn mix up notation: they write Phi where they mean
            // 1 - Phi. (In Corollaries 1 and 2, phi is used correctly again.)
            max_err = bl * exp(-0.5 * (k * k)) / k;
            if (max_err * 2 <= epsilon) {
                // If the error on the left hand side is smaller, we can be more lenient on the right hand
                // side. To this end, we now set epsilon to the part of the error that has not yet been eaten
                // up by the left-hand truncation.
                epsilon -= max_err;
                break;
            }
        }

        // Finally the left truncation point is found.
    }

    // Compute right truncation point.
    {
        ValueType lambda_max;
        int64_t m_max, k;

        // According to Fox-Glynn, if lambda < 400 we should take lambda = 400, otherwise use the original
        // value. This is for computing the right truncation point.
        if (m < 400) {
            lambda_max = 400;
            m_max = 400;
            epsilon *= 0.662608824988162441697980;
            /* i.e. al = (1+1/400) * exp(1/16) * sqrt_2; epsilon /= al; */
        } else {
            lambda_max = lambda;
            m_max = m;
            epsilon *= (1 - 1 / (lambda + 1)) * 0.664265347050632847802225;
            /* i.e. al = (1+1/lambda) * exp(1/16) * sqrt_2; epsilon /= al; */
        }

        // Find right truncation point.

        // This loop is a modification to the original Fox-Glynn paper.
        // The search for the right truncation point is only terminated by  the error condition and not by
        // the stop index from the FG paper. This can yield more accurate results if necessary.
        for (k = 4;; ++k) {
            // dkl_inv is between 1 - 1e-33 and 1 if lambda_max >= 400 and k >= 4; this will always be
            // rounded to 1.0. We therefore leave the factor out.
            // double dkl_inv=1 - exp(-266/401.0 * (k*sqrt(2*lambda_max) + 1.5));

            // actually: "k * (dkl_inv*epsilon/al) >= exp(-0.5 * k^2)", but epsilon has been changed appropriately.
            if (k * epsilon >= exp(-0.5 * (k * k))) {
                break;
            }
        }
        right = m_max + static_cast<int64_t>(std::ceil(k * std::sqrt(2 * lambda_max) + 0.5));
        if (right > m_max + static_cast<int64_t>(std::ceil((lambda_max + 1) * 0.5))) {
            STORM_LOG_WARN("Fox-Glynn: right = " << right << " >> lambda = " << lambda_max << ", cannot bound the right tail. The results are unreliable.");
        }
    }

    // Time to set the initial value for weights.
    FoxGlynnResult<ValueType> fgresult;
    fgresult.left = static_cast<uint64_t>(left);
    fgresult.right = static_cast<uint64_t>(right);
    fgresult.weights.resize(fgresult.right - fgresult.left + 1);

    fgresult.weights[m - left] = omega / (1.0e+10 * (right - left));

    if (m >= 25) {
        // Perform underflow check.
        ValueType result, log_c_m_inf;
        int64_t i;

        // we are going to compare with tau - log(w[m]).
        tau -= std::log(fgresult.weights[m - left]);

        // We take the c_m_inf = 0.14627 / sqrt( m ), as for lambda >= 25
        // c_m = 1 / ( sqrt( 2.0 * pi * m ) ) * exp( m - lambda - 1 / ( 12.0 * m ) ) => c_m_inf.
        // Note that m-lambda is in the interval (-1,0], and -1/(12*m) is in [-1/(12*25),0).
        // So, exp(m-lambda - 1/(12*m)) is in (exp(-1-1/(12*25)),exp(0)).
        // Therefore, we can improve the lower bound on c_m to exp(-1-1/(12*25)) / sqrt(2*pi) = ~0.14627.
        // Its logarithm is -1 - 1/(12*25) - log(2*pi) * 0.5 = ~ -1.922272 (rounded towards -infinity).
        log_c_m_inf = -1.922272 - log((double)m) * 0.5;

        // We use FG's Proposition 6 directly (and not Corollary 4 i and ii), as k_prime may be too large
        // if pFG->left == 0.
        i = m - left;

        // Equivalent to 2*i <= m, equivalent to i <= lambda/2.
        if (i <= left) {
            // Use Proposition 6 (i). Note that Fox--Glynn are off by one in the proof of this proposition;
            // they sum up to i-1, but should have summed up to i. */
            result = log_c_m_inf - i * (i + 1) * (0.5 + (2 * i + 1) / (6 * lambda)) / lambda;
        } else {
            // Use Corollary 4 (iii). Note that k_prime <= sqrt(m+1)/m is a misprint for k_prime <= m/sqrt(m+1),
            // which is equivalent to left >= 0, which holds trivially.
            result = -lambda;
            if (left != 0) {
                // Also use Proposition 6 (ii).
                double result_1 = log_c_m_inf + i * log(1 - i / (double)(m + 1));

                // Take the maximum.
                if (result_1 > result) {
                    result = result_1;
                }
            }
        }
        if (result <= tau) {
            int64_t const log10_result = static_cast<int64_t>(std::floor(result * log10_e));
            STORM_LOG_WARN("Fox-Glynn: lambda >= 25, underflow near Poi(" << lambda << "," << left << ") <= " << std::exp(result - log10_result / log10_e)
                                                                          << log10_result << ". The results are unreliable.");
        }

        // We still have to perform an underflow check for the right truncation point when lambda >= 400.
        if (m >= 400) {
            // Use Proposition 5 of Fox--Glynn.
            i = right - m;
            result = log_c_m_inf - i * (i + 1) / (2 * lambda);
            if (result <= tau) {
                int64_t const log10_result = static_cast<int64_t>(std::floor(result * log10_e));
                STORM_LOG_WARN("Fox-Glynn: lambda >= 25, underflow near Poi(" << lambda << "," << right << ") <= " << std::exp(result - log10_result / log10_e)
                                                                              << log10_result << ". The results are unreliable.");
            }
        }
    }

    return fgresult;
}

template<typename ValueType>
FoxGlynnResult<ValueType> foxGlynnWeighter(ValueType lambda, ValueType epsilon) {
    ValueType tau = std::numeric_limits<ValueType>::min();

    // The magic m point.
    uint64_t m = static_cast<uint64_t>(lambda);
    int64_t j, t;

    FoxGlynnResult<ValueType> result = foxGlynnFinder(lambda, epsilon);

    // Fill the left side of the array.
    for (j = m - result.left; j > 0; --j) {
        result.weights[j - 1] = (j + result.left) / lambda * result.weights[j];
    }

    t = result.right - result.left;

    // Fill the right side of the array, have two cases lambda < 400 & lambda >= 400.
    if (m < 400) {
        // Perform the underflow check, according to Fox-Glynn.
        STORM_LOG_ERROR_COND(result.right <= 600, "Fox-Glynn: " << result.right << " > 600, underflow is possible.");

        // Compute weights.
        for (j = m - result.left; j < t; ++j) {
            ValueType q = lambda / (j + 1 + result.left);
            if (result.weights[j] > tau / q) {
                result.weights[j + 1] = q * result.weights[j];
            } else {
                t = j;
                result.right = j + result.left;
                result.weights.resize(result.right - result.left + 1);

                // It's time to compute W.
                break;
            }
        }
    } else {
        // Compute weights.
        for (j = m - result.left; j < t; ++j) {
            result.weights[j + 1] = lambda / (j + 1 + result.left) * result.weights[j];
        }
    }

    // It is time to compute the normalization weight W.
    result.totalWeight = storm::utility::zero<ValueType>();
    j = 0;

    // t was set above.
    while (j < t) {
        if (result.weights[j] <= result.weights[t]) {
            result.totalWeight += result.weights[j];
            j++;
        } else {
            result.totalWeight += result.weights[t];
            t--;
        }
    }
    result.totalWeight += result.weights[j];

    STORM_LOG_TRACE("Fox-Glynn: ltp = " << result.left << ", rtp = " << result.right << ", w = " << result.totalWeight << ", " << result.weights.size()
                                        << " weights.");

    return result;
}

template<typename ValueType>
FoxGlynnResult<ValueType> foxGlynn(ValueType lambda, ValueType epsilon) {
    STORM_LOG_THROW(lambda > 0, storm::exceptions::InvalidArgumentException, "Fox-Glynn requires positive lambda.");
    return foxGlynnWeighter(lambda, epsilon);
}

template FoxGlynnResult<double> foxGlynn(double lambda, double epsilon);

}  // namespace numerical
}  // namespace utility
}  // namespace storm
