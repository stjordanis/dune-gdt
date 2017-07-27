// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2017 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2016 - 2017)
//   Rene Milk       (2016 - 2017)
//   Tobias Leibner  (2016)

#ifndef DUNE_GDT_LOCAL_FLUXES_LAXFRIEDRICHS_HH
#define DUNE_GDT_LOCAL_FLUXES_LAXFRIEDRICHS_HH

#include <tuple>
#include <memory>

#include <dune/xt/functions/interfaces.hh>
#include <dune/xt/functions/type_traits.hh>

#include <dune/gdt/operators/fv/eigensolver.hh>

#include "interfaces.hh"
#include "godunov.hh"

namespace Dune {
namespace GDT {


// forwards
template <class AnalyticalFluxImp,
          class LocalizableFunctionImp,
          class EigenSolverImp = DefaultEigenSolver<typename AnalyticalFluxImp::RangeFieldType,
                                                    AnalyticalFluxImp::dimRange,
                                                    AnalyticalFluxImp::dimRangeCols>>
class LaxFriedrichsLocalNumericalCouplingFlux;

template <class AnalyticalFluxImp,
          class BoundaryValueImp,
          class LocalizableFunctionImp,
          class EigenSolverImp = DefaultEigenSolver<typename AnalyticalFluxImp::RangeFieldType,
                                                    AnalyticalFluxImp::dimRange,
                                                    AnalyticalFluxImp::dimRangeCols>>
class LaxFriedrichsLocalDirichletNumericalBoundaryFlux;

template <class AnalyticalFluxImp,
          class LocalizableFunctionImp,
          class EigenSolverImp = DefaultEigenSolver<typename AnalyticalFluxImp::RangeFieldType,
                                                    AnalyticalFluxImp::dimRange,
                                                    AnalyticalFluxImp::dimRangeCols>>
class LaxFriedrichsLocalAbsorbingNumericalBoundaryFlux;


namespace internal {


template <class AnalyticalFluxImp, class LocalizableFunctionImp, class EigenSolverImp>
class LaxFriedrichsLocalNumericalCouplingFluxTraits
    : public GodunovLocalNumericalCouplingFluxTraits<AnalyticalFluxImp, EigenSolverImp>
{
  static_assert(Dune::XT::Functions::is_localizable_function<LocalizableFunctionImp>::value,
                "LocalizableFunctionImp has to be derived from XT::Functions::is_localizable_function.");
  typedef GodunovLocalNumericalCouplingFluxTraits<AnalyticalFluxImp, EigenSolverImp> BaseType;

public:
  typedef LocalizableFunctionImp LocalizableFunctionType;
  typedef typename LocalizableFunctionType::LocalfunctionType LocalfunctionType;
  using typename BaseType::AnalyticalFluxLocalfunctionType;
  typedef std::tuple<std::shared_ptr<AnalyticalFluxLocalfunctionType>, std::shared_ptr<LocalfunctionType>>
      LocalfunctionTupleType;
  static_assert(LocalizableFunctionType::dimRangeCols == 1, "Not implemented for dimRangeCols > 1!");
  typedef LaxFriedrichsLocalNumericalCouplingFlux<AnalyticalFluxImp, LocalizableFunctionType, EigenSolverImp>
      derived_type;
}; // class LaxFriedrichsLocalNumericalCouplingFluxTraits

template <class AnalyticalFluxImp, class BoundaryValueImp, class LocalizableFunctionImp, class EigenSolverImp>
class LaxFriedrichsLocalDirichletNumericalBoundaryFluxTraits
    : public LaxFriedrichsLocalNumericalCouplingFluxTraits<AnalyticalFluxImp, LocalizableFunctionImp, EigenSolverImp>
{
  typedef LaxFriedrichsLocalNumericalCouplingFluxTraits<AnalyticalFluxImp, LocalizableFunctionImp, EigenSolverImp>
      BaseType;

public:
  using typename BaseType::LocalfunctionType;
  using typename BaseType::AnalyticalFluxLocalfunctionType;
  typedef BoundaryValueImp BoundaryValueType;
  typedef typename BoundaryValueType::LocalfunctionType BoundaryValueLocalfunctionType;
  typedef LaxFriedrichsLocalDirichletNumericalBoundaryFlux<AnalyticalFluxImp,
                                                           BoundaryValueImp,
                                                           LocalizableFunctionImp,
                                                           EigenSolverImp>
      derived_type;
  typedef std::tuple<std::shared_ptr<AnalyticalFluxLocalfunctionType>,
                     std::shared_ptr<LocalfunctionType>,
                     std::shared_ptr<BoundaryValueLocalfunctionType>>
      LocalfunctionTupleType;
}; // class LaxFriedrichsLocalDirichletNumericalBoundaryFluxTraits

template <class AnalyticalFluxImp, class LocalizableFunctionImp, class EigenSolverImp>
class LaxFriedrichsLocalAbsorbingNumericalBoundaryFluxTraits
    : public LaxFriedrichsLocalNumericalCouplingFluxTraits<AnalyticalFluxImp, LocalizableFunctionImp, EigenSolverImp>
{
public:
  typedef LaxFriedrichsLocalAbsorbingNumericalBoundaryFlux<AnalyticalFluxImp, LocalizableFunctionImp, EigenSolverImp>
      derived_type;
}; // class LaxFriedrichsLocalAbsorbingNumericalBoundaryFluxTraits

template <class Traits>
class LaxFriedrichsFluxImplementation
{
public:
  typedef typename Traits::LocalizableFunctionType LocalizableFunctionType;
  typedef typename Traits::LocalfunctionTupleType LocalfunctionTupleType;
  typedef typename Traits::EntityType EntityType;
  typedef typename Traits::DomainFieldType DomainFieldType;
  typedef typename Traits::RangeFieldType RangeFieldType;
  typedef typename Traits::AnalyticalFluxType AnalyticalFluxType;
  typedef typename Traits::RangeType RangeType;
  typedef typename Traits::DomainType DomainType;
  typedef typename Traits::AnalyticalFluxLocalfunctionType AnalyticalFluxLocalfunctionType;
  typedef typename Traits::EigenSolverType EigenSolverType;
  typedef typename AnalyticalFluxLocalfunctionType::StateRangeType StateRangeType;
  static const size_t dimDomain = Traits::dimDomain;
  static const size_t dimRange = Traits::dimRange;
  typedef typename Dune::FieldVector<Dune::FieldMatrix<double, dimRange, dimRange>, dimDomain> JacobianRangeType;

  explicit LaxFriedrichsFluxImplementation(const AnalyticalFluxType& analytical_flux,
                                           XT::Common::Parameter param,
                                           const bool use_local,
                                           const bool is_linear,
                                           const RangeFieldType alpha,
                                           const DomainType lambda,
                                           const bool boundary)
    : analytical_flux_(analytical_flux)
    , param_inside_(param)
    , param_outside_(param)
    , dt_(param.get("dt")[0])
    , use_local_(use_local)
    , is_linear_(is_linear)
    , alpha_(alpha)
    , lambda_(lambda)
    , lambda_provided_(XT::Common::FloatCmp::ne(lambda_, DomainType(0)))
  {
    param_inside_.set("boundary", {0.}, true);
    param_outside_.set("boundary", {double(boundary)}, true);
    if (lambda_provided_ && use_local_)
      std::cerr << "WARNING: Parameter lambda in Lax-Friedrichs flux set but will have no effect because local "
                   "Lax-Friedrichs flux is requested."
                << std::endl;
    if (is_instantiated_)
      DUNE_THROW(InvalidStateException,
                 "This class uses several static variables to save its state between time "
                 "steps, so using several instances at the same time may result in undefined "
                 "behavior!");
    is_instantiated_ = true;
  }

  ~LaxFriedrichsFluxImplementation()
  {
    is_instantiated_ = false;
  }

  template <class IntersectionType>
  RangeType evaluate(const LocalfunctionTupleType& local_functions_tuple_entity,
                     const LocalfunctionTupleType& local_functions_tuple_neighbor,
                     const IntersectionType& intersection,
                     const Dune::FieldVector<DomainFieldType, dimDomain - 1>& x_in_intersection_coords,
                     const DomainType& x_in_inside_coords,
                     const DomainType& x_in_outside_coords,
                     const RangeType& u_i,
                     const RangeType& u_j) const
  {
    // find direction of unit outer normal
    size_t direction = intersection.indexInInside() / 2;

    const auto& local_flux_inside = std::get<0>(local_functions_tuple_entity);
    const auto& local_flux_outside = std::get<0>(local_functions_tuple_neighbor);
    auto n_ij = intersection.unitOuterNormal(x_in_intersection_coords);

    if (use_local_) {
      if (!is_linear_ || !(max_derivative_calculated_[direction])) {
        if (!jacobian_inside_) {
          jacobian_inside_ = XT::Common::make_unique<JacobianRangeType>();
          jacobian_outside_ = XT::Common::make_unique<JacobianRangeType>();
        }
        DomainFieldType max_derivative(0);
        helper<dimDomain>::get_jacobian(
            direction, local_flux_inside, x_in_inside_coords, u_i, *jacobian_inside_, param_inside_);
        helper<dimDomain>::get_jacobian(
            direction, local_flux_outside, x_in_outside_coords, u_j, *jacobian_outside_, param_outside_);
        const auto eigen_solver_inside = EigenSolverType((*jacobian_inside_)[direction], false);
        const auto eigen_solver_outside = EigenSolverType((*jacobian_outside_)[direction], false);
        const auto& eigenvalues_inside = eigen_solver_inside.eigenvalues();
        const auto& eigenvalues_outside = eigen_solver_outside.eigenvalues();
        for (size_t jj = 0; jj < dimRange; ++jj)
          max_derivative =
              std::max({std::abs(eigenvalues_inside[jj]), std::abs(eigenvalues_outside[jj]), max_derivative});
        lambda_ij_[direction] = 1. / max_derivative;
        if (is_linear_) {
          jacobian_inside_ = nullptr;
          jacobian_outside_ = nullptr;
        }
        max_derivative_calculated_[direction] = true;
      }
    } else if (lambda_provided_) {
      lambda_ij_[direction] = lambda_[direction];
    } else {
      const RangeFieldType dx = std::get<1>(local_functions_tuple_entity)->evaluate(x_in_inside_coords)[0];
      lambda_ij_[direction] = dt_ / dx;
    } // if (use_local)

    // calculate flux evaluation as
    // ret[kk] = (f_u_i[kk] + f_u_j[kk])*n_ij*0.5 - (u_j - u_i)[kk]*1.0/(num_neighbors*lambda_ij)
    auto second_part = u_j;
    second_part -= u_i;
    second_part /= lambda_ij_[direction] * 2 * alpha_;
    auto ret = local_flux_inside->evaluate_col(direction, x_in_inside_coords, u_i, param_inside_);
    ret += local_flux_outside->evaluate_col(direction, x_in_outside_coords, u_j, param_outside_);
    ret *= n_ij[direction] * 0.5;
    ret -= second_part;
    return ret;
  } // ... evaluate(...)

  const AnalyticalFluxType& analytical_flux() const
  {
    return analytical_flux_;
  }

private:
  template <size_t domainDim, class anything = void>
  struct helper
  {
    static void get_jacobian(const size_t direction,
                             const std::shared_ptr<AnalyticalFluxLocalfunctionType>& local_func,
                             const DomainType& x_in_inside_coords,
                             const StateRangeType& u,
                             JacobianRangeType& ret,
                             const XT::Common::Parameter& param)
    {
      local_func->partial_u_col(direction, x_in_inside_coords, u, ret[direction], param);
    }
  };

  template <class anything>
  struct helper<1, anything>
  {
    static void get_jacobian(const size_t direction,
                             const std::shared_ptr<AnalyticalFluxLocalfunctionType>& local_func,
                             const DomainType& x_in_inside_coords,
                             const StateRangeType& u,
                             JacobianRangeType& ret,
                             const XT::Common::Parameter& param)
    {
      assert(direction == 0);
      local_func->partial_u(x_in_inside_coords, u, ret[direction], param);
    }
  };

  const AnalyticalFluxType& analytical_flux_;
  XT::Common::Parameter param_inside_;
  XT::Common::Parameter param_outside_;
  const double dt_;
  const bool use_local_;
  const bool is_linear_;
  const RangeFieldType alpha_;
  const DomainType lambda_;
  const bool lambda_provided_;
  static thread_local DomainType lambda_ij_;
  static thread_local FieldVector<bool, dimDomain> max_derivative_calculated_;
  static thread_local std::unique_ptr<JacobianRangeType> jacobian_inside_;
  static thread_local std::unique_ptr<JacobianRangeType> jacobian_outside_;
  static bool is_instantiated_;
}; // class LaxFriedrichsFluxImplementation<...>

template <class Traits>
thread_local
    typename LaxFriedrichsFluxImplementation<Traits>::DomainType LaxFriedrichsFluxImplementation<Traits>::lambda_ij_;

template <class Traits>
thread_local FieldVector<bool, LaxFriedrichsFluxImplementation<Traits>::dimDomain>
    LaxFriedrichsFluxImplementation<Traits>::max_derivative_calculated_(false);

template <class Traits>
thread_local std::unique_ptr<typename LaxFriedrichsFluxImplementation<Traits>::JacobianRangeType>
    LaxFriedrichsFluxImplementation<Traits>::jacobian_inside_;

template <class Traits>
thread_local std::unique_ptr<typename LaxFriedrichsFluxImplementation<Traits>::JacobianRangeType>
    LaxFriedrichsFluxImplementation<Traits>::jacobian_outside_;

template <class Traits>
bool LaxFriedrichsFluxImplementation<Traits>::is_instantiated_(false);


} // namespace internal


/**
 *  \brief  Lax-Friedrichs flux evaluation for inner intersections and periodic boundary intersections.
 *
 *  The Lax-Friedrichs flux is an approximation to the integral
 *  \int_{S_{ij}} \mathbf{F}(\mathbf{u}) \cdot \mathbf{n}_{ij},
 *  where S_{ij} is the intersection between the entities i and j, \mathbf{F}(\mathbf{u}) is the analytical flux
 *  (evaluated at \mathbf{u}) and \mathbf{n}_{ij} is the unit outer normal of S_{ij}.
 *  The Lax-Friedrichs flux takes the form
 *  \mathbf{g}_{ij}^{LF}(\mathbf{u}_i, \mathbf{u}_j)
 *  = \int_{S_{ij}} \frac{1}{2}(\mathbf{F}(\mathbf{u}_i) + \mathbf{F}(\mathbf{u}_j) \cdot \mathbf{n}_{ij}
 *  - \frac{1}{\alpha_i \lambda_{ij}} (\mathbf{u}_j - \mathbf{u}_i),
 *  where \alpha_i is the number of neighbors (i.e. intersections) of the entity i and lambda_{ij} is a local
 *  constant fulfilling
 *  \lambda_{ij} \sup_{\mathbf{u}} (\mathbf{F}(\mathbf{u} \cdot \mathbf{n}_{ij})^\prime \leq 1.
 *  The integration is done numerically and implemented in the LocalCouplingFvOperator. This class implements
 *  the evaluation of the integrand. As we are restricting ourselves to axis-parallel cubic grids, only one component of
 *  \mathbf{n}_{ij} is non-zero, denote this component by k. Then the Lax-Friedrichs flux evaluation reduces to
 *  \frac{1}{2}(\mathbf{f}^k(\mathbf{u}_i) + \mathbf{f}^k(\mathbf{u}_j) n_{ij,k}
 *  - \frac{1}{\alpha_i \lambda_{ij}} (\mathbf{u}_j - \mathbf{u}_i),
 *  where \mathbf{f}^k is the k-th column of the analytical flux.
 *  For the classical Lax-Friedrichs flux, \lambda_{ij} is chosen as dt/dx_i, where dt is the current time
 *  step length and dx_i is the width of entity i. This fulfills the equation above as long as the CFL condition
 *  is fulfilled.
 *  The local Lax-Friedrichs flux can be chosen by setting \param use_local to true, here \lambda_{ij} is chosen
 *  as the inverse of the maximal eigenvalue of \mathbf{f}^k(\mathbf{u}_i) and \mathbf{f}^k(\mathbf{u}_j). In this
 *  case, you should also specify whether your analytical flux is linear by setting \param is_linear, which avoids
 *  recalculating the eigenvalues on every intersection in the linear case.
 *  You can also provide a user-defined \param lambda that is used as \lambda_{ij} on all intersections. You need to set
 *  use_local to false, otherwise lambda will not be used.
 */
template <class AnalyticalFluxImp, class LocalizableFunctionImp, class EigenSolverImp>
class LaxFriedrichsLocalNumericalCouplingFlux
    : public LocalNumericalCouplingFluxInterface<internal::
                                                     LaxFriedrichsLocalNumericalCouplingFluxTraits<AnalyticalFluxImp,
                                                                                                   LocalizableFunctionImp,
                                                                                                   EigenSolverImp>>
{
public:
  typedef internal::LaxFriedrichsLocalNumericalCouplingFluxTraits<AnalyticalFluxImp,
                                                                  LocalizableFunctionImp,
                                                                  EigenSolverImp>
      Traits;
  typedef typename Traits::LocalizableFunctionType LocalizableFunctionType;
  typedef typename Traits::LocalfunctionTupleType LocalfunctionTupleType;
  typedef typename Traits::EntityType EntityType;
  typedef typename Traits::DomainFieldType DomainFieldType;
  typedef typename Traits::RangeFieldType RangeFieldType;
  typedef typename Traits::AnalyticalFluxType AnalyticalFluxType;
  typedef typename Traits::RangeType RangeType;
  typedef typename LocalizableFunctionType::DomainType DomainType;
  static const size_t dimDomain = Traits::dimDomain;
  static const size_t dimRange = Traits::dimRange;

  explicit LaxFriedrichsLocalNumericalCouplingFlux(const AnalyticalFluxType& analytical_flux,
                                                   const XT::Common::Parameter& param,
                                                   const LocalizableFunctionType& dx,
                                                   const bool use_local = false,
                                                   const bool is_linear = false,
                                                   const RangeFieldType alpha = dimDomain,
                                                   const DomainType lambda = DomainType(0))
    : dx_(dx)
    , implementation_(analytical_flux, param, use_local, is_linear, alpha, lambda, false)
  {
  }

  LocalfunctionTupleType local_functions(const EntityType& entity) const
  {
    return std::make_tuple(implementation_.analytical_flux().local_function(entity), dx_.local_function(entity));
  }

  template <class IntersectionType>
  RangeType evaluate(
      const LocalfunctionTupleType& local_functions_tuple_entity,
      const LocalfunctionTupleType& local_functions_tuple_neighbor,
      const XT::Functions::LocalfunctionInterface<EntityType, DomainFieldType, dimDomain, RangeFieldType, dimRange, 1>&
          local_source_entity,
      const XT::Functions::LocalfunctionInterface<EntityType, DomainFieldType, dimDomain, RangeFieldType, dimRange, 1>&
          local_source_neighbor,
      const IntersectionType& intersection,
      const Dune::FieldVector<DomainFieldType, dimDomain - 1>& x_in_intersection_coords) const
  {
    const auto x_in_inside_coords = intersection.geometryInInside().global(x_in_intersection_coords);
    const auto x_in_outside_coords = intersection.geometryInOutside().global(x_in_intersection_coords);
    const RangeType u_i = local_source_entity.evaluate(x_in_inside_coords);
    const RangeType u_j = local_source_neighbor.evaluate(x_in_outside_coords);
    return implementation_.evaluate(local_functions_tuple_entity,
                                    local_functions_tuple_neighbor,
                                    intersection,
                                    x_in_intersection_coords,
                                    x_in_inside_coords,
                                    x_in_outside_coords,
                                    u_i,
                                    u_j);
  } // RangeType evaluate(...) const

private:
  const LocalizableFunctionType& dx_;
  const internal::LaxFriedrichsFluxImplementation<Traits> implementation_;
}; // class LaxFriedrichsLocalNumericalCouplingFlux

/**
*  \brief  Lax-Friedrichs flux evaluation for Dirichlet boundary intersections.
*  \see    LaxFriedrichsLocalNumericalCouplingFlux
*/
template <class AnalyticalFluxImp, class BoundaryValueImp, class LocalizableFunctionImp, class EigenSolverImp>
class LaxFriedrichsLocalDirichletNumericalBoundaryFlux
    : public LocalNumericalBoundaryFluxInterface<internal::
                                                     LaxFriedrichsLocalDirichletNumericalBoundaryFluxTraits<AnalyticalFluxImp,
                                                                                                            BoundaryValueImp,
                                                                                                            LocalizableFunctionImp,
                                                                                                            EigenSolverImp>>
{
public:
  typedef internal::LaxFriedrichsLocalDirichletNumericalBoundaryFluxTraits<AnalyticalFluxImp,
                                                                           BoundaryValueImp,
                                                                           LocalizableFunctionImp,
                                                                           EigenSolverImp>
      Traits;
  typedef typename Traits::BoundaryValueType BoundaryValueType;
  typedef typename Traits::LocalizableFunctionType LocalizableFunctionType;
  typedef typename Traits::LocalfunctionTupleType LocalfunctionTupleType;
  typedef typename Traits::EntityType EntityType;
  typedef typename Traits::DomainFieldType DomainFieldType;
  typedef typename Traits::RangeFieldType RangeFieldType;
  typedef typename Traits::AnalyticalFluxType AnalyticalFluxType;
  typedef typename Traits::RangeType RangeType;
  typedef typename Traits::DomainType DomainType;
  static const size_t dimDomain = Traits::dimDomain;
  static const size_t dimRange = Traits::dimRange;

  explicit LaxFriedrichsLocalDirichletNumericalBoundaryFlux(const AnalyticalFluxType& analytical_flux,
                                                            const BoundaryValueType& boundary_values,
                                                            const XT::Common::Parameter& param,
                                                            const LocalizableFunctionType& dx,
                                                            const bool use_local = false,
                                                            const bool is_linear = false,
                                                            const RangeFieldType alpha = dimDomain,
                                                            const DomainType lambda = DomainType(0))
    : boundary_values_(boundary_values)
    , dx_(dx)
    , implementation_(analytical_flux, param, use_local, is_linear, alpha, lambda, true)
  {
  }

  LocalfunctionTupleType local_functions(const EntityType& entity) const
  {
    return std::make_tuple(implementation_.analytical_flux().local_function(entity),
                           dx_.local_function(entity),
                           boundary_values_.local_function(entity));
  }

  template <class IntersectionType>
  RangeType evaluate(
      const LocalfunctionTupleType& local_functions_tuple,
      const XT::Functions::LocalfunctionInterface<EntityType, DomainFieldType, dimDomain, RangeFieldType, dimRange, 1>&
          local_source_entity,
      const IntersectionType& intersection,
      const Dune::FieldVector<DomainFieldType, dimDomain - 1>& x_in_intersection_coords) const

  {
    const auto x_in_inside_coords = intersection.geometryInInside().global(x_in_intersection_coords);
    const RangeType u_i = local_source_entity.evaluate(x_in_inside_coords);
    const RangeType u_j = std::get<2>(local_functions_tuple)->evaluate(x_in_inside_coords);
    return implementation_.evaluate(local_functions_tuple,
                                    local_functions_tuple,
                                    intersection,
                                    x_in_intersection_coords,
                                    x_in_inside_coords,
                                    x_in_inside_coords,
                                    u_i,
                                    u_j);
  } // RangeType evaluate(...) const

private:
  const BoundaryValueType& boundary_values_;
  const LocalizableFunctionType& dx_;
  const internal::LaxFriedrichsFluxImplementation<Traits> implementation_;
}; // class LaxFriedrichsLocalDirichletNumericalBoundaryFlux

/**
 *  \brief  Lax-Friedrichs flux evaluation for absorbing boundary conditions on boundary intersections.
 *  \see    LaxFriedrichsLocalNumericalCouplingFlux
 */
template <class AnalyticalFluxImp, class LocalizableFunctionImp, class EigenSolverImp>
class LaxFriedrichsLocalAbsorbingNumericalBoundaryFlux
    : public LocalNumericalBoundaryFluxInterface<internal::
                                                     LaxFriedrichsLocalAbsorbingNumericalBoundaryFluxTraits<AnalyticalFluxImp,
                                                                                                            LocalizableFunctionImp,
                                                                                                            EigenSolverImp>>
{
public:
  typedef internal::LaxFriedrichsLocalAbsorbingNumericalBoundaryFluxTraits<AnalyticalFluxImp,
                                                                           LocalizableFunctionImp,
                                                                           EigenSolverImp>
      Traits;
  typedef typename Traits::LocalizableFunctionType LocalizableFunctionType;
  typedef typename Traits::LocalfunctionTupleType LocalfunctionTupleType;
  typedef typename Traits::EntityType EntityType;
  typedef typename Traits::DomainFieldType DomainFieldType;
  typedef typename Traits::RangeFieldType RangeFieldType;
  typedef typename Traits::AnalyticalFluxType AnalyticalFluxType;
  typedef typename Traits::DomainType DomainType;
  typedef typename Traits::RangeType RangeType;
  static const size_t dimDomain = Traits::dimDomain;
  static const size_t dimRange = Traits::dimRange;

  explicit LaxFriedrichsLocalAbsorbingNumericalBoundaryFlux(const AnalyticalFluxType& analytical_flux,
                                                            const XT::Common::Parameter param,
                                                            const LocalizableFunctionType& dx,
                                                            const bool use_local = false,
                                                            const bool is_linear = false,
                                                            const RangeFieldType alpha = dimDomain,
                                                            const DomainType lambda = DomainType(0))
    : dx_(dx)
    , implementation_(analytical_flux, param, use_local, is_linear, alpha, lambda, false)
  {
  }

  LocalfunctionTupleType local_functions(const EntityType& entity) const
  {
    return std::make_tuple(
        implementation_.analytical_flux().local_function(entity), entity.subEntities(1), dx_.local_function(entity));
  }

  template <class IntersectionType>
  RangeType evaluate(
      const LocalfunctionTupleType& local_functions_tuple_entity,
      const XT::Functions::LocalfunctionInterface<EntityType, DomainFieldType, dimDomain, RangeFieldType, dimRange, 1>&
          local_source_entity,
      const IntersectionType& intersection,
      const Dune::FieldVector<DomainFieldType, dimDomain - 1>& x_in_intersection_coords) const
  {
    const auto x_in_inside_coords = intersection.geometryInInside().global(x_in_intersection_coords);
    const RangeType u_i = local_source_entity.evaluate(x_in_inside_coords);
    return implementation_.evaluate(local_functions_tuple_entity,
                                    local_functions_tuple_entity,
                                    intersection,
                                    x_in_intersection_coords,
                                    x_in_inside_coords,
                                    x_in_inside_coords,
                                    u_i,
                                    u_i);
  } // void evaluate(...) const

private:
  const LocalizableFunctionType& dx_;
  const internal::LaxFriedrichsFluxImplementation<Traits>& implementation_;
}; // class LaxFriedrichsLocalAbsorbingNumericalBoundaryFlux


} // namespace GDT
} // namespace Dune

#endif // DUNE_GDT_LOCAL_FLUXES_LAXFRIEDRICHS_HH
