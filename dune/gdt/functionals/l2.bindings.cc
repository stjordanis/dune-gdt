// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2017 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2017)

#include "config.h"

#if HAVE_DUNE_PYBINDXI

#include <dune/common/parallel/mpihelper.hh>

#if HAVE_DUNE_FEM
#include <dune/fem/misc/mpimanager.hh>
#endif

#include <dune/pybindxi/pybind11.h>
#include <dune/pybindxi/stl.h>

#include <dune/xt/common/bindings.hh>

#include <dune/gdt/functionals/l2.bindings.hh>


#define DUNE_GDT_FUNCTIONALS_L2_BIND(_m, _d, _GRID, _layer, _g_backend, _s_type, _s_backend, _p, _la)                  \
  Dune::GDT::bindings::                                                                                                \
      L2VolumeVectorFunctional<Dune::XT::Functions::                                                                   \
                                   LocalizableFunctionInterface<Dune::XT::Grid::extract_entity_t<                      \
                                                                    typename Dune::XT::Grid::                          \
                                                                        Layer<_GRID,                                   \
                                                                              Dune::XT::Grid::Layers::_layer,          \
                                                                              Dune::XT::Grid::Backends::_g_backend,    \
                                                                              Dune::XT::Grid::DD::                     \
                                                                                  SubdomainGrid<_GRID>>::type>,        \
                                                                double,                                                \
                                                                _d,                                                    \
                                                                double,                                                \
                                                                1,                                                     \
                                                                1>,                                                    \
                               Dune::GDT::SpaceProvider<_GRID,                                                         \
                                                        Dune::XT::Grid::Layers::_layer,                                \
                                                        Dune::GDT::SpaceType::_s_type,                                 \
                                                        Dune::GDT::Backends::_s_backend,                               \
                                                        _p,                                                            \
                                                        double,                                                        \
                                                        1,                                                             \
                                                        1>,                                                            \
                               typename Dune::XT::LA::Container<double,                                                \
                                                                Dune::XT::LA::Backends::_la>::VectorType>::bind(_m);   \
  Dune::GDT::bindings::                                                                                                \
      L2FaceVectorFunctional<Dune::XT::Functions::                                                                     \
                                 LocalizableFunctionInterface<Dune::XT::Grid::extract_entity_t<                        \
                                                                  typename Dune::XT::Grid::                            \
                                                                      Layer<_GRID,                                     \
                                                                            Dune::XT::Grid::Layers::_layer,            \
                                                                            Dune::XT::Grid::Backends::_g_backend,      \
                                                                            Dune::XT::Grid::DD::                       \
                                                                                SubdomainGrid<_GRID>>::type>,          \
                                                              double,                                                  \
                                                              _d,                                                      \
                                                              double,                                                  \
                                                              1,                                                       \
                                                              1>,                                                      \
                             Dune::GDT::SpaceProvider<_GRID,                                                           \
                                                      Dune::XT::Grid::Layers::_layer,                                  \
                                                      Dune::GDT::SpaceType::_s_type,                                   \
                                                      Dune::GDT::Backends::_s_backend,                                 \
                                                      _p,                                                              \
                                                      double,                                                          \
                                                      1,                                                               \
                                                      1>,                                                              \
                             typename Dune::XT::LA::Container<double, Dune::XT::LA::Backends::_la>::VectorType,        \
                             typename Dune::XT::Grid::Layer<_GRID,                                                     \
                                                            Dune::XT::Grid::Layers::_layer,                            \
                                                            Dune::XT::Grid::Backends::_g_backend,                      \
                                                            Dune::XT::Grid::DD::SubdomainGrid<_GRID>>::type>::bind(_m)


PYBIND11_PLUGIN(__functionals_l2)
{
  namespace py = pybind11;
  using namespace pybind11::literals;

  py::module m("__functionals_l2", "dune-gdt: l2 functionals");
  DUNE_XT_COMMON_BINDINGS_INITIALIZE(m, "dune.gdt.functionsl.l2");

  using G = ALU_2D_SIMPLEX_CONFORMING;

  DUNE_GDT_FUNCTIONALS_L2_BIND(m, 2, G, leaf, part, dg, fem, 1, istl_sparse);
  DUNE_GDT_FUNCTIONALS_L2_BIND(m, 2, G, leaf, part, dg, fem, 2, istl_sparse);
  DUNE_GDT_FUNCTIONALS_L2_BIND(m, 2, G, leaf, part, dg, fem, 3, istl_sparse);
  DUNE_GDT_FUNCTIONALS_L2_BIND(m, 2, G, dd_subdomain, part, dg, fem, 1, istl_sparse);

  return m.ptr();
}

#endif // HAVE_DUNE_PYBINDXI
