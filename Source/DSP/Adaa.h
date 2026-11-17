#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// First-order antiderivative anti-aliasing (ADAA), sharing the HybridEQ architecture.
//
// A memoryless nonlinearity y = f(x) creates harmonics that fold back below Nyquist as aliasing.
// First-order ADAA replaces the nonlinear residual r(x) = f(x) - slope * x with its exact average
// over [x_{n-1}, x_n]: (Fr(x_n) - Fr(x_{n-1})) / (x_n - x_{n-1}), where Fr is the antiderivative
// of r(x). Leaving the small-signal linear component unaveraged (CurveADAA residual form) preserves
// bit-exact unity magnitude and zero phase shift when clean (essential for parallel dry/wet Mix),
// while band-limiting nonlinear harmonic generation and rail limiting.
namespace hcdsp::adaa
{

// Smooth rail limiter f(v) = v / (1 + |v/rail|^8)^(1/8) and its Simpson-integrated
// cubic-Hermite antiderivative table (accurate to ~1e-11, identical to HybridEQ).
class RailTable
{
public:
    RailTable() noexcept
    {
        for (int i = 0; i <= kSize; ++i)
        {
            const double t = static_cast<double> (i) * kStep;
            fn[static_cast<size_t> (i)] = value (t);
        }
        G[0] = 0.0;
        for (int i = 0; i < kSize; ++i)
        {
            double acc = 0.0;
            const double t0 = static_cast<double> (i) * kStep;
            const double h = kStep / 16.0;
            for (int k = 0; k < 16; ++k)
            {
                const double a = t0 + static_cast<double> (k) * h;
                const double m = a + 0.5 * h;
                const double b = a + h;
                acc += (h / 6.0) * (value (a) + 4.0 * value (m) + value (b));
            }
            G[static_cast<size_t> (i) + 1] = G[static_cast<size_t> (i)] + acc;
        }
    }

    static double value (double t) noexcept
    {
        return t / std::pow (1.0 + std::pow (std::abs (t), 8.0), 0.125);
    }

    // Integral of value(u) for u in [0, t], t >= 0.
    double integral (double t) const noexcept
    {
        if (t >= kMax)
            return G[static_cast<size_t> (kSize)] + (t - kMax);
        const double u = t / kStep;
        const int i = std::min (static_cast<int> (u), kSize - 1);
        const double s = u - static_cast<double> (i);
        const double s2 = s * s;
        const double s3 = s2 * s;
        const size_t idx = static_cast<size_t> (i);
        const double g0 = G[idx], g1 = G[idx + 1];
        const double f0 = fn[idx], f1 = fn[idx + 1];
        return (2.0 * s3 - 3.0 * s2 + 1.0) * g0
             + (s3 - 2.0 * s2 + s) * kStep * f0
             + (-2.0 * s3 + 3.0 * s2) * g1
             + (s3 - s2) * kStep * f1;
    }

private:
    static constexpr int kSize = 4096;
    static constexpr double kMax = 8.0;
    static constexpr double kStep = kMax / static_cast<double> (kSize);
    std::array<double, static_cast<size_t> (kSize) + 1> G {}, fn {};
};

inline const RailTable& railTable() noexcept
{
    static const RailTable table;
    return table;
}

inline double railLimit (double v, double rail = 1.0) noexcept
{
    return rail * RailTable::value (v / rail);
}

inline double railLimitF (double v, double rail = 1.0) noexcept
{
    return rail * rail * railTable().integral (std::abs (v) / rail);
}

// Zero-delay linear-identity residual ADAA (HybridEQ CurveADAA form):
// Keeps the linear term `slope * x` unaveraged so 0% harmonics is bit-exact unity with zero
// phase shift (preventing comb-filtering when blended with dry signal via Mix), while applying
// first-order ADAA to the nonlinear residual r(x) = f(x) - slope * x.
struct ZeroDelayResidual
{
    double x1 = 0.0, Fr1 = 0.0, r1 = 0.0;
    bool primed = false;

    void reset() noexcept { primed = false; x1 = Fr1 = r1 = 0.0; }

    double step (double x, double f, double F, double slope) noexcept
    {
        const double lin = slope * x;
        const double r = f - lin;
        const double Fr = F - 0.5 * slope * x * x;

        if (! primed)
        {
            x1 = x;
            Fr1 = Fr;
            r1 = r;
            primed = true;
            return f;
        }

        const double dx = x - x1;
        double meanR = 0.5 * (r + r1);
        if (std::abs (dx) > 1.0e-5 * std::max ({ 1.0, std::abs (x), std::abs (x1) }))
        {
            meanR = (Fr - Fr1) / dx;
            const double lo = std::min (r, r1);
            const double hi = std::max (r, r1);
            const double tol = (hi - lo) + 1.0e-6 * (1.0 + std::abs (r));
            if (! (meanR >= lo - tol && meanR <= hi + tol))
                meanR = 0.5 * (r + r1);
        }

        x1 = x;
        Fr1 = Fr;
        r1 = r;
        return lin + meanR;
    }
};

} // namespace hcdsp::adaa
