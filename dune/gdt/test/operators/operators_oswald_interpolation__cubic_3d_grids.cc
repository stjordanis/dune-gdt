// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2018 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2019)

#include <dune/xt/common/test/main.hxx> // <- this one has to come first (includes the config.h)!

#include <dune/xt/grid/grids.hh>

#include "oswald-interpolation.hh"


using Cubic3dGrids = ::testing::Types<YASP_3D_EQUIDISTANT_OFFSET
#if HAVE_DUNE_ALUGRID
                                      ,
                                      ALU_3D_CUBE
#endif
#if HAVE_DUNE_UGGRID || HAVE_UG
                                      ,
                                      UG_3D
#endif
                                      >;


template <class G>
using OswaldInterpolationOperator = Dune::GDT::Test::OswaldInterpolationOperatorOnCubicLeafViewTest<G>;
TYPED_TEST_CASE(OswaldInterpolationOperator, Cubic3dGrids);
TYPED_TEST(OswaldInterpolationOperator, applies_correctly_on_cubic_grids)
{
  this->applies_correctly_on_cubic_grids();
}
TYPED_TEST(OswaldInterpolationOperator, fulfills_l2_interpolation_estimate)
{
  this->fulfills_l2_interpolation_estimate();
}
TYPED_TEST(OswaldInterpolationOperator, fulfills_h1_interpolation_estimate)
{
  this->fulfills_h1_interpolation_estimate();
}