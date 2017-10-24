// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2017 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2017)

#ifndef DUNE_GDT_LOCAL_OPERATORS_ADVECTION_FV_HH
#define DUNE_GDT_LOCAL_OPERATORS_ADVECTION_FV_HH

#include <functional>
#include <type_traits>

#include <dune/common/typetraits.hh>
#include <dune/geometry/referenceelements.hh>

#include <dune/xt/grid/type_traits.hh>
#include <dune/xt/functions/interfaces/localizable-function.hh>
#include <dune/xt/functions/interfaces/localizable-flux-function.hh>

#include <dune/gdt/discretefunction/default.hh>
#include <dune/gdt/type_traits.hh>

#include "interfaces.hh"

namespace Dune {
namespace GDT {


// forward
template <class SpaceType>
class LocalAdvectionFvInnerOperator;


namespace internal {


template <class SpaceType>
class LocalAdvectionFvInnerOperatorTraits
{
  static_assert(is_fv_space<SpaceType>::value, "Use LocalAdvectionDgInnerOperator instead!");

public:
  using derived_type = LocalAdvectionFvInnerOperator<SpaceType>;
};


} // namespace internal


/**
 * \note Presumes that the basis evaluates to 1.
 */
template <class SpaceType>
class LocalAdvectionFvInnerOperator
    : public LocalCouplingOperatorInterface<internal::LocalAdvectionFvInnerOperatorTraits<SpaceType>>
{
  static_assert(SpaceType::dimRangeCols == 1, "Not Implemented yet!");
  using E = typename SpaceType::EntityType;
  using D = typename SpaceType::DomainFieldType;
  static const constexpr size_t d = SpaceType::dimDomain;
  using R = typename SpaceType::RangeFieldType;
  static const constexpr size_t r = SpaceType::dimRange;
  using StateType = XT::Functions::LocalizableFunctionInterface<E, D, d, R, r>;

public:
  using NumericalFluxType = std::function<typename StateType::RangeType(const typename StateType::RangeType&,
                                                                        const typename StateType::RangeType&,
                                                                        const typename StateType::DomainType&,
                                                                        const XT::Common::Parameter&)>;

  LocalAdvectionFvInnerOperator(NumericalFluxType numerical_flux)
    : numerical_flux_(numerical_flux)
    , parameter_type_("dt_", 1)
  {
  }

  bool is_parametric() const override final
  {
    return true;
  }

  const XT::Common::ParameterType& parameter_type() const override final
  {
    return parameter_type_;
  }

  // // scalar equation
  //  template <class VectorType, class I>
  //  void apply(const ConstDiscreteFunction<SpaceType, VectorType>& source,
  //             const I& intersection,
  //             LocalDiscreteFunction<SpaceType, VectorType>& local_range_entity,
  //             LocalDiscreteFunction<SpaceType, VectorType>& local_range_neighbor,
  //             const XT::Common::Parameter& mu = {}) const
  //  {
  //    static_assert(XT::Grid::is_intersection<I>::value, "");
  //    const auto& entity = local_range_entity.entity();
  //    const auto& neighbor = local_range_neighbor.entity();
  //    const auto normal = intersection.centerUnitOuterNormal();
  //    const auto u_inside = source.local_discrete_function(entity);
  //    const auto u_outside = source.local_discrete_function(neighbor);
  //    assert(u_inside->vector().size() == 1);
  //    assert(u_outside->vector().size() == 1);
  //    assert(local_range_entity.vector().size() == 1);
  //    assert(local_range_neighbor.vector().size() == 1);
  //    // If we do not assume that the local basis evaluates to 1 (but is still constant), we need to
  //    // * use u_inside->evaluate(x_entity) instead of u_inside->vector().get(0)
  //    // * use u_outside->evaluate(x_neighbor) instead of u_outside->vector().get(0)
  //    // * \int_entity basis^2 \dx instead of h
  //    // where x_entity and x_neighbor are the corresponding coordinates of the intersections midpoint.
  //    const auto g = numerical_flux_(u_inside->vector().get(0), u_outside->vector().get(0), normal, mu);
  //    const auto h = local_range_entity.entity().geometry().volume();
  //    for (size_t ii = 0; ii < r; ++ii) {
  //      local_range_entity.vector().add(ii, (g[ii] * intersection.geometry().volume()) / h);
  //      local_range_neighbor.vector().add(ii, (-1.0 * g[ii] * intersection.geometry().volume()) / h);
  //    }
  //  }

  // system of equations
  template <class VectorType, class I>
  void apply(const ConstDiscreteFunction<SpaceType, VectorType>& source,
             const I& intersection,
             LocalDiscreteFunction<SpaceType, VectorType>& local_range_entity,
             LocalDiscreteFunction<SpaceType, VectorType>& local_range_neighbor,
             const XT::Common::Parameter& mu = {}) const
  {
    static_assert(XT::Grid::is_intersection<I>::value, "");
    const auto& entity = local_range_entity.entity();
    const auto& neighbor = local_range_neighbor.entity();
    const auto u_inside = source.local_discrete_function(entity);
    const auto u_outside = source.local_discrete_function(neighbor);
    //    const auto x_intersection = QuadratureRules<D, d - 1>::rule(intersection.type(), 0).begin()->position();
    //    const auto x_entity = intersection.geometryInInside().global(x_intersection);
    //    const auto x_neighbor = intersection.geometryInOutside().global(x_intersection);
    const auto normal = intersection.centerUnitOuterNormal(/*x_intersection*/);
    typename StateType::RangeType u;
    typename StateType::RangeType v;
    if (u.size() != u_inside->vector().size())
      DUNE_THROW(InvalidStateException, "");
    if (v.size() != u_outside->vector().size())
      DUNE_THROW(InvalidStateException, "");
    for (size_t ii = 0; ii < u.size(); ++ii) {
      u[ii] = u_inside->vector().get(ii);
      v[ii] = u_outside->vector().get(ii);
    }
    const auto g = numerical_flux_(u, v, normal, mu);
    //    for (size_t ii = 0; ii < g.size(); ++ii)
    //      if (XT::Common::isnan(g[ii]) || XT::Common::isinf(g[ii]))
    //        DUNE_THROW(InvalidStateException, g);
    const auto h = local_range_entity.entity().geometry().volume();
    for (size_t ii = 0; ii < r; ++ii) {
      local_range_entity.vector().add(ii, (g[ii] * intersection.geometry().volume()) / h);
      local_range_neighbor.vector().add(ii, (-1.0 * g[ii] * intersection.geometry().volume()) / h);
    }
  }

private:
  const NumericalFluxType numerical_flux_;
  const XT::Common::ParameterType parameter_type_;
}; // class LocalAdvectionFvInnerOperator


} // namespace GDT
} // namespace Dune

#endif // DUNE_GDT_LOCAL_OPERATORS_ADVECTION_FV_HH
