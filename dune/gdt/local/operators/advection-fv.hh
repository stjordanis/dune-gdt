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

#include <dune/xt/common/memory.hh>
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
class LocalAdvectionFvCouplingOperator;


namespace internal {


template <class SpaceType>
class LocalAdvectionFvCouplingOperatorTraits
{
  static_assert(is_fv_space<SpaceType>::value, "Use LocalAdvectionDgInnerOperator instead!");

public:
  using derived_type = LocalAdvectionFvCouplingOperator<SpaceType>;
};


} // namespace internal


template <class E, class D, size_t d, class R, size_t m>
class NumericalFluxInterface : public XT::Common::ParametricInterface
{
public:
  using StateType = XT::Functions::LocalizableFunctionInterface<E, D, d, R, m, 1>;
  using FluxType = XT::Functions::GlobalFluxFunctionInterface<E, D, d, StateType, 0, R, d, m>;
  using DomainType = typename StateType::DomainType;
  using RangeType = typename StateType::RangeType;

  NumericalFluxInterface(const FluxType& flx, const XT::Common::ParameterType& param_type = {})
    : XT::Common::ParametricInterface(param_type)
    , flux_(flx)
  {
  }

  NumericalFluxInterface(FluxType*&& flx_ptr, const XT::Common::ParameterType& param_type = {})
    : XT::Common::ParametricInterface(param_type)
    , flux_(flx_ptr)
  {
  }

  const FluxType& flux() const
  {
    return flux_.access();
  }

  virtual RangeType
  apply(const RangeType& u, const RangeType& v, const DomainType& n, const XT::Common::Parameter& mu = {}) const = 0;

private:
  const XT::Common::ConstStorageProvider<FluxType> flux_;
}; // class NumericalFluxInterface


template <class LF>
class NumericalLambdaFlux : public NumericalFluxInterface<typename LF::E, typename LF::D, LF::d, typename LF::R, LF::r>
{
  static_assert(XT::Functions::is_localizable_function<LF>::value, "");
  static_assert(LF::rC == 1, "");
  using BaseType = NumericalFluxInterface<typename LF::E, typename LF::D, LF::d, typename LF::R, LF::r>;

public:
  using typename BaseType::FluxType;
  using typename BaseType::DomainType;
  using typename BaseType::RangeType;

  using LambdaType =
      std::function<RangeType(const RangeType&, const RangeType&, const DomainType&, const XT::Common::Parameter&)>;

  NumericalLambdaFlux(const FluxType& flx, LambdaType lambda, const XT::Common::ParameterType& param_type = {})
    : BaseType(flx, param_type)
    , lambda_(lambda)
  {
  }

  RangeType apply(const RangeType& u,
                  const RangeType& v,
                  const DomainType& n,
                  const XT::Common::Parameter& mu = {}) const override final
  {
    return lambda_(u, v, n, this->parse_parameter(mu));
  }

private:
  const LambdaType lambda_;
}; // class NumericalLambdaFlux


/**
 * \note Presumes that the basis evaluates to 1.
 * \todo Improve local vector handling in apply.
 */
template <class SpaceType>
class LocalAdvectionFvCouplingOperator
    : public LocalCouplingOperatorInterface<internal::LocalAdvectionFvCouplingOperatorTraits<SpaceType>>
{
  static_assert(SpaceType::dimRangeCols == 1, "Not Implemented yet!");
  using E = typename SpaceType::EntityType;
  using D = typename SpaceType::DomainFieldType;
  static const constexpr size_t d = SpaceType::dimDomain;
  using R = typename SpaceType::RangeFieldType;
  static const constexpr size_t r = SpaceType::dimRange;
  static const constexpr size_t rC = 1;
  using StateType = XT::Functions::LocalizableFunctionInterface<E, D, d, R, r>;

public:
  using NumericalFluxType = NumericalFluxInterface<E, D, d, R, r>;

  LocalAdvectionFvCouplingOperator(const NumericalFluxType& numerical_flux)
    : numerical_flux_(numerical_flux)
  {
  }

  const XT::Common::ParameterType& parameter_type() const override final
  {
    return numerical_flux_.parameter_type();
  }

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
    const auto normal = intersection.centerUnitOuterNormal(/*x_intersection*/);
    // copy the local DoF vector to matching FieldVectors
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
    const auto g = numerical_flux_.apply(u, v, normal, mu);
    const auto h = local_range_entity.entity().geometry().volume();
    for (size_t ii = 0; ii < r; ++ii) {
      local_range_entity.vector().add(ii, (g[ii] * intersection.geometry().volume()) / h);
      local_range_neighbor.vector().add(ii, (-1.0 * g[ii] * intersection.geometry().volume()) / h);
    }
  } // ... apply(...)

private:
  const NumericalFluxType& numerical_flux_;
}; // class LocalAdvectionFvCouplingOperator


} // namespace GDT
} // namespace Dune

#endif // DUNE_GDT_LOCAL_OPERATORS_ADVECTION_FV_HH
