// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2016 dune-gdt developers and contributors. All rights reserved.
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
// Authors:
//   Felix Schindler (2015 - 2016)


#include <dune/grid/yaspgrid.hh>

#include "../problems/ESV2007.hh"
#include "../eocexpectations.hh"


namespace Dune {
namespace GDT {
namespace Test {

template <>
class LinearEllipticEocExpectations<LinearElliptic::ESV2007TestCase<Yasp2Grid, double, 1>,
                                    LinearElliptic::ChooseDiscretizer::swipdg, 1>
    : public internal::LinearEllipticEocExpectationsBase<1>
{
  typedef LinearElliptic::ESV2007TestCase<Yasp2Grid, double, 1> TestCaseType;

public:
  static std::vector<double> results(const TestCaseType& /*test_case*/, const std::string type); // ... results(...)
}; // LinearEllipticEocExpectations

template <>
class LinearEllipticEocExpectations<LinearElliptic::ESV2007TestCase<Yasp2Grid, double, 1>,
                                    LinearElliptic::ChooseDiscretizer::swipdg, 2>
    : public internal::LinearEllipticEocExpectationsBase<2>
{
  typedef LinearElliptic::ESV2007TestCase<Yasp2Grid, double, 1> TestCaseType;

public:
  static std::vector<double> results(const TestCaseType& /*test_case*/, const std::string type); // ... results(...)
}; // LinearEllipticEocExpectations


} // namespace Test
} // namespace GDT
} // namespace Dune
