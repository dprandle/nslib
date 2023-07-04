#include <cmath>
#include "algorithm.h"

namespace noble_steed::math
{

bool fequals(float left, float right, float eps)
{
    return (std::abs(left - right) < eps);
}

double round_decimal(double to_round, i8 decimal_places)
{
    double mult = 1.0;
    for (i8 i{0}; i < decimal_places; ++i)
        mult *= 10;
    to_round *= mult;
    to_round = std::round(to_round);
    to_round *= (1.0 / mult);
    return to_round;
}

float random_float(float high, float low)
{
    return low + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (high - low));
}

i8 count_digits(i32 number)
{
    return i8(log10(number) + 1);
}

} // namespace noble_steed
