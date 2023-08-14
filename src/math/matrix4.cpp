
#include "algorithm.h"
#include "vector4.h"
#include "matrix4.h"

namespace nslib
{

namespace math
{

#if NOBLE_STEED_SIMD

float determinant(const matrix4<float> &mat)
{
    // Thanks to GLM for this code
    // First 2 columns
    __m128 swp_2a = _mm_shuffle_ps(mat._data[2], mat._data[2], _MM_SHUFFLE(0, 1, 1, 2));
    __m128 swp_3a = _mm_shuffle_ps(mat._data[3], mat._data[3], _MM_SHUFFLE(3, 2, 3, 3));
    __m128 mula = _mm_mul_ps(swp_2a, swp_3a);

    // Second 2 columns
    __m128 Swp2B = _mm_shuffle_ps(mat._data[2], mat._data[2], _MM_SHUFFLE(3, 2, 3, 3));
    __m128 Swp3B = _mm_shuffle_ps(mat._data[3], mat._data[3], _MM_SHUFFLE(0, 1, 1, 2));
    __m128 MulB = _mm_mul_ps(Swp2B, Swp3B);

    // Columns subtraction
    __m128 SubE = _mm_sub_ps(mula, MulB);

    // Last 2 rows
    __m128 Swp2C = _mm_shuffle_ps(mat._data[2], mat._data[2], _MM_SHUFFLE(0, 0, 1, 2));
    __m128 Swp3C = _mm_shuffle_ps(mat._data[3], mat._data[3], _MM_SHUFFLE(1, 2, 0, 0));
    __m128 MulC = _mm_mul_ps(Swp2C, Swp3C);
    __m128 SubF = _mm_sub_ps(_mm_movehl_ps(MulC, MulC), MulC);

    __m128 SubFacA = _mm_shuffle_ps(SubE, SubE, _MM_SHUFFLE(2, 1, 0, 0));
    __m128 SwpFacA = _mm_shuffle_ps(mat._data[1], mat._data[1], _MM_SHUFFLE(0, 0, 0, 1));
    __m128 MulFacA = _mm_mul_ps(SwpFacA, SubFacA);

    __m128 SubTmpB = _mm_shuffle_ps(SubE, SubF, _MM_SHUFFLE(0, 0, 3, 1));
    __m128 SubFacB = _mm_shuffle_ps(SubTmpB, SubTmpB, _MM_SHUFFLE(3, 1, 1, 0)); //SubF[0], SubE[3], SubE[3], SubE[1];
    __m128 SwpFacB = _mm_shuffle_ps(mat._data[1], mat._data[1], _MM_SHUFFLE(1, 1, 2, 2));
    __m128 MulFacB = _mm_mul_ps(SwpFacB, SubFacB);

    __m128 SubRes = _mm_sub_ps(MulFacA, MulFacB);

    __m128 SubTmpC = _mm_shuffle_ps(SubE, SubF, _MM_SHUFFLE(1, 0, 2, 2));
    __m128 SubFacC = _mm_shuffle_ps(SubTmpC, SubTmpC, _MM_SHUFFLE(3, 3, 2, 0));
    __m128 SwpFacC = _mm_shuffle_ps(mat._data[1], mat._data[1], _MM_SHUFFLE(2, 3, 3, 3));
    __m128 MulFacC = _mm_mul_ps(SwpFacC, SubFacC);

    __m128 AddRes = _mm_add_ps(SubRes, MulFacC);
    __m128 DetCof = _mm_mul_ps(AddRes, _mm_setr_ps(1.0f, -1.0f, 1.0f, -1.0f));

    return _mm_cvtss_f32(_sse_dp(mat._data[0], DetCof));
}

matrix4<float> inverse(const matrix4<float> &mat)
{
    // Thanks to GLM for this code
    matrix4<float> ret;

    __m128 Fac0;
    {
        __m128 Swp0a = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(3, 3, 3, 3));
        __m128 Swp0b = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(2, 2, 2, 2));

        __m128 Swp00 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(2, 2, 2, 2));
        __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp03 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(3, 3, 3, 3));

        __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
        __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
        Fac0 = _mm_sub_ps(Mul00, Mul01);
    }

    __m128 Fac1;
    {
        __m128 Swp0a = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(3, 3, 3, 3));
        __m128 Swp0b = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(1, 1, 1, 1));

        __m128 Swp00 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(1, 1, 1, 1));
        __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp03 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(3, 3, 3, 3));

        __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
        __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
        Fac1 = _mm_sub_ps(Mul00, Mul01);
    }

    __m128 Fac2;
    {
        __m128 Swp0a = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(2, 2, 2, 2));
        __m128 Swp0b = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(1, 1, 1, 1));

        __m128 Swp00 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(1, 1, 1, 1));
        __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp03 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(2, 2, 2, 2));

        __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
        __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
        Fac2 = _mm_sub_ps(Mul00, Mul01);
    }

    __m128 Fac3;
    {
        __m128 Swp0a = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(3, 3, 3, 3));
        __m128 Swp0b = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(0, 0, 0, 0));

        __m128 Swp00 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(0, 0, 0, 0));
        __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp03 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(3, 3, 3, 3));

        __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
        __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
        Fac3 = _mm_sub_ps(Mul00, Mul01);
    }

    __m128 Fac4;
    {
        __m128 Swp0a = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(2, 2, 2, 2));
        __m128 Swp0b = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(0, 0, 0, 0));

        __m128 Swp00 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(0, 0, 0, 0));
        __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp03 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(2, 2, 2, 2));

        __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
        __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
        Fac4 = _mm_sub_ps(Mul00, Mul01);
    }

    __m128 Fac5;
    {
        __m128 Swp0a = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(1, 1, 1, 1));
        __m128 Swp0b = _mm_shuffle_ps(mat._data[3], mat._data[2], _MM_SHUFFLE(0, 0, 0, 0));

        __m128 Swp00 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(0, 0, 0, 0));
        __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
        __m128 Swp03 = _mm_shuffle_ps(mat._data[2], mat._data[1], _MM_SHUFFLE(1, 1, 1, 1));

        __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
        __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
        Fac5 = _mm_sub_ps(Mul00, Mul01);
    }

    __m128 SignA = _mm_set_ps(1.0f, -1.0f, 1.0f, -1.0f);
    __m128 SignB = _mm_set_ps(-1.0f, 1.0f, -1.0f, 1.0f);

    __m128 Temp0 = _mm_shuffle_ps(mat._data[1], mat._data[0], _MM_SHUFFLE(0, 0, 0, 0));
    __m128 Vec0 = _mm_shuffle_ps(Temp0, Temp0, _MM_SHUFFLE(2, 2, 2, 0));

    __m128 Temp1 = _mm_shuffle_ps(mat._data[1], mat._data[0], _MM_SHUFFLE(1, 1, 1, 1));
    __m128 Vec1 = _mm_shuffle_ps(Temp1, Temp1, _MM_SHUFFLE(2, 2, 2, 0));

    __m128 Temp2 = _mm_shuffle_ps(mat._data[1], mat._data[0], _MM_SHUFFLE(2, 2, 2, 2));
    __m128 Vec2 = _mm_shuffle_ps(Temp2, Temp2, _MM_SHUFFLE(2, 2, 2, 0));

    __m128 Temp3 = _mm_shuffle_ps(mat._data[1], mat._data[0], _MM_SHUFFLE(3, 3, 3, 3));
    __m128 Vec3 = _mm_shuffle_ps(Temp3, Temp3, _MM_SHUFFLE(2, 2, 2, 0));

    __m128 Mul00 = _mm_mul_ps(Vec1, Fac0);
    __m128 Mul01 = _mm_mul_ps(Vec2, Fac1);
    __m128 Mul02 = _mm_mul_ps(Vec3, Fac2);
    __m128 Sub00 = _mm_sub_ps(Mul00, Mul01);
    __m128 Add00 = _mm_add_ps(Sub00, Mul02);
    __m128 Inv0 = _mm_mul_ps(SignB, Add00);

    __m128 Mul03 = _mm_mul_ps(Vec0, Fac0);
    __m128 Mul04 = _mm_mul_ps(Vec2, Fac3);
    __m128 Mul05 = _mm_mul_ps(Vec3, Fac4);
    __m128 Sub01 = _mm_sub_ps(Mul03, Mul04);
    __m128 Add01 = _mm_add_ps(Sub01, Mul05);
    __m128 Inv1 = _mm_mul_ps(SignA, Add01);

    __m128 Mul06 = _mm_mul_ps(Vec0, Fac1);
    __m128 Mul07 = _mm_mul_ps(Vec1, Fac3);
    __m128 Mul08 = _mm_mul_ps(Vec3, Fac5);
    __m128 Sub02 = _mm_sub_ps(Mul06, Mul07);
    __m128 Add02 = _mm_add_ps(Sub02, Mul08);
    __m128 Inv2 = _mm_mul_ps(SignB, Add02);

    __m128 Mul09 = _mm_mul_ps(Vec0, Fac2);
    __m128 Mul10 = _mm_mul_ps(Vec1, Fac4);
    __m128 Mul11 = _mm_mul_ps(Vec2, Fac5);
    __m128 Sub03 = _mm_sub_ps(Mul09, Mul10);
    __m128 Add03 = _mm_add_ps(Sub03, Mul11);
    __m128 Inv3 = _mm_mul_ps(SignA, Add03);

    __m128 Row0 = _mm_shuffle_ps(Inv0, Inv1, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 Row1 = _mm_shuffle_ps(Inv2, Inv3, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 Row2 = _mm_shuffle_ps(Row0, Row1, _MM_SHUFFLE(2, 0, 2, 0));

    __m128 Det0 = _sse_dp(mat._data[0], Row2);
    __m128 Rcp0 = _mm_div_ps(_mm_set1_ps(1.0f), Det0);

    ret._data[0] = _mm_mul_ps(Inv0, Rcp0);
    ret._data[1] = _mm_mul_ps(Inv1, Rcp0);
    ret._data[2] = _mm_mul_ps(Inv2, Rcp0);
    ret._data[3] = _mm_mul_ps(Inv3, Rcp0);
    return ret;
}

#endif

} // namespace math
} // namespace nslib
