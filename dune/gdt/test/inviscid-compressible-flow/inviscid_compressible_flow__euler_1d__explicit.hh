// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2017 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2018)

#ifndef DUNE_GDT_TEXT_INVISCID_COMPRESSIBLE_FLOW_EULER_1D_EXPLICIT_HH
#define DUNE_GDT_TEXT_INVISCID_COMPRESSIBLE_FLOW_EULER_1D_EXPLICIT_HH

#include <algorithm>
#include <cmath>
#include <sstream>

#include <dune/xt/common/test/gtest/gtest.h>
#include <dune/xt/common/vector.hh>
#include <dune/xt/grid/boundaryinfo.hh>
#include <dune/xt/grid/grids.hh>
#include <dune/xt/grid/gridprovider/cube.hh>
#include <dune/xt/grid/intersection.hh>
#include <dune/xt/grid/view/periodic.hh>
#include <dune/xt/functions/constant.hh>
#include <dune/xt/functions/lambda/global-function.hh>
#include <dune/xt/functions/lambda/global-flux-function.hh>

#include <dune/gdt/discretefunction/default.hh>
#include <dune/gdt/local/operators/lambda.hh>
#include <dune/gdt/operators/advection-fv.hh>
#include <dune/gdt/operators/l1.hh>
#include <dune/gdt/operators/l2.hh>
#include <dune/gdt/projections.hh>
#include <dune/gdt/timestepper/explicit-rungekutta.hh>
#include <dune/gdt/spaces/fv/default.hh>
#include <dune/gdt/tools/euler.hh>

namespace Dune {
namespace GDT {


template <template <class, class, size_t> class Space,
          template <class> class SpatialOperator,
          XT::Grid::Backends layer_backend = XT::Grid::Backends::view>
struct InviscidCompressibleFlowEuler1dExplicitTest : public ::testing::Test
{
  using G = YASP_1D_EQUIDISTANT_OFFSET;
  using E = typename G::template Codim<0>::Entity;
  using D = double;
  static const constexpr size_t d = G::dimension;
  using R = double;
  static const constexpr size_t m = d + 2;

  using DomainType = XT::Common::FieldVector<D, d>;
  using RangeType = XT::Common::FieldVector<D, m>;

  using GridProvider = XT::Grid::GridProvider<G>;
  using LeafGL = typename XT::Grid::Layer<G, XT::Grid::Layers::leaf, layer_backend>::type;
  using GL = XT::Grid::PeriodicGridLayer<LeafGL>;

  using UI = XT::Functions::LocalizableFunctionInterface<E, D, d, R, m>;
  using U = XT::Functions::GlobalLambdaFunction<E, D, d, R, m>;
  using F = XT::Functions::GlobalLambdaFluxFunction<UI, 0, R, d, m>;

  using NF = NumericalVijayasundaramFlux<E, D, d, R, m>;

  using S = Space<GL, R, m>;
  using V = XT::LA::CommonDenseVector<R>;
  using DF = DiscreteFunction<S, V>;
  using Op = SpatialOperator<DF>;

  using TimeStepper = ExplicitRungeKuttaTimeStepper<Op, DF, TimeStepperMethods::explicit_euler>;

  InviscidCompressibleFlowEuler1dExplicitTest()
    : logger_(XT::Common::TimedLogger().get("main"))
    , euler_tools_(/*gamma=*/1.4) // air or water at roughly 20 deg Cels.
    , grid_(nullptr)
    , leaf_grid_layer_(nullptr)
    , grid_layer_(nullptr)
    , space_(nullptr)
    , initial_values_(nullptr)
    , flux_(nullptr)
    , numerical_flux_(nullptr)
    , op_(nullptr)
  {
  }

  void SetUp() override final
  {
    grid_ = std::make_shared<GridProvider>(XT::Grid::make_cube_grid<G>(-1., 1., 128u));
    leaf_grid_layer_ = std::make_shared<LeafGL>(grid_->template layer<XT::Grid::Layers::leaf, layer_backend>());
    grid_layer_ = std::make_shared<GL>(XT::Grid::make_periodic_grid_layer(*leaf_grid_layer_));
    logger_.info() << "grid has " << leaf_grid_layer_->indexSet().size(0) << " elements" << std::endl;

    space_ = std::make_shared<S>(*grid_layer_);
    logger_.info() << "space has " << space_->mapper().size() << " DoFs\n" << std::endl;

    initial_values_ = std::make_shared<DF>(*space_, "solution");
    const U periodic_initial_values_euler( // compare [Kröner, 1997, p. 394]
        [&](const auto& xx, const auto& /*mu*/) {
          FieldVector<R, m> primitive_variables(0.);
          // density
          if (XT::Common::FloatCmp::ge(xx, DomainType(-0.5)) && XT::Common::FloatCmp::le(xx, DomainType(0)))
            primitive_variables[0] = 4.;
          else
            primitive_variables[0] = 1.;
          // velocity
          for (size_t ii = 0; ii < d; ++ii)
            primitive_variables[1 + ii] = 0.;
          // pressure
          if (XT::Common::FloatCmp::ge(xx, DomainType(-0.5)) && XT::Common::FloatCmp::le(xx, DomainType(0)))
            primitive_variables[m - 1] = 1.6;
          else
            primitive_variables[m - 1] = 0.4;
          return euler_tools_.to_conservative(primitive_variables);
        },
        /*order=*/0,
        /*parameter_type=*/{},
        /*name=*/"periodic_initial_values_euler");
    project(periodic_initial_values_euler, *initial_values_);

    flux_ = std::shared_ptr<F>(new F([&](const auto& /*x*/,
                                         const auto& conservative_variables,
                                         const auto& /*mu*/) { return euler_tools_.flux(conservative_variables); },
                                     {},
                                     "euler_flux",
                                     [](const auto& /*mu*/) { return 3; },
                                     [&](const auto& /*x*/, const auto& conservative_variables, const auto& /*mu*/) {
                                       return euler_tools_.flux_jacobian(conservative_variables);
                                     }));
    numerical_flux_ = std::make_shared<NF>(*flux_);
  } // ... SetUp(...)

  void do_the_timestepping(
      const size_t num_timesteps = 100,
      const double& expected_fv_dt = 0.002708880865541605,
      const double& CFL = 1.,
      const std::pair<XT::Common::FieldVector<R, m>, XT::Common::FieldVector<R, m>>& boundary_data_range = {
          XT::Common::FieldVector<R, m>(std::numeric_limits<R>::max()),
          XT::Common::FieldVector<R, m>(std::numeric_limits<R>::min())})
  {
    ASSERT_NE(op_, nullptr) << "You have to create the op_ before calling this method!";
    time_stepper_ = std::make_shared<TimeStepper>(*op_, *initial_values_, -1.);

    // no idea why we need to provide the <GL, E, D, d, R, m> here, but the compiler could not figure it out without
    auto dt = estimate_dt_for_hyperbolic_system<GL, E, D, d, R, m>(
        *grid_layer_, *initial_values_, *flux_, boundary_data_range);
    EXPECT_DOUBLE_EQ(expected_fv_dt, dt);
    dt *= CFL;
    const double T = num_timesteps * dt;

    // test for stability
    const auto test_dt = time_stepper_->find_suitable_dt(dt, 10, 1.1 * initial_values_->vector().sup_norm(), 25);
    ASSERT_TRUE(test_dt.first) << "Could not determine optimal dt (in particular, the dt computed to match the CFL "
                                  "condition did not yield a stable scheme)!";
    ASSERT_DOUBLE_EQ(test_dt.second, dt)
        << "The computed dt (to match the CFL condition) does not yield a stable scheme: " << dt
        << "\nThe following dt seems to work fine: " << test_dt.second;

    // do the work
    time_stepper_->solve(T,
                         dt,
                         num_timesteps,
                         /*save_solution=*/true,
                         /*visualize=*/false,
                         "solution",
                         /*visualizer=*/[&](const auto& u, const auto& filename_prefix, const auto& step) {
                           euler_tools_.visualize(u, *grid_layer_, filename_prefix, XT::Common::to_string(step));
                         });
  } // ... do_the_timestepping(...)

  void periodic_boundaries(const size_t num_timesteps = 100,
                           const double& CFL = 1.,
                           const std::tuple<double, double>& tolerances = {1e-15, 1e-15})
  {
    ASSERT_NE(grid_layer_, nullptr);
    ASSERT_NE(numerical_flux_, nullptr);
    ASSERT_NE(initial_values_, nullptr);
    ASSERT_NE(space_, nullptr);

    // create operator, the layer is periodic and the operator includes handling of periodic boundaries
    op_ = std::make_shared<Op>(*grid_layer_, *numerical_flux_);

    // do timestepping
    this->do_the_timestepping(num_timesteps, 0.002708880865541605, CFL);
    ASSERT_NE(time_stepper_, nullptr);

    const auto& relative_mass_conservation_error_tolerance = std::get<0>(tolerances);
    const auto& relative_expected_state_l_2_error_tolerance = std::get<1>(tolerances);

    // check conservation principle
    const auto initial_mass = make_l1_operator(*grid_layer_)->induced_norm(*initial_values_);
    size_t saved_timesteps = 0;
    for (const auto& t_and_u : time_stepper_->solution()) {
      ++saved_timesteps;
      const auto& u_conservative = t_and_u.second;
      const auto mass_conservation_error =
          std::abs(initial_mass - make_l1_operator(*grid_layer_)->induced_norm(u_conservative));
      EXPECT_LT(mass_conservation_error / initial_mass, relative_mass_conservation_error_tolerance)
          << "mass_conservation_error = " << mass_conservation_error << "\n"
          << "initial_mass = " << initial_mass;
    }
    EXPECT_EQ(num_timesteps + 1, saved_timesteps);

    // check expected state at the end
    auto leaf_view = grid_->leaf_view();
    using FvS = FvSpace<decltype(leaf_view), R, m>;
    const FvS fv_space(leaf_view);
    const V expected_end_state_vector(expected_end_state__periodic_boundaries());
    const ConstDiscreteFunction<FvS, V> expected_end_state_fv(fv_space, expected_end_state_vector);
    const auto& actual_end_state = time_stepper_->solution().rbegin()->second;

    const auto l2_error = make_l2_operator(*grid_layer_)->induced_norm(expected_end_state_fv - actual_end_state);
    const auto reference_l2_norm = make_l2_operator(*grid_layer_)->induced_norm(expected_end_state_fv);

    EXPECT_LT(l2_error / reference_l2_norm, relative_expected_state_l_2_error_tolerance)
        << "l2_error = " << l2_error << "\n"
        << "reference_l2_norm = " << reference_l2_norm << "\n\n"
        << "actual_end_state = " << print_vector(actual_end_state.vector());
  } // ... periodic_boundaries(...)

  static std::vector<double> expected_end_state__periodic_boundaries()
  {
    return {1.0000013466060014,      -1.007708936471061e-06,  1.0000018852500976,      1.0000044547440901,
            -3.3336389923735734e-06, 1.0000062366599656,      1.000014162794056,       -1.0598601470495452e-05,
            1.0000198280927333,      1.000043278400877,       -3.2387847239154085e-05, 1.0000605914221288,
            1.0001271363870938,      -9.5150841989372032e-05, 1.0001780050273525,      1.0003591157268059,
            -0.00026882151151833586, 1.0005028725006329,      1.0009754377211006,      -0.00073056045889799773,
            1.001366414575251,       1.0025466912916001,      -0.0019098633059258037,  1.0035707470786868,
            1.0063791334121701,      -0.0047990073260674075,  1.0089640383862377,      1.0152525334758986,
            -0.011556252795424875,   1.0215410194458294,      1.0344167689618096,      -0.026465478852591921,
            1.0491252572310434,      1.0717895217997457,      -0.056729189674759324,   1.1045402937983513,
            1.1345944672335124,      -0.11088822552498599,    1.2023203685383403,      1.2214962381923051,
            -0.19199643614110681,    1.3466923165714426,      1.318393207671563,       -0.28991309450150105,
            1.5191861689934714,      1.4064898984983827,      -0.38498980936562155,    1.6857360789318017,
            1.4740706738430702,      -0.46144563975373926,    1.8193503686036647,      1.519606001779207,
            -0.51450081921013058,    1.9119474298981303,      1.5478521082711401,      -0.54757063753091517,
            1.9693139550900038,      1.5658078733144107,      -0.56708662728700576,    2.0019170714815573,
            1.5811005497742998,      -0.57952996128806766,    2.0194077923147566,      1.6020567438188698,
            -0.59064351046306418,    2.0288955951068219,      1.637910468064709,       -0.60550700348793163,
            2.0350692009495228,      1.6976821954586885,      -0.6283864059322446,     2.0408008920090324,
            1.7869319023449324,      -0.66181003497671198,    2.0476112699953899,      1.9032430479136619,
            -0.70510282990341322,    2.0558660436391993,      2.0334797411187977,      -0.75349613653548186,
            2.0648975771168909,      2.1565336929162089,      -0.79921077698650322,    2.0733560945837159,
            2.2522404991379013,      -0.83479169848126478,    2.0798968043590746,      2.3114265912752696,
            -0.85684760016965011,    2.083899954809131,       2.3392396063595036,      -0.86729577601409358,
            2.0857112114673102,      2.3495693584200379,      -0.8708360702921687,     2.0868782867966438,
            2.3594878090231135,      -0.8700490995609067,     2.0935220152989653,      2.3858965507748144,
            -0.86320896878437114,    2.1172843206457626,      2.4395232929593429,      -0.84710304992662333,
            2.168019040837367,       2.5213650728113812,      -0.82007172518620353,    2.247679359487039,
            2.6259471479126888,      -0.78220350934058758,    2.3522148436628951,      2.7461027591121283,
            -0.73442560285831604,    2.4758328024512104,      2.875508269532367,       -0.67801868499946605,
            2.613128798835147,       3.0090513581218814,      -0.61445262326237748,    2.7593667089725793,
            3.1424675478253601,      -0.54534702557942139,    2.9101211473906226,      3.2719132514803162,
            -0.47243300608725741,    3.0608637997446988,      3.3936513815634881,      -0.39746595752458397,
            3.2066604292704208,      3.5038736821181184,      -0.32207774266836353,    3.3420261199208121,
            3.5986697395755836,      -0.24758527153533763,    3.4609838548574712,      3.6741637361057897,
            -0.17480020554354553,    3.5573815207499746,      3.726826687177506,       -0.10390461773147322,
            3.6254886508954995,      3.7539167274869008,      -0.034455164260433178,   3.6607941606870735,
            3.7539167274868928,      0.034455164260433054,    3.6607941606870731,      3.7268266871774998,
            0.10390461773147293,     3.6254886508954995,      3.6741637361058204,      0.17480020554354664,
            3.5573815207499742,      3.5986697395755729,      0.24758527153533685,     3.4609838548574707,
            3.5038736821181176,      0.32207774266836303,     3.3420261199208112,      3.393651381563485,
            0.39746595752458352,     3.2066604292704195,      3.2719132514803055,      0.47243300608725658,
            3.0608637997446966,      3.1424675478253561,      0.54534702557942083,     2.9101211473906226,
            3.0090513581218783,      0.6144526232623776,      2.7593667089725789,      2.8755082695323657,
            0.67801868499946627,     2.6131287988351448,      2.7461027591121265,      0.73442560285831648,
            2.47583280245121,        2.6259471479126861,      0.78220350934058758,     2.3522148436628947,
            2.5213650728113803,      0.82007172518620264,     2.247679359487039,       2.4395232929593424,
            0.84710304992662344,     2.1680190408373674,      2.3858965507748127,      0.86320896878437159,
            2.1172843206457626,      2.3594878090231139,      0.87004909956090648,     2.0935220152989662,
            2.3495693584200379,      0.87083607029216892,     2.0868782867966438,      2.3392396063595027,
            0.86729577601409458,     2.0857112114673111,      2.3114265912752687,      0.85684760016965045,
            2.0838999548091315,      2.2522404991378995,      0.83479169848126455,     2.0798968043590751,
            2.156533692916208,       0.79921077698650256,     2.0733560945837142,      2.0334797411187986,
            0.75349613653548275,     2.0648975771168909,      1.9032430479136633,      0.70510282990341322,
            2.0558660436391998,      1.7869319023449348,      0.66181003497671265,     2.0476112699953917,
            1.6976821954586898,      0.62838640593224493,     2.0408008920090333,      1.6379104680647092,
            0.60550700348793218,     2.0350692009495237,      1.6020567438188695,      0.59064351046306418,
            2.0288955951068219,      1.5811005497742998,      0.57952996128806766,     2.0194077923147566,
            1.5658078733144112,      0.56708662728700587,     2.0019170714815577,      1.5478521082711409,
            0.54757063753091517,     1.9693139550900034,      1.519606001779207,       0.51450081921013024,
            1.9119474298981289,      1.4740706738430702,      0.46144563975374009,     1.8193503686036647,
            1.4064898984983805,      0.38498980936562099,     1.6857360789318021,      1.3183932076715654,
            0.2899130945015016,      1.5191861689934716,      1.221496238192302,       0.19199643614110648,
            1.3466923165714419,      1.1345944672335149,      0.11088822552498603,     1.2023203685383399,
            1.0717895217997426,      0.056729189674758818,    1.1045402937983511,      1.0344167689618122,
            0.026465478852591956,    1.0491252572310437,      1.0152525334759077,      0.011556252795424923,
            1.0215410194458294,      1.0063791334121563,      0.0047990073260674084,   1.0089640383862379,
            1.0025466912916026,      0.0019098633059258323,   1.003570747078687,       1.0009754377211053,
            0.00073056045889800716,  1.0013664145752508,      1.0003591157268044,      0.00026882151151820115,
            1.0005028725006326,      1.0001271363870972,      9.515084198936241e-05,   1.0001780050273525,
            1.0000432784008766,      3.2387847239163707e-05,  1.0000605914221286,      1.0000141627940529,
            1.059860147056282e-05,   1.0000198280927326,      1.0000044547440927,      3.3336389926430426e-06,
            1.0000062366599654,      1.0000013466059992,      1.0077089366539149e-06,  1.0000018852500976,
            1.0000003911827935,      2.9273450724623062e-07,  1.0000005476560563,      1.0000001092050823,
            8.172160862392107e-08,   1.0000001528871254,      1.0000000292989173,      2.192530327153722e-08,
            1.0000000410184859,      1.0000000075551212,      5.6537353301225452e-09,  1.0000000105771703,
            1.0000000018726529,      1.4013644091063348e-09,  1.0000000026217131,      1.0000000004462188,
            3.3392012206851674e-10,  1.0000000006247076,      1.0000000001022271,      7.6500033828831142e-11,
            1.0000000001431189,      1.0000000000225202,      1.6852238215810858e-11,  1.0000000000315279,
            1.0000000000047693,      3.5702951711201839e-12,  1.0000000000066795,      1.0000000000009728,
            7.273727071948405e-13,   1.0000000000013611,      1.0000000000001921,      1.4246226759202467e-13,
            1.0000000000002669,      1.0000000000000335,      2.6869867669859681e-14,  1.0000000000000504,
            1.000000000000006,       4.9370494679935592e-15,  1.0000000000000093,      1.000000000000002,
            7.121669797885445e-16,   1.0000000000000018,      0.99999999999999845,     1.6360592778926019e-16,
            1.0000000000000004,      1.0000000000000000,      2.1172531831551323e-16,  1.0000000000000002,
            1.0000000000000002,      6.7367146736754204e-17,  1.0000000000000000,      1.0000000000000007,
            -1.8285368399976143e-16, 1.0000000000000004,      1.0000000000000027,      -8.5652515136730364e-16,
            1.0000000000000016,      1.0000000000000064,      -4.9081778336778064e-15, 1.0000000000000091,
            1.0000000000000351,      -2.6840996035543926e-14, 1.0000000000000502,      1.0000000000001898,
            -1.425103869825509e-13,  1.0000000000002667,      1.0000000000009721,      -7.2740157882915619e-13,
            1.0000000000013611,      1.0000000000047702,      -3.5701411890705e-12,    1.0000000000066795,
            1.0000000000225202,      -1.6852382573982434e-11, 1.0000000000315281,      1.0000000001022293,
            -7.650021668251514e-11,  1.0000000001431189,      1.0000000004462193,      -3.3392008357300429e-10,
            1.0000000006247074,      1.0000000018726496,      -1.4013647940614589e-09, 1.0000000026217135,
            1.0000000075551243,      -5.6537354841045952e-09, 1.0000000105771707,      1.0000000292989182,
            -2.1925303204170072e-08, 1.0000000410184864,      1.0000001092050819,      -8.172160844106737e-08,
            1.0000001528871256,      1.0000003911827926,      -2.9273450717886344e-07, 1.0000005476560563};
  } // ... expected_end_state__periodic_boundaries(...)

  void impermeable_walls_by_direct_euler_treatment()
  {
    ASSERT_NE(grid_layer_, nullptr);
    ASSERT_NE(numerical_flux_, nullptr);
    ASSERT_NE(space_, nullptr);

    // impermeable walls everywhere
    XT::Grid::NormalBasedBoundaryInfo<XT::Grid::extract_intersection_t<GL>> boundary_info;
    boundary_info.register_new_normal({-1}, new XT::Grid::ImpermeableBoundary());
    boundary_info.register_new_normal({1}, new XT::Grid::ImpermeableBoundary());
    XT::Grid::ApplyOn::CustomBoundaryIntersections<GL> impermeable_wall_filter(boundary_info,
                                                                               new XT::Grid::ImpermeableBoundary());

    // create operator, the layer is periodic and the operator includes handling of periodic boundaries so we need to
    // make an exception for all non-periodic boundaries
    op_ = std::make_shared<Op>(
        *grid_layer_, *numerical_flux_, /*periodicity_restriction=*/impermeable_wall_filter.copy());

    // the actual handling of impermeable walls
    const auto euler_impermeable_wall_treatment = [&](const auto& u, const auto& n, const auto& /*mu*/ = {}) {
      return euler_tools_.flux_at_impermeable_walls(u, n);
    }; //                                         | flux order: only required for DG spaces
    op_->append(euler_impermeable_wall_treatment, euler_tools_.flux_order(), impermeable_wall_filter.copy(), {});

    // do timestepping
    this->do_the_timestepping(300); // we need enough for the wave to hit the boundary
    ASSERT_NE(time_stepper_, nullptr);

    // check expected state at the end
    V expected_end_state(expected_end_state__impermeable_walls_by_direct_euler_treatment());

    const auto& actual_end_state = time_stepper_->solution().rbegin()->second.vector();
    EXPECT_LT((expected_end_state - actual_end_state).sup_norm(), 1e-15) << "actual_end_state = "
                                                                         << print_vector(actual_end_state);
  } // ... impermeable_walls_by_direct_euler_treatment(...)

  static std::vector<double> expected_end_state__impermeable_walls_by_direct_euler_treatment()
  {
    return {2.4333683449209049,      -0.00034510309372732661, 3.5070282077224091,      2.4129251526029525,
            -0.00048135403348335907, 3.5064729726900739,      2.4102795368359966,      -0.00077585929697480613,
            3.5061597418367745,      2.4094606046399374,      -0.00099329187530774587, 3.5055297521690312,
            2.408558773860435,       -0.0010958271868925979,  3.504367904647514,       2.4079702252657471,
            -0.0011053391961194706,  3.5022371280433693,      2.4089949999331002,      -0.0012221659306074822,
            3.4982136293279811,      2.4144451797505933,      -0.0020358236847386113,  3.4904241519559549,
            2.4288615756272685,      -0.004890008933088808,   3.475270693843457,       2.4570330768754882,
            -0.012550639873116673,   3.4461037207420779,      2.4995299909813693,      -0.03040983919076598,
            3.3910283704071111,      2.545553822030584,       -0.068129226165393839,   3.2902004237202367,
            2.5675244687087826,      -0.13964961033183751,    3.1164469198428,         2.5285593894752196,
            -0.25493861764348091,    2.8508391252368064,      2.4150802546676733,      -0.39909657415126193,
            2.5209846950326793,      2.2713606921342615,      -0.5262694610396732,     2.2151842769994352,
            2.1655306565307506,      -0.60216637594507949,    2.0076136726529508,      2.1168503403819887,
            -0.6326422784952892,     1.8933056527059249,      2.1011997694768874,      -0.63735295234516065,
            1.8297943004937052,      2.0945710613215969,      -0.62906248695577127,    1.7863293594080167,
            2.0857058880154495,      -0.61373043952860029,    1.7491365436277015,      2.0714322799315861,
            -0.59414717978247433,    1.7132625078929753,      2.0519435595307107,      -0.57184194172773084,
            1.6772066823679197,      2.0284510857141078,      -0.54779231362320702,    1.6406654059546013,
            2.0022197595551376,      -0.52266703481454146,    1.6037089656655776,      1.9742419813467598,
            -0.49692548003110443,    1.566501284652392,       1.9451926445596064,      -0.47087983805797512,
            1.5292123552223786,      1.915493545352245,       -0.44474440077580701,    1.4919961415081806,
            1.8854012268195584,      -0.41867335506936448,    1.4549886554763136,      1.8550795598099479,
            -0.39278622013675712,    1.4183107146625968,      1.8246466083646666,      -0.36718262807007956,
            1.3820707893382493,      1.7941997254488722,      -0.34194983393013972,    1.3463671297693427,
            1.7638271394429743,      -0.31716623881434702,    1.311289426249566,       1.7336131602499452,
            -0.29290308972833917,    1.2769204691284517,      1.703641709842816,       -0.26922519939177975,
            1.2433385572909916,      1.6740014583021761,      -0.24619035497768835,    1.210622083972672,
            1.6447955005133721,      -0.22384622586405092,    1.1788584407099003,      1.6161577304704366,
            -0.20222346463070379,    1.148159033676539,       1.5882752079434734,      -0.18132492809849177,
            1.1186796971263935,      1.5614108616406244,      -0.16111357127495607,    1.0906413574393254,
            1.5359163975388874,      -0.14150435455128185,    1.0643418654422354,      1.5122251942948925,
            -0.1223662545549494,     1.0401499993200831,      1.4908213721789294,      -0.10353762270221029,
            1.0184785176214364,      1.4721916932478558,      -0.084852500073558892,   0.99974244214126518,
            1.4567752628956356,      -0.066170219214407883,   0.98431582661532613,     1.4449266161929581,
            -0.047398911979264186,   0.97250039752200312,     1.4369003145741166,      -0.028506126355068349,
            0.96451269954371399,     1.4328546002007283,      -0.0095145744070903714,  0.9604873105835714,
            1.4328546002007325,      0.009514574407090675,    0.9604873105835714,      1.4369003145741146,
            0.028506126355068766,    0.96451269954371377,     1.4449266161929539,      0.047398911979264657,
            0.9725003975220019,      1.4567752628956325,      0.066170219214409576,    0.98431582661532335,
            1.4721916932478503,      0.084852500073564555,    0.99974244214125618,     1.490821372178899,
            0.10353762270222792,     1.0184785176214057,      1.512225194294782,       0.12236625455500889,
            1.0401499993199799,      1.5359163975385091,      0.14150435455148053,     1.0643418654418886,
            1.5614108616393474,      0.1611135712756131,      1.0906413574381595,      1.5882752079392146,
            0.18132492810064177,     1.1186796971225037,      1.6161577304563641,      0.20222346463767552,
            1.1481590336636784,      1.6447955004673245,      0.22384622588645015,     1.1788584406677711,
            1.6740014581529219,      0.24619035504896597,     1.2106220838359172,      1.7036417093635885,
            0.26922519961639152,     1.2433385568512121,      1.7336131587260069,      0.29290309042918505,
            1.2769204677276029,      1.7638271346440306,      0.31716624097946605,     1.3112894218302684,
            1.7941997104850789,      0.34194984055225808,     1.3463671159628441,      1.8246465621634915,
            0.36718264812361628,     1.3820707466233753,      1.8550794185474877,      0.39278628027346119,
            1.4183105837784973,      1.8854007989835124,      0.4186735337144683,      1.4549882581721825,
            1.9154922612340448,      0.44474492677993327,     1.4919949461572817,      1.9451888223846192,
            0.47088137443035638,     1.5292087880342873,      1.9742306885841578,      0.49692993674266756,
            1.5664907147753506,      2.0021866029676536,      0.52267989329160147,     1.6036778255569903,
            2.0283542225777103,      0.54782927871257614,     1.6405740400180842,      2.0516616845984972,
            0.57194802533474942,     1.6769392504001752,      2.0706146771479359,      0.59445162210373226,
            1.7124804303362862,      2.0833433030752762,      0.61460498209043968,     1.7468502116580378,
            2.0877908550522872,      0.6315751212443349,      1.7796588848745916,      2.0820477011888592,
            0.64454001106559577,     1.8105033846995473,      2.0647766964637757,      0.65284091835324076,
            1.8390115350580269,      2.0356239339363364,      0.65614373733121301,     1.8648933161555918,
            1.9954866654647552,      0.65456165075799555,     1.8879861019963664,      1.9465357482767609,
            0.6486953773896571,      1.9082804169921623,      1.8919591170249948,      0.63956944291929874,
            1.9259176447817434,      1.8354800777369022,      0.62847548695630706,     1.9411596011233374,
            1.7807742366561365,      0.61676318105153771,     1.9543384793000562,      1.7309339171600517,
            0.60563379128271044,     1.9658009274821626,      1.6881022776196775,      0.59598601418038855,
            1.9758601677545682,      1.653336646552704,       0.58834266097562049,     1.9847656986446407,
            1.6266897183133782,      0.58286008348576501,     1.9926935391637899,      1.6074440401041041,
            0.57940029251328307,     1.9997538856217738,      1.5944131392372038,      0.57763478274167401,
            2.0060093804271775,      1.5862305348362302,      0.57714981573575119,     2.0114964897594922,
            1.5815745779105104,      0.57753171576491036,     2.0162441042029728,      1.5793086405382393,
            0.57842238608037566,     2.0202861661488911,      1.5785418577822576,      0.57954535995969447,
            2.0236677039715785,      1.5786304030550515,      0.58070894346304147,     2.0264453977113899,
            1.5791433940799546,      0.58179515641368451,     2.0286845364711268,      1.5798142914671072,
            0.58274233387872365,     2.0304541522231663,      1.5804920797948037,      0.58352691590927153,
            2.0318215666389632,      1.5810997564173972,      0.58414737071329215,     2.0328468625312603,
            1.5816024027828757,      0.58461107275243229,     2.0335770575995453,      1.5819838188379787,
            0.5849234982603817,      2.0340390516754789,      1.5822290323571186,      0.58507818911100751,
            2.0342296791351311,      1.5823093280069866,      0.58504531697685225,     2.0341003079172473,
            1.5821661711523807,      0.58475608688342984,     2.033532239829178,       1.5816900793704474,
            0.58407943795580719,     2.0322975357876487,      1.5806898552438733,      0.58278640639662316,
            2.0299977239188691,      1.5788465796474089,      0.58049617000928955,     2.0259702948184266,
            1.5756456709022213,      0.57659670615040948,     2.019150830453921,       1.5702802619732577,
            0.57013383442315912,     2.0078799130690439,      1.5615232347177228,      0.5596695850393365,
            1.989656198863706,       1.5475808825588477,      0.54313439073904335,     1.9608775723847265,
            1.5259834472808385,      0.51775661011432761,     1.9167140540061662,      1.4936595833933037,
            0.48027107707924527,     1.8514600879017977,      1.4474906879906702,      0.42777099074502567,
            1.7599943997875562,      1.3857408046189987,      0.35957608961084353,     1.6409907446919481,
            1.3103950052215085,      0.27977329745970625,     1.5012969190500374,      1.2290385725359598,
            0.19825491546884244,     1.3577960828638163,      1.1533690491311199,      0.12726326486794629,
            1.231694946632331,       1.0934730078914803,      0.074729785223835637,    1.1372574100302058,
            1.0526935076165664,      0.040982819811075981,    1.0758204891008185,      1.0280828924623446,
            0.021459836039747677,    1.0399010326222926,      1.0144264454398266,      0.010910154434093764,
            1.0203518834346268,      1.0072412807218425,      0.00543914677486568,     1.010176875805129,
            1.0035853778734434,      0.0026687135362074691,   1.0050290806497879,      1.001772808907248,
            0.0012808311162860191,   1.0024842114992132,      1.0009088783081277,      0.00057737196828583782,
            1.0012729617122325,      1.0006280611243779,      0.00024873614355938508,  1.0008794416251166};
  } // ... expected_end_state__impermeable_walls_by_direct_euler_treatment(...)

  void impermeable_walls_by_inviscid_mirror_treatment()
  {
    ASSERT_NE(grid_layer_, nullptr);
    ASSERT_NE(numerical_flux_, nullptr);
    ASSERT_NE(space_, nullptr);

    // impermeable walls everywhere
    XT::Grid::NormalBasedBoundaryInfo<XT::Grid::extract_intersection_t<GL>> boundary_info;
    boundary_info.register_new_normal({-1}, new XT::Grid::ImpermeableBoundary());
    boundary_info.register_new_normal({1}, new XT::Grid::ImpermeableBoundary());
    XT::Grid::ApplyOn::CustomBoundaryIntersections<GL> impermeable_wall_filter(boundary_info,
                                                                               new XT::Grid::ImpermeableBoundary());

    // create operator, the layer is periodic and the operator includes handling of periodic boundaries so we need to
    // make
    // an exception for all non-periodic boundaries
    op_ = std::make_shared<Op>(
        *grid_layer_, *numerical_flux_, /*periodicity_restriction=*/impermeable_wall_filter.copy());

    // the actual handling of impermeable walls, see [DF2015, p. 415, (8.66 - 8.67)]
    const auto inviscid_mirror_impermeable_wall_treatment = [&](const auto& intersection,
                                                                const auto& x_intersection,
                                                                const auto& /*f*/,
                                                                const auto& u,
                                                                const auto& /*mu*/ = {}) {
      const auto normal = intersection.unitOuterNormal(x_intersection);
      const auto rho = euler_tools_.density_from_conservative(u);
      auto velocity = euler_tools_.velocity_from_conservative(u);
      velocity -= normal * 2. * (velocity * normal);
      const auto pressure = euler_tools_.pressure_from_conservative(u);
      return euler_tools_.to_conservative(XT::Common::hstack(rho, velocity, pressure));
    };
    op_->append(inviscid_mirror_impermeable_wall_treatment, impermeable_wall_filter.copy(), {});

    // do timestepping
    this->do_the_timestepping(300); // we need enough for the wave to hit the boundary
    ASSERT_NE(time_stepper_, nullptr);

    // check expected state at the end
    V expected_end_state(expected_end_state__impermeable_walls_by_inviscid_mirror_treatment());

    const auto& actual_end_state = time_stepper_->solution().rbegin()->second.vector();
    EXPECT_LT((expected_end_state - actual_end_state).sup_norm(), 1e-15) << "actual_end_state = "
                                                                         << print_vector(actual_end_state);
  } // ... impermeable_walls_by_inviscid_mirror_treatment(...)

  static std::vector<double> expected_end_state__impermeable_walls_by_inviscid_mirror_treatment()
  {
    return {2.4128067906611528,      -0.00018257469025065507, 3.5075118863543633,     2.4123659455862563,
            -0.00053957586498921408, 3.5073761917146977,      2.4118269036469493,     -0.00087746957133593166,
            3.5069987288711197,      2.410961666019066,       -0.0012041793667654226, 3.5061431938961953,
            2.4097568919715733,      -0.0015714690666170667,  3.5043871019390425,     2.4085417491612024,
            -0.0021216332749881481,  3.5010174294429679,      2.4084810909172729,     -0.0031732734337611796,
            3.4948443807004748,      2.4122767915436629,      -0.005379370790090301,  3.4838676585176165,
            2.4244285204223526,      -0.010027846562987931,   3.4646643853444288,     2.4498545666617986,
            -0.01962436633520773,    3.4312616114276575,      2.4896977499720712,     -0.038950473573872338,
            3.3732081878468017,      2.5346335815003078,      -0.076510730321447645,  3.2731223581006939,
            2.5595863830031407,      -0.14468876100542485,    3.106832816246897,      2.5294972304816907,
            -0.25311068783569479,    2.8558573354479417,      2.4275382061225055,     -0.39058397807621636,
            2.540540945547205,       2.2892660814012058,      -0.51654695003671325,   2.2383194698438289,
            2.1797700684398431,      -0.59571302936625214,    2.0239171293262164,     2.124716713647274,
            -0.62946891209330846,    1.9016583022689886,      2.1047485408038091,     -0.63600127136320994,
            1.8333962396545462,      2.0960167125678235,      -0.62852417763962343,   1.7877561978543117,
            2.0862640771031313,      -0.61352360221156943,    1.7496774717556349,     2.0716409997212408,
            -0.59406947869086901,    1.713462285479332,       2.0520199138225728,     -0.57181322270929913,
            1.6772791467593859,      2.028478562202384,       -0.54778183465602992,   1.6406913274238673,
            2.0022295167300892,      -0.52266325311805895,    1.6037181303872274,     1.9742454068178883,
            -0.49692412888669268,    1.5665044910836972,      1.9451938346356983,     -0.47087935992596675,
            1.5292134659703798,      1.9154939546823291,      -0.4447442331813361,    1.491996522561555,
            1.885401366220161,       -0.41867329688704447,    1.4549887849345744,     1.8550796068132194,
            -0.39278620013528243,    1.4183107582143717,      1.8246466240546984,     -0.36718262126253759,
            1.3820708038448655,      1.7941997306335038,      -0.34194983163658588,   1.3463671345531609,
            1.7638271411388557,      -0.31716623804950039,    1.3112894278113327,     1.7336131607990592,
            -0.29290308947589316,    1.276920469633227,       1.7036417100188315,     -0.2692251993093101,
            1.2433385574525204,      1.6740014583580374,      -0.24619035495102179,   1.2106220840238544,
            1.6447955005309256,      -0.22384622585551509,    1.1788584407259608,     1.6161577304758992,
            -0.20222346462799895,    1.1481590336815302,      1.5882752079451565,     -0.18132492809764292,
            1.1186796971279305,      1.5614108616411384,      -0.16111357127469175,   1.0906413574397944,
            1.535916397539042,       -0.14150435455120039,    1.0643418654423771,     1.5122251942949396,
            -0.12236625455492472,    1.0401499993201249,      1.4908213721789427,     -0.10353762270220282,
            1.0184785176214488,      1.4721916932478598,      -0.084852500073556603,  0.99974244214126906,
            1.4567752628956356,      -0.066170219214407161,   0.98431582661532735,    1.4449266161929593,
            -0.047398911979263873,   0.97250039752200346,     1.4369003145741175,     -0.028506126355068374,
            0.96451269954371466,     1.4328546002007307,      -0.0095145744070905692, 0.96048731058357195,
            1.4328546002007245,      0.0095145744070903471,   0.9604873105835714,     1.4369003145741155,
            0.02850612635506827,     0.96451269954371399,     1.4449266161929579,     0.047398911979264623,
            0.97250039752200224,     1.4567752628956347,      0.066170219214409631,   0.98431582661532335,
            1.4721916932478474,      0.084852500073564679,    0.99974244214125663,    1.4908213721788977,
            0.10353762270222827,     1.0184785176214057,      1.5122251942947831,     0.12236625455500971,
            1.0401499993199808,      1.5359163975385086,      0.14150435455148097,    1.0643418654418886,
            1.5614108616393469,      0.16111357127561315,     1.0906413574381588,     1.5882752079392137,
            0.18132492810064221,     1.118679697122503,       1.616157730456363,      0.20222346463767543,
            1.148159033663678,       1.6447955004673247,      0.22384622588645023,    1.1788584406677709,
            1.6740014581529212,      0.24619035504896597,     1.2106220838359174,     1.7036417093635885,
            0.26922519961639174,     1.2433385568512123,      1.7336131587260069,     0.29290309042918516,
            1.2769204677276029,      1.7638271346440311,      0.31716624097946577,    1.3112894218302684,
            1.7941997104850784,      0.34194984055225769,     1.3463671159628441,     1.8246465621634911,
            0.36718264812361612,     1.3820707466233753,      1.8550794185474875,     0.39278628027346069,
            1.4183105837784975,      1.8854007989835115,      0.41867353371446753,    1.4549882581721816,
            1.9154922612340439,      0.44474492677993294,     1.4919949461572823,     1.9451888223846201,
            0.47088137443035655,     1.5292087880342879,      1.9742306885841594,     0.49692993674266789,
            1.5664907147753511,      2.0021866029676523,      0.52267989329160125,    1.6036778255569895,
            2.0283542225777089,      0.5478292787125757,      1.6405740400180835,     2.0516616845984958,
            0.5719480253347502,      1.6769392504001743,      2.0706146771479341,     0.5944516221037327,
            1.7124804303362855,      2.0833433030752744,      0.61460498209044001,    1.7468502116580384,
            2.0877908550522868,      0.63157512124433479,     1.7796588848745913,     2.0820477011888583,
            0.644540011065596,       1.8105033846995462,      2.0647766964637753,     0.65284091835324043,
            1.8390115350580261,      2.035623933936336,       0.65614373733121323,    1.8648933161555916,
            1.9954866654647561,      0.65456165075799633,     1.8879861019963669,     1.9465357482767607,
            0.64869537738965755,     1.9082804169921612,      1.8919591170249961,     0.63956944291929951,
            1.9259176447817439,      1.8354800777369016,      0.62847548695630739,    1.9411596011233379,
            1.7807742366561377,      0.6167631810515386,      1.9543384793000564,     1.7309339171600522,
            0.60563379128271011,     1.9658009274821631,      1.6881022776196797,     0.59598601418038821,
            1.9758601677545717,      1.6533366465527082,      0.58834266097562105,    1.9847656986446414,
            1.6266897183133799,      0.5828600834857659,      1.992693539163791,      1.6074440401041039,
            0.5794002925132824,      1.9997538856217738,      1.5944131392372027,     0.57763478274167379,
            2.0060093804271775,      1.58623053483623,        0.57714981573575086,    2.0114964897594922,
            1.581574577910511,       0.57753171576491102,     2.0162441042029733,     1.5793086405382395,
            0.57842238608037633,     2.0202861661488889,      1.5785418577822594,     0.57954535995969547,
            2.0236677039715789,      1.578630403055052,       0.58070894346304103,    2.0264453977113894,
            1.5791433940799544,      0.58179515641368462,     2.0286845364711272,     1.5798142914671063,
            0.5827423338787231,      2.0304541522231658,      1.5804920797948039,     0.58352691590927186,
            2.0318215666389627,      1.5810997564173959,      0.58414737071329181,    2.032846862531259,
            1.5816024027828754,      0.58461107275243296,     2.0335770575995453,     1.5819838188379798,
            0.58492349826038226,     2.0340390516754789,      1.5822290323571193,     0.58507818911100773,
            2.0342296791351315,      1.5823093280069906,      0.58504531697685147,    2.0341003079172513,
            1.5821661711523898,      0.58475608688342684,     2.0335322398291877,     1.5816900793704736,
            0.5840794379557972,      2.032297535787686,       1.5806898552439568,     0.58278640639658386,
            2.029997723918993,       1.5788465796477018,      0.58049617000915421,    2.0259702948188578,
            1.5756456709032114,      0.57659670614995273,     2.0191508304553731,     1.5702802619765277,
            0.57013383442164334,     2.0078799130738321,      1.5615232347282526,     0.55966958503440722,
            1.9896561988791013,      1.5475808825918491,      0.54313439072332015,    1.9608775724328698,
            1.5259834473812059,      0.51775661006521256,     1.9167140541521073,     1.4936595836881379,
            0.4802710769291727,      1.8514600883284826,      1.4474906888227415,     0.42777099029784044,
            1.7599944009838366,      1.3857408068635573,      0.35957608831896104,    1.6409907478928605,
            1.3103950109966358,      0.27977329387337357,     1.5012969272172938,     1.2290385867599494,
            0.19825490598568793,     1.3577961028505121,      1.1533690829858327,     0.1272632410445515,
            1.2316949940492479,      1.0934730866037754,      0.074729727977721233,   1.1372575201866817,
            1.0526936876657702,      0.040982686681057465,    1.0758207411003571,     1.0280832988267206,
            0.021459533395688565,    1.0399016014685327,      1.0144273511234425,     0.010909477941803235,
            1.0203531513495976,      1.0072432747605273,      0.0054376556175654723,  1.0101796674529628,
            1.0035897164903345,      0.0026654675760835384,   1.0050351548073106,     1.001782141886429,
            0.0012738471707738569,   1.002497278180533,       1.0009287369217688,     0.00056251190729067187,
            1.0013007699017782,      1.0005909086926197,      0.00015832581657331679, 1.000827421420597};
  } // ... expected_end_state__impermeable_walls_by_inviscid_mirror_treatment(...)

  void inflow_from_the_left_by_heuristic_euler_treatment_impermeable_wall_right()
  {
    ASSERT_NE(grid_layer_, nullptr);
    ASSERT_NE(numerical_flux_, nullptr);
    ASSERT_NE(space_, nullptr);

    // inflow/outflow left, impermeable wall right
    XT::Grid::NormalBasedBoundaryInfo<XT::Grid::extract_intersection_t<GL>> boundary_info;
    boundary_info.register_new_normal({-1}, new XT::Grid::InflowOutflowBoundary());
    boundary_info.register_new_normal({1}, new XT::Grid::ImpermeableBoundary());
    XT::Grid::ApplyOn::CustomBoundaryIntersections<GL> impermeable_wall_filter(boundary_info,
                                                                               new XT::Grid::ImpermeableBoundary());
    XT::Grid::ApplyOn::CustomBoundaryIntersections<GL> inflow_outflow_filter(boundary_info,
                                                                             new XT::Grid::InflowOutflowBoundary());

    // create operator, the layer is periodic and the operator includes handling of periodic boundaries so we need to
    // make
    // an exception for all non-periodic boundaries
    op_ =
        std::make_shared<Op>(*grid_layer_, *numerical_flux_, /*periodicity_restriction=*/inflow_outflow_filter.copy());

    // define timedependent inflow/outflow boundary values
    const U periodic_density_variation(
        [&](const auto& /*xx*/, const auto& mu) {
          FieldVector<R, m> primitive_variables(0.);
          const auto t = mu.get("t_").at(0);
          // density
          primitive_variables[0] = 1. + 0.5 * std::sin(2. * M_PI * (t - 0.25));
          // velocity
          for (size_t ii = 0; ii < d; ++ii)
            primitive_variables[1 + ii] = 0.25 + 0.25 * std::sin(2. * M_PI * (t - 0.25));
          // pressure
          primitive_variables[m - 1] = 0.4;
          return euler_tools_.to_conservative(primitive_variables);
        },
        /*order=*/0,
        /*parameter_type=*/{"t_", 1},
        /*name=*/"periodic_density_variation");
    // overwrite initial values
    project(U(
                [&](const auto& /*xx*/, const auto& /*mu*/) {
                  FieldVector<R, m> primitive_variables(0.);
                  // density
                  primitive_variables[0] = 0.5;
                  // velocity
                  for (size_t ii = 0; ii < d; ++ii)
                    primitive_variables[1 + ii] = 0.;
                  // pressure
                  primitive_variables[m - 1] = 0.4;
                  return euler_tools_.to_conservative(primitive_variables);
                },
                /*order=*/0,
                /*parameter_type=*/{},
                /*name=*/""),
            *initial_values_);

    // the actual handling of inflow/outflow, see [DF2015, p. 421, (8.88)]
    // (supposedly this does not work well for slow flows!)
    const auto heuristic_euler_inflow_outflow_treatment = [&](
        const auto& intersection, const auto& x_intersection, const auto& /*f*/, const auto& u, const auto& mu = {}) {
      // evaluate boundary values
      const auto entity = intersection.inside();
      const auto x_entity = intersection.geometryInInside().global(x_intersection);
      const RangeType bv = periodic_density_variation.local_function(entity)->evaluate(x_entity, mu);
      // determine flow regime
      const auto a = euler_tools_.speed_of_sound_from_conservative(u);
      const auto velocity = euler_tools_.velocity_from_conservative(u);
      const auto normal = intersection.unitOuterNormal(x_intersection);
      const auto flow_speed = velocity * normal;
      // compute v
      if (flow_speed < -a) {
        // supersonic inlet
        return bv;
      } else if (!(flow_speed > 0)) {
        // subsonic inlet
        const auto rho_outer = euler_tools_.density_from_conservative(bv);
        const auto v_outer = euler_tools_.velocity_from_conservative(bv);
        const auto p_inner = euler_tools_.pressure_from_conservative(u);
        return euler_tools_.to_conservative(XT::Common::hstack(rho_outer, v_outer, p_inner));
      } else if (flow_speed < a) {
        // subsonic outlet
        const auto rho_inner = euler_tools_.density_from_conservative(u);
        const auto v_inner = euler_tools_.velocity_from_conservative(u);
        const auto p_outer = euler_tools_.pressure_from_conservative(bv);
        return euler_tools_.to_conservative(XT::Common::hstack(rho_inner, v_inner, p_outer));
      } else {
        // supersonic outlet
        return RangeType(u);
      }
    }; // ... heuristic_euler_inflow_outflow_treatment(...)
    op_->append(heuristic_euler_inflow_outflow_treatment, inflow_outflow_filter.copy(), {});

    // the actual handling of impermeable walls, see [DF2015, p. 415, (8.66 - 8.67)]
    const auto inviscid_mirror_impermeable_wall_treatment = [&](const auto& intersection,
                                                                const auto& x_intersection,
                                                                const auto& /*f*/,
                                                                const auto& u,
                                                                const auto& /*mu*/ = {}) {
      const auto normal = intersection.unitOuterNormal(x_intersection);
      const auto rho = euler_tools_.density_from_conservative(u);
      auto velocity = euler_tools_.velocity_from_conservative(u);
      velocity -= normal * 2. * (velocity * normal);
      const auto pressure = euler_tools_.pressure_from_conservative(u);
      return euler_tools_.to_conservative(XT::Common::hstack(rho, velocity, pressure));
    };
    op_->append(inviscid_mirror_impermeable_wall_treatment, impermeable_wall_filter.copy(), {});

    // do timestepping
    this->do_the_timestepping(300, 0.0024452850605123986, 1., {{0.5, 0., 0.4}, {1.5, 0.5, 0.4}});
    ASSERT_NE(time_stepper_, nullptr);

    // check expected state at the end
    V expected_end_state(
        expected_end_state__inflow_from_the_left_by_heuristic_euler_treatment_impermeable_wall_right());

    const auto& actual_end_state = time_stepper_->solution().rbegin()->second.vector();
    EXPECT_LT((expected_end_state - actual_end_state).sup_norm(), 1e-15) << "actual_end_state = "
                                                                         << print_vector(actual_end_state);
  } // ... inflow_from_the_left_by_heuristic_euler_treatment_impermeable_wall_right(...)

  static std::vector<double>
  expected_end_state__inflow_from_the_left_by_heuristic_euler_treatment_impermeable_wall_right()
  {
    return {1.1009118973633434,      0.35330249410648068,     1.4732830984568579,      1.1528295046402057,
            0.39351888721419487,     1.5271089670477516,      1.1963674667694724,      0.4310102575107661,
            1.5801186255762272,      1.2338037112033473,      0.46611202209041547,     1.6318285053294048,
            1.2659945247710827,      0.49873585938714832,     1.6819019952027696,      1.2926406817944998,
            0.52842412370511005,     1.7299367921142861,      1.3124041615447823,      0.55430879609217054,
            1.7754164185418981,      1.3230579260721504,      0.57511713691656619,     1.8176881204010453,
            1.3218702012380497,      0.58931121642033002,     1.8559940286922998,      1.3063074876478125,
            0.59540933404349183,     1.889573542848326,       1.274953974405266,       0.59244905919578528,
            1.9178262522679761,      1.2283534732043544,      0.58044694123895779,     1.9404872935329662,
            1.1694024160837084,      0.56065467009137904,     1.9577441454205602,      1.1030428756888055,
            0.53545939583137325,     1.9702356249195334,      1.035289426164856,       0.50791979278674648,
            1.9789199980060443,      0.97192175094953237,     0.48109368523598067,     1.9848557974144547,
            0.91731440205618819,     0.45740237549473334,     1.9889742764461675,      0.87378323669504732,
            0.43824346845489659,     1.9919184792701508,      0.84157494050205128,     0.42393748367005879,
            1.9939871489647392,      0.8193634459277005,      0.41395234361198568,     1.9951755605649688,
            0.80497183894162028,     0.4072646467013149,      1.9952733964349643,      0.79604530283777386,
            0.40271155335473108,     1.9939726079495295,      0.79050961827123123,     0.39923968467029169,
            1.9909513850831044,      0.78677962513132305,     0.39602554253192285,     1.9859212591484101,
            0.78377261812475107,     0.39249202335558087,     1.978641408517352,       0.78081421528657957,
            0.38826548393707616,     1.9689122430401578,      0.77751268652663563,     0.38311351423660339,
            1.9565603525822062,      0.77364748246910242,     0.3768883000500125,      1.9414228601360284,
            0.76908903903740922,     0.36948533459855687,     1.9233347559699963,      0.76374893284839285,
            0.36081752382070131,     1.9021197982813507,      0.75755221147879925,     0.35080074293026509,
            1.8775843600286302,      0.75042332168935078,     0.33934667545987346,     1.8495137374894906,
            0.74227952771358718,     0.32636021587727165,     1.8176714274927479,      0.73302862110206579,
            0.31174057326583826,     1.7818035129562562,      0.72257017002411217,     0.29538701102349274,
            1.7416525794260496,      0.71080148956981359,     0.27721180687406594,     1.6969884268364226,
            0.69763104445998225,     0.2571642854162508,      1.6476654472790284,      0.6830028437412996,
            0.23526978147932917,     1.5937165528681008,      0.66693449800650384,     0.21168429275412737,
            1.5354863561090466,      0.64956715206332305,     0.18675705773034484,     1.4737861730404609,
            0.63121601464394772,     0.16107900272828329,     1.4100190941293984,      0.61239765375553912,
            0.13548121676978261,     1.3461891225994962,      0.593803604577538,       0.11094977027671478,
            1.2847104313849813,      0.57620293324689065,     0.088456248786680652,    1.228007749482394,
            0.56029330547756695,     0.068758094013782828,    1.1780287518342782,      0.54655892838193221,
            0.052256124113481274,    1.1358758434121634,      0.53519794571784207,     0.038969738531927728,
            1.1017128665416849,      0.52614163355940946,     0.0286211075461545,      1.0749428775382575,
            0.51913731313135303,     0.020767999559807498,    1.0545221752562521,      0.5138460793153582,
            0.01492416088339227,     1.039260788216088,       0.50991966215516382,     0.01063755820644093,
            1.0280279462402258,      0.50704516800763766,     0.007526559558001117,    1.0198543036838899,
            0.50496295767673682,     0.0052874790227797706,   1.013959869095912,       0.50346796637654989,
            0.0036873825319877788,   1.0097414369157862,      0.50240319707008119,     0.0025516060280478129,
            1.0067439452043401,      0.50165080623606495,     0.0017509788676654161,   1.0046293550588179,
            0.50112347230547649,     0.0011907975087860653,   1.0031490212400551,      0.50075707259308821,
            0.0008020424408123507,   1.0021213068093537,      0.50050485931893141,     0.00053466424077653794,
            1.0014142773516008,      0.50033298152175465,     0.00035255665831436837,  1.0009326414716553,
            0.50021710143822451,     0.00022982775202697262,  1.0006080092056961,      0.50013986012434863,
            0.00014804286833330256,  1.000391660530624,       0.50008898785166045,     9.4187606313609248e-05,
            1.0002491872067794,      0.50005589991330124,     5.9163549814854394e-05,  1.0001565281710296,
            0.50003465718610851,     3.6679510128642371e-05,  1.0000970433708212,      0.50002120089446056,
            2.2437591147216144e-05,  1.0000593637265589,      0.50001279345975336,     1.3539571454165479e-05,
            1.0000358221345438,      0.50000761385460635,     8.0578339383196815e-06,  1.0000213189521017,
            0.50000446814920585,     4.7286749510812885e-06,  1.0000125108728719,      0.50000258519570362,
            2.7359241662090618e-06,  1.0000072385665286,      0.50000147451416443,     1.5604824416932356e-06,
            1.0000041286457182,      0.50000082899311338,     8.7732490630837905e-07,  1.0000023211826501,
            0.50000045937192428,     4.8615387430160871e-07,  1.0000012862419818,      0.500000250877638,
            2.6550403290674893e-07,  1.0000007024575626,      0.50000013502750529,     1.4289970993991296e-07,
            1.0000003780770712,      0.50000007161924098,     7.579468884605042e-08,   1.000000200533891,
            0.50000003743463151,     3.9617091786446708e-08,  1.0000001048169707,      0.50000001928178273,
            2.0405920008947353e-08,  1.0000000539889893,      0.50000000978695491,     1.0357538663497255e-08,
            1.000000027403471,       0.50000000489524898,     5.1806457488363833e-09,  1.0000000137067,
            0.50000000241286091,     2.5535306549620701e-09,  1.0000000067560066,      0.50000000117199062,
            1.2403188968585907e-09,  1.0000000032815752,      0.50000000056099259,     5.9370130116855074e-10,
            1.0000000015707859,      0.50000000026463487,     2.8006191140957485e-10,  1.0000000007409744,
            0.50000000012302537,     1.301972558974602e-10,   1.0000000003444696,      0.50000000005636758,
            5.9651755568724845e-11,  1.000000000157824,       0.50000000002545142,     2.6935954133312421e-11,
            1.0000000000712657,      0.50000000001132616,     1.1987792187225288e-11,  1.0000000000317171,
            0.50000000000497247,     5.2584557553564567e-12,  1.0000000000139129,      0.5000000000021495,
            2.2733964122793859e-12,  1.0000000000060154,      0.50000000000091382,     9.6883596440898041e-13,
            1.0000000000025639,      0.50000000000038458,     4.0696114446242613e-13,  1.0000000000010769,
            0.50000000000015965,     1.684573284720014e-13,   1.0000000000004456,      0.50000000000006373,
            6.8708628275233809e-14,  1.0000000000001819,      0.50000000000002776,     2.7460864076117602e-14,
            1.0000000000000731,      0.5000000000000091,      1.0798435320029793e-14,  1.0000000000000291,
            0.50000000000000577,     4.1786382855465251e-15,  1.0000000000000115,      0.50000000000000056,
            1.5637315829488035e-15,  1.000000000000004,       0.49999999999999845,     3.4749590732195643e-16,
            1.0000000000000013,      0.50000000000000144,     6.9499181464391299e-17,  1.0000000000000002,
            0.49999999999999822,     -1.3031096524573359e-16, 1.0000000000000000,      0.50000000000000078,
            3.6977854932234928e-32,  1.0000000000000000,      0.49999999999999956,     -1.7374795366097817e-16,
            0.99999999999999978,     0.50000000000000078,     -3.0405891890671181e-16, 0.99999999999999978,
            0.4999999999999985,      -1.4768576061183139e-16, 0.99999999999999989,     0.50000000000000167,
            -1.9112274902707594e-16, 1.0000000000000000,      0.49999999999999911,     -2.9537152122366282e-16,
            0.99999999999999967,     0.499999999999999,       -2.6062193049146723e-16, 1.0000000000000000,
            0.49999999999999822,     -2.1718494207622277e-16, 0.99999999999999989,     0.50000000000000311,
            -3.4749590732195631e-17, 0.99999999999999978,     0.50000000000000033,     -3.4749590732195576e-17,
            1.0000000000000002,      0.499999999999999,       -2.6062193049146683e-17, 1.0000000000000002,
            0.49999999999999889,     -2.6062193049146702e-17, 1.0000000000000002,      0.50000000000000155,
            1.7374795366097831e-17,  1.0000000000000000,      0.49999999999999928,     6.9499181464391287e-17,
            1.0000000000000000,      0.49999999999999856,     -5.212438609829344e-17,  1.0000000000000000,
            0.50000000000000111,     -6.949918146439125e-17,  1.0000000000000000,      0.5000000000000000,
            4.343698841524453e-17,   1.0000000000000000,      0.49999999999999978,     -3.4749590732195631e-17,
            1.0000000000000002,      0.49999999999999784,     -1.7374795366097775e-17, 1.0000000000000002,
            0.49999999999999928,     1.5637315829488032e-16,  1.0000000000000002,      0.50000000000000067,
            1.7374795366097822e-16,  1.0000000000000000,      0.50000000000000056,     5.2124386098293471e-17,
            1.0000000000000000,      0.50000000000000033,     -6.0811783781342376e-17, 1.0000000000000000,
            0.4999999999999985,      -6.9499181464391213e-17, 1.0000000000000000,      0.50000000000000044,
            -6.9499181464391262e-17, 1.0000000000000000,      0.50000000000000033,     -1.0424877219658691e-16,
            1.0000000000000002,      0.4999999999999995,      5.2124386098293447e-17,  1.0000000000000002};
  } // ... expected_end_state__inflow_from_the_left_by_heuristic_euler_treatment_impermeable_wall_right(...)

  static std::string print_vector(const V& vec)
  {
    if (vec.size() == 0)
      return "{}";
    std::stringstream stream;
    stream << "{" << std::setprecision(17) << vec[0];
    for (size_t ii = 1; ii < vec.size(); ++ii)
      stream << ", " << std::setprecision(17) << vec[ii];
    stream << "}" << std::endl;
    return stream.str();
  } // ... print_vector(...)

  XT::Common::TimedLogManager logger_;
  EulerTools<d> euler_tools_;
  std::shared_ptr<GridProvider> grid_;
  std::shared_ptr<LeafGL> leaf_grid_layer_;
  std::shared_ptr<GL> grid_layer_;
  std::shared_ptr<S> space_;
  std::shared_ptr<DF> initial_values_;
  std::shared_ptr<F> flux_;
  std::shared_ptr<NF> numerical_flux_;
  std::shared_ptr<Op> op_;
  std::shared_ptr<TimeStepper> time_stepper_;
}; // class InviscidCompressibleFlowEuler1dExplicitTest


} // namespace GDT
} // namespace Dune

#endif // DUNE_GDT_TEXT_INVISCID_COMPRESSIBLE_FLOW_EULER_1D_EXPLICIT_HH
