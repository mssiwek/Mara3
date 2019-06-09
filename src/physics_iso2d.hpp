/**
 ==============================================================================
 Copyright 2019, Jonathan Zrake and Andrew MacFadyen

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do
 so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

 ==============================================================================
*/




#pragma once
#include <cmath>
#include "core_geometric.hpp"
#include "core_matrix.hpp"




//=============================================================================
namespace mara { struct iso2d; }




//=============================================================================
struct mara::iso2d
{
    using unit_conserved_per_area = dimensional_value_t<-2, 1, 0, double>;
    using unit_conserved          = dimensional_value_t< 0, 1, 0, double>;
    using unit_flow               = dimensional_value_t< 0, 1,-1, double>;
    using unit_flux               = dimensional_value_t<-1, 1,-1, double>;

    using conserved_per_area_t    = covariant_sequence_t<unit_conserved_per_area, 3>;
    using conserved_t             = covariant_sequence_t<unit_conserved, 3>;
    using flow_t                  = covariant_sequence_t<unit_flow, 3>;
    using flux_t                  = covariant_sequence_t<unit_flux, 3>;

    struct primitive_t;
    struct wavespeeds_t
    {
        unit_velocity<double> m;
        unit_velocity<double> p;
    };

    static inline primitive_t recover_primitive(const conserved_per_area_t& U);

    static inline primitive_t roe_average(
        const primitive_t& Pl,
        const primitive_t& Pr);

    static inline flux_t riemann_hlle(
        const primitive_t& Pl,
        const primitive_t& Pr,
        double sound_speed_squared_l,
        double sound_speed_squared_r,
        const unit_vector_t& nhat);

    static inline flux_t riemann_hllc(
        const primitive_t& Pl,
        const primitive_t& Pr,
        double sound_speed_squared_l,
        double sound_speed_squared_r,
        const unit_vector_t& nhat);
};




//=============================================================================
struct mara::iso2d::primitive_t : public mara::arithmetic_sequence_t<double, 3, primitive_t>
{




    /**
     * @brief      Retrieve const-references to the quantities by name.
     */
    const double& sigma() const { return operator[](0); }
    const double& velocity_x() const { return operator[](1); }
    const double& velocity_y() const { return operator[](2); }




    /**
     * @brief      Return a new state with individual, named quantities replaced.
     *
     * @param[in]  v     The new value
     *
     * @return     A new primitive variable state
     */
    primitive_t with_sigma(double v) const { auto res = *this; res[0] = v; return res; }
    primitive_t with_velocity_x(double v) const { auto res = *this; res[1] = v; return res; }
    primitive_t with_velocity_y(double v) const { auto res = *this; res[2] = v; return res; }




    /**
     * @brief      Return the square of the four-velocity magnitude.
     *
     * @return     u^2
     */
    double velocity_squared() const
    {
        const auto&_ = *this;
        return _[1] * _[1] + _[2] * _[2];
    }

    unit_velocity<double> velocity_magnitude() const
    {
        return std::sqrt(velocity_squared());
    }

    auto velocity() const
    {
        return covariant_sequence_t<double, 3>{velocity_x(), velocity_y(), 0.0};
    }




    /**
     * @brief      Return the kinematic three-velocity along the given unit
     *             vector.
     *
     * @param[in]  nhat  The unit vector
     *
     * @return     v
     */
    double velocity_along(const unit_vector_t& nhat) const
    {
        const auto&_ = *this;
        return nhat.project(_[1], _[2], 0.0);
    }




    /**
     * @brief      Convert this state to conserved mass and momentum, per unit
     *             area.
     *
     * @return     The conserved quantities per unit area.
     */
    conserved_per_area_t to_conserved_per_area() const
    {
        auto U = conserved_per_area_t();
        U[0].value = sigma();
        U[1].value = sigma() * velocity_x();
        U[2].value = sigma() * velocity_y();
        return U;
    }




    /**
     * @brief      Return the flux of conserved quantities in the given
     *             direction.
     *
     * @param[in]  nhat             The unit vector
     * @param[in]  gamma_law_index  The gamma law index
     *
     * @return     The flux F
     */
    flux_t flux(const unit_vector_t& nhat, double sound_speed_squared) const
    {
        auto v = velocity_along(nhat);
        auto p = sigma() * sound_speed_squared;
        auto F = flux_t();
        F[0] = v * sigma();
        F[1] = v * sigma() * velocity_x() + p * nhat.get_n1();
        F[2] = v * sigma() * velocity_y() + p * nhat.get_n2();
        return F;
    }




    /**
     * @brief      Return the wavespeeds along a given direction
     *
     * @param[in]  nhat             The direction
     * @param[in]  gamma_law_index  The gamma law index
     *
     * @return     The wavespeeds
     */
    wavespeeds_t wavespeeds(const unit_vector_t& nhat, double sound_speed_squared) const
    {
        auto cs = std::sqrt(sound_speed_squared);
        auto vn = velocity_along(nhat);
        return {
            make_velocity(vn - cs),
            make_velocity(vn + cs),
        };
    }
};




/**
 * @brief      Attempt to recover a primitive variable state from the given
 *             vector of conserved densities.
 *
 * @param[in]  U     The conserved densities
 *
 * @return     A primitive variable state, if the recovery succeeds
 */
mara::iso2d::primitive_t mara::iso2d::recover_primitive(const conserved_per_area_t& U)
{
    auto P = primitive_t();
    P[0] = U[0].value;
    P[1] = U[1].value / U[0].value;
    P[2] = U[2].value / U[0].value;
    return P;
}




/**
 * @brief      Compute a sensible Roe-average state Q = Roe(Pr, Pl), defined
 *             here as the primitives weighted by square root of the mass
 *             density. This averaged state is symmetric in (Pr, Pl), and has
 *             the property that A(Q) * (Ur - Ul) = F(Ur) - F(Ul), where A is
 *             the flux Jacobian.
 *
 * @param[in]  Pr    The other state
 * @param[in]  Pl    The first state
 *
 * @return     The Roe-averaged primitive state
 */
mara::iso2d::primitive_t mara::iso2d::roe_average(
    const primitive_t& Pr,
    const primitive_t& Pl)
{
    auto kr = std::sqrt(Pr.sigma());
    auto kl = std::sqrt(Pl.sigma());
    return (Pr * kr + Pl * kl) / (kr + kl);
}




/**
 * @brief      Return the HLLE flux for the given pair of states
 *
 * @param[in]  Pl                     The state to the left of the interface
 * @param[in]  Pr                     The state to the right
 * @param[in]  sound_speed_squared_l  The sound speed squared to the left of the interface
 * @param[in]  sound_speed_squared_r  The sound speed squared to the right
 * @param[in]  nhat                   The normal vector to the interface
 * @param[in]  gamma_law_index  The gamma law index
 *
 * @return     A vector of fluxes
 */
mara::iso2d::flux_t mara::iso2d::riemann_hlle(
    const primitive_t& Pl,
    const primitive_t& Pr,
    double sound_speed_squared_l,
    double sound_speed_squared_r,
    const unit_vector_t& nhat)
{
    auto Ul = Pl.to_conserved_per_area();
    auto Ur = Pr.to_conserved_per_area();
    auto Al = Pl.wavespeeds(nhat, sound_speed_squared_l);
    auto Ar = Pr.wavespeeds(nhat, sound_speed_squared_r);
    auto Fl = Pl.flux(nhat, sound_speed_squared_l);
    auto Fr = Pr.flux(nhat, sound_speed_squared_r);

    auto ap = std::max(make_velocity(0.0), std::max(Al.p, Ar.p));
    auto am = std::min(make_velocity(0.0), std::min(Al.m, Ar.m));

    return (Fl * ap - Fr * am - (Ul - Ur) * ap * am) / (ap - am);
}




/**
 * @brief      Return the HLLC flux for the given pair of states, following Toro
 *             3rd ed. Sec 10.6.
 *
 * @param[in]  Pl                     The state to the left of the interface
 * @param[in]  Pr                     The state to the right
 * @param[in]  sound_speed_squared_l  The isothermal sound speed squared
 * @param[in]  sound_speed_squared_r  The sound speed squared r
 * @param[in]  nhat                   The normal vector to the interface
 *
 * @return     A vector of fluxes
 */
mara::iso2d::flux_t mara::iso2d::riemann_hllc(
    const primitive_t& Pl,
    const primitive_t& Pr,
    double sound_speed_squared_l,
    double sound_speed_squared_r,
    const unit_vector_t& nhat)
{
    // left and right parallel velocity magnitudes
    auto ul = Pl.velocity_along(nhat);
    auto ur = Pr.velocity_along(nhat);

    // left and right parallel and perpendicular velocity vectors
    auto v_para_l = nhat * ul;
    auto v_para_r = nhat * ur;
    auto v_perp_l = Pl.velocity() - v_para_l;
    auto v_perp_r = Pl.velocity() - v_para_r;

    // left, right, and average sigma's
    auto sigma_l   = Pl.sigma();
    auto sigma_r   = Pr.sigma();
    auto sigma_bar = 0.5 * (sigma_l + sigma_r);

    // left, right, and average sound speeds
    auto al        = std::sqrt(sound_speed_squared_l);
    auto ar        = std::sqrt(sound_speed_squared_r);
    auto a_bar     = 0.5 * (al + ar);

    // left, right, and star-state pressures (Equation 10.61)
    auto press_l   = sigma_l * sound_speed_squared_l;
    auto press_r   = sigma_r * sound_speed_squared_r;
    auto ppvrs     = 0.5 * (press_l + press_r) - 0.5 * (ur - ul) * sigma_bar * a_bar; 
    auto pstar     = std::max(0.0, ppvrs);

    // Equation 10.69 for the isothermal case, gamma = 1
    auto ql = std::max(1.0, std::sqrt(pstar / press_l));
    auto qr = std::max(1.0, std::sqrt(pstar / press_r));

    // Equation 10.68 for the wavespeeds
    auto sl = ul - al * ql;
    auto sr = ur + ar * qr;

    // Equation 10.70 for the contact speed
    auto sstar = (press_r - press_l + ul * sigma_l * (sl - ul) - ur * sigma_r * (sr - ur)) /
                 (                         sigma_l * (sl - ul) -      sigma_r * (sr - ur));

    auto Ulstar = conserved_per_area_t();
    auto Urstar = conserved_per_area_t();

    Ulstar[0] = sigma_l * (sl - ul) / (sl - sstar);
    Ulstar[1] = sigma_l * (sl - ul) / (sl - sstar) * (sstar * nhat[0] + v_perp_l[0]);
    Ulstar[2] = sigma_l * (sl - ul) / (sl - sstar) * (sstar * nhat[1] + v_perp_l[1]);
    Urstar[0] = sigma_r * (sr - ur) / (sr - sstar);
    Urstar[1] = sigma_r * (sr - ur) / (sr - sstar) * (sstar * nhat[0] + v_perp_r[0]);
    Urstar[2] = sigma_r * (sr - ur) / (sr - sstar) * (sstar * nhat[1] + v_perp_r[1]);

    auto Ul = Pl.to_conserved_per_area();
    auto Ur = Pr.to_conserved_per_area();
    auto Fl = Pl.flux(nhat, sound_speed_squared_l);
    auto Fr = Pr.flux(nhat, sound_speed_squared_r);

    if      (0.0   <= sl                 ) return Fl;
    else if (sl    <= 0.0 && 0.0 <= sstar) return Fl + (Ulstar - Ul) * make_velocity(sl);
    else if (sstar <= 0.0 && 0.0 <= sr   ) return Fr + (Urstar - Ur) * make_velocity(sr);
    else if (sr    <= 0.0                ) return Fr;

    throw;
}
