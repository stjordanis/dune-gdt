// This file is part of the dune-gdt project:
//   http://users.dune-project.org/projects/dune-gdt
// Copyright holders: Felix Albrecht
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#ifndef DUNE_GDT_OPERATOR_PROLONGATIONS_HH
#define DUNE_GDT_OPERATOR_PROLONGATIONS_HH

#include <type_traits>
#include <vector>
#include <limits>

#include <dune/common/dynmatrix.hh>

#include <dune/stuff/functions/interfaces.hh>
#include <dune/stuff/grid/boundaryinfo.hh>
#include <dune/stuff/grid/intersection.hh>
#include <dune/stuff/common/vector.hh>

#include <dune/geometry/quadraturerules.hh>

#include <dune/stuff/grid/search.hh>

#include <dune/gdt/discretefunction/default.hh>
#include <dune/gdt/space/continuouslagrange/fem.hh>
#include <dune/gdt/space/continuouslagrange/pdelab.hh>
#include <dune/gdt/space/continuouslagrange/fem-localfunctions.hh>
#include <dune/gdt/playground/space/discontinuouslagrange/fem-localfunctions.hh>

namespace Dune {
namespace GDT {
namespace ProlongationOperator {


/**
 *  \note We would have liked to do something like this and match on implementations of SpaceInterface:\code
template< class T, class VS, class GPR, int pR, class RR, int rR, int rCR, class VR >
void apply(const ConstDiscreteFunction< SpaceInterface< T >, VS >& source,
           DiscreteFunction< DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper< GPR, pR, RR, rR, rCR >, VR >& range)
const
{
  static_assert((Dune::AlwaysFalse< T >::value), "Not implemented for this combination of source and range!");
}\endcode
 *        but that gave compile errors (the compiler just could not match the first argument for whatever reason). This
 *        is why we need all combinations of spaces below which are just compile time checks and forwards.
 */
template <class GridViewType>
class L2
{
  typedef typename GridViewType::template Codim<0>::Entity EntityType;
  typedef typename GridViewType::ctype DomainFieldType;
  static const unsigned int dimDomain = GridViewType::dimension;

public:
  L2(const GridViewType& grid_view)
    : grid_view_(grid_view)
  {
  }

  // Source: ContinuousLagrangeSpace::FemWrapper
  // Range:  DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void apply(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, RS, rS, rCS>, VS>& /*source*/,
      DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, int rC, class VS, class GPR, int pR, class VR>
  inline void
  apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, R, r, rC>, VS>& source,
        DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, R, r, rC>, VR>& range) const
  {
    prolong_onto_dg_fem_localfunctions_wrapper(source, range);
  }

  // Source: ContinuousLagrangeSpace::FemLocalfunctionsWrapper
  // Range:  DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void apply(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>,
                                  VS>& /*source*/,
      DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, int rC, class VS, class GPR, int pR, class VR>
  inline void
  apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, R, r, rC>, VS>& source,
        DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, R, r, rC>, VR>& range) const
  {
    prolong_onto_dg_fem_localfunctions_wrapper(source, range);
  }

  // Source: DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper
  // Range:  DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void apply(
      const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>,
                                  VS>& /*source*/,
      DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, int rC, class VS, class GPR, int pR, class VR>
  inline void apply(
      const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, R, r, rC>, VS>& source,
      DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, R, r, rC>, VR>& range) const
  {
    prolong_onto_dg_fem_localfunctions_wrapper(source, range);
  }

private:
  template <class SourceFunctionType, class RangeFunctionType>
  void prolong_onto_dg_fem_localfunctions_wrapper(const SourceFunctionType& source, RangeFunctionType& range) const
  {
    typedef typename RangeFunctionType::DomainType DomainType;
    typedef typename RangeFunctionType::RangeType RangeType;
    typedef typename RangeFunctionType::RangeFieldType RangeFieldType;
    // clear
    Stuff::Common::clear(range.vector());
    // create search in the source grid part
    typedef typename SourceFunctionType::SpaceType::GridViewType::GridViewType SourceGridViewType;
    typedef Stuff::Grid::EntityInlevelSearch<SourceGridViewType> EntitySearch;
    EntitySearch entity_search(source.space().gridPart()->gridView());
    // walk the grid
    RangeType source_value(0);
    std::vector<RangeType> basis_values(range.space().mapper().maxNumDofs());
    const auto entity_it_end = grid_view_.template end<0>();
    for (auto entity_it = grid_view_.template begin<0>(); entity_it != entity_it_end; ++entity_it) {
      // prepare
      const auto& entity     = *entity_it;
      const auto local_basis = range.space().baseFunctionSet(entity);
      auto local_range = range.local_discrete_function(entity);
      DynamicMatrix<RangeFieldType> local_matrix(local_basis.size(), local_basis.size(), RangeFieldType(0));
      DynamicVector<RangeFieldType> local_vector(local_basis.size(), RangeFieldType(0));
      // create quadrature
      const size_t integrand_order = local_range.order();
      assert(integrand_order < std::numeric_limits<int>::max());
      const auto& quadrature = QuadratureRules<DomainFieldType, dimDomain>::rule(entity.type(), int(integrand_order));
      // get global quadrature points
      std::vector<DomainType> quadrature_points;
      for (const auto& quadrature_point : quadrature)
        quadrature_points.emplace_back(entity.geometry().global(quadrature_point.position()));
      // get source entities
      const auto source_entity_ptr_unique_ptrs = entity_search(quadrature_points);
      assert(source_entity_ptr_unique_ptrs.size() >= quadrature_points.size());
      // loop over all quadrature points
      size_t pp = 0;
      for (const auto& quadrature_point : quadrature) {
        const auto local_point         = quadrature_point.position();
        const auto quadrature_weight   = quadrature_point.weight();
        const auto integration_element = entity.geometry().integrationElement(local_point);
        // evaluate source
        const auto& source_entity_ptr_unique_ptr = source_entity_ptr_unique_ptrs[pp];
        if (source_entity_ptr_unique_ptr) {
          const auto source_entity_ptr = *source_entity_ptr_unique_ptr;
          const auto& source_entity    = *source_entity_ptr;
          const auto local_source = source.local_function(source_entity);
          local_source->evaluate(source_entity.geometry().local(entity.geometry().global(local_point)), source_value);
        } else
          source_value *= 0.0;
        // evaluate
        local_basis.evaluate(local_point, basis_values);
        // compute integrals
        for (size_t ii = 0; ii < local_basis.size(); ++ii) {
          local_vector[ii] += integration_element * quadrature_weight * (source_value * basis_values[ii]);
          auto& local_matrix_row = local_matrix[ii];
          for (size_t jj = 0; jj < local_basis.size(); ++jj) {
            local_matrix_row[jj] += integration_element * quadrature_weight * (basis_values[ii] * basis_values[jj]);
          }
        }
        ++pp;
      } // loop over all quadrature points
      // compute local DoFs
      DynamicVector<RangeFieldType> local_DoFs(local_basis.size(), 0);
      local_matrix.solve(local_DoFs, local_vector);
      // set local DoFs
      auto local_range_vector = local_range.vector();
      assert(local_range_vector.size() == local_DoFs.size());
      for (size_t ii = 0; ii < local_range_vector.size(); ++ii)
        local_range_vector.set(ii, local_DoFs[ii]);
    } // walk the grid
  } // ... prolong_onto_dg_fem_localfunctions_wrapper(...)

  const GridViewType& grid_view_;
}; // class L2


/**
 *  \note We would have liked to do something like this and match on implementations of SpaceInterface:\code
template< class T, class VS, class GPR, int pR, class RR, int rR, int rCR, class VR >
void apply(const ConstDiscreteFunction< SpaceInterface< T >, VS >& source,
           DiscreteFunction< ContinuousLagrangeSpace::FemWrapper< GPR, pR, RR, rR, rCR >, VR >& range) const
{
  static_assert((Dune::AlwaysFalse< T >::value), "Not implemented for this combination of source and range!");
}\endcode
 *        but that gave compile errors (the compiler just could not match the first argument for whatever reason). This
 *        is why we need all combinations of spaces below which are just compile time checks and forwards.
 */
template <class GridViewType>
class Lagrange
{
public:
  typedef typename GridViewType::ctype DomainFieldType;
  static const unsigned int dimDomain = GridViewType::dimension;

  Lagrange(const GridViewType& grid_view)
    : grid_view_(grid_view)
  {
  }

  // Source: ContinuousLagrangeSpace::FemWrapper
  // Range:  ContinuousLagrangeSpace::FemWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, RS, rS, rCS>, VS>& /*source*/,
             DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, class VS, class GPR, int pR, class VR>
  inline void apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, R, r, 1>, VS>& source,
                    DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, R, r, 1>, VR>& range) const
  {
    redirect_to_appropriate_apply(source, range);
  }

  // Source: ContinuousLagrangeSpace::FemLocalfunctionsWrapper
  // Range:  ContinuousLagrangeSpace::FemWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>,
                                         VS>& /*source*/,
             DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, class VS, class GPR, int pR, class VR>
  inline void
  apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, R, r, 1>, VS>& source,
        DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, R, r, 1>, VR>& range) const
  {
    redirect_to_appropriate_apply(source, range);
  }

  // Source: DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper
  // Range:  ContinuousLagrangeSpace::FemWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void apply(const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>,
                                         VS>& /*source*/,
             DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, class VS, class GPR, int pR, class VR>
  inline void
  apply(const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, R, r, 1>, VS>& source,
        DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, R, r, 1>, VR>& range) const
  {
    redirect_to_appropriate_apply(source, range);
  }

  // Source: ContinuousLagrangeSpace::FemWrapper
  // Range:  ContinuousLagrangeSpace::FemLocalfunctionsWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void
  apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, RS, rS, rCS>, VS>& /*source*/,
        DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, class VS, class GPR, int pR, class VR>
  inline void
  apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, R, r, 1>, VS>& source,
        DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, R, r, 1>, VR>& range) const
  {
    redirect_to_appropriate_apply(source, range);
  }

  // Source: ContinuousLagrangeSpace::FemLocalfunctionsWrapper
  // Range:  ContinuousLagrangeSpace::FemLocalfunctionsWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void
  apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>,
                                    VS>& /*source*/,
        DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, class VS, class GPR, int pR, class VR>
  inline void
  apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, R, r, 1>, VS>& source,
        DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, R, r, 1>, VR>& range) const
  {
    redirect_to_appropriate_apply(source, range);
  }

  // Source: DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper
  // Range:  ContinuousLagrangeSpace::FemLocalfunctionsWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void
  apply(const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>,
                                    VS>& /*source*/,
        DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, class VS, class GPR, int pR, class VR>
  inline void
  apply(const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, R, r, 1>, VS>& source,
        DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, R, r, 1>, VR>& range) const
  {
    redirect_to_appropriate_apply(source, range);
  }

  // Source: ContinuousLagrangeSpace::PdelabWrapper
  // Range:  ContinuousLagrangeSpace::PdelabWrapper

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  void apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::PdelabWrapper<GPS, pS, RS, rS, rCS>, VS>& /*source*/,
             DiscreteFunction<ContinuousLagrangeSpace::PdelabWrapper<GPR, pR, RR, rR, rCR>, VR>& /*range*/) const
  {
    static_assert((Dune::AlwaysFalse<GPS>::value), "Not implemented for this combination of source and range!");
  }

  template <class GPS, int pS, class R, int r, int rC, class VS, class GPR, class VR>
  inline void apply(const ConstDiscreteFunction<ContinuousLagrangeSpace::PdelabWrapper<GPS, pS, R, r, rC>, VS>& source,
                    DiscreteFunction<ContinuousLagrangeSpace::PdelabWrapper<GPR, 1, R, r, rC>, VR>& range) const
  {
    redirect_to_appropriate_apply(source, range);
  }

private:
  template <class SourceType, class RangeType>
  void redirect_to_appropriate_apply(const SourceType& source, RangeType& range) const
  {
    // create search in the source grid part
    typedef typename SourceType::SpaceType::GridViewType SourceGridViewType;
    typedef Stuff::Grid::EntityInlevelSearch<SourceGridViewType> EntitySearch;
    EntitySearch entity_search(*(source.space().grid_view()));
    // set all range dofs to infinity
    const auto infinity = std::numeric_limits<typename RangeType::RangeFieldType>::infinity();
    for (size_t ii = 0; ii < range.vector().size(); ++ii)
      range.vector().set_entry(ii, infinity);
    // walk the grid
    const auto entity_it_end = grid_view_.template end<0>();
    for (auto entity_it = grid_view_.template begin<0>(); entity_it != entity_it_end; ++entity_it) {
      const auto& entity = *entity_it;
      // get global lagrange point coordinates
      const auto lagrange_point_set = range.space().lagrange_points(entity);
      typedef FieldVector<typename SourceGridViewType::ctype, SourceGridViewType::dimension> DomainType;
      std::vector<DomainType> lagrange_points(lagrange_point_set.size());
      for (size_t ii = 0; ii < lagrange_point_set.size(); ++ii)
        lagrange_points[ii] = entity.geometry().global(lagrange_point_set[ii]);
      // get source entities
      const auto source_entity_ptrs = entity_search(lagrange_points);
      assert(source_entity_ptrs.size() == lagrange_points.size());
      // get range
      auto local_range            = range.local_discrete_function(entity);
      auto local_range_DoF_vector = local_range.vector();
      // do the actual work (see below)
      apply_local(source, lagrange_points, source_entity_ptrs, local_range_DoF_vector);
    } // walk the grid
  } // ... redirect_to_appropriate_apply(...)

  template <class SourceType, class LagrangePointsType, class EntityPointers, class LocalDoFVectorType>
  void apply_local(const SourceType& source, const LagrangePointsType& lagrange_points,
                   const EntityPointers& source_entity_ptr_unique_ptrs, LocalDoFVectorType& range_DoF_vector) const
  {
    static const unsigned int dimRange = SourceType::dimRange;
    size_t kk = 0;
    assert(source_entity_ptr_unique_ptrs.size() >= lagrange_points.size());
    for (size_t ii = 0; ii < lagrange_points.size(); ++ii) {
      if (std::isinf(range_DoF_vector.get(kk))) {
        const auto& global_point = lagrange_points[ii];
        // evaluate source function
        const auto& source_entity_ptr_unique_ptr = source_entity_ptr_unique_ptrs[ii];
        if (source_entity_ptr_unique_ptr) {
          const auto source_entity_ptr  = *source_entity_ptr_unique_ptr;
          const auto& source_entity     = *source_entity_ptr;
          const auto local_source_point = source_entity.geometry().local(global_point);
          const auto local_source       = source.local_function(source_entity);
          const auto source_value = local_source->evaluate(local_source_point);
          for (size_t jj = 0; jj < dimRange; ++jj, ++kk)
            range_DoF_vector.set(kk, source_value[jj]);
        } else
          for (size_t jj = 0; jj < dimRange; ++jj, ++kk)
            range_DoF_vector.set(kk, 0.0);
      } else
        kk += dimRange;
    }
  } // ... apply_local(...)

  const GridViewType& grid_view_;
}; // class Lagrange


template <class GridViewType>
class Generic
{
public:
  typedef typename GridViewType::ctype DomainFieldType;
  static const unsigned int dimDomain = GridViewType::dimension;

  Generic(const GridViewType& grid_view)
    : l2_prolongation_operator_(grid_view)
    , lagrange_prolongation_operator_(grid_view)
  {
  }

  template <class SourceType, class RangeType>
  void apply(const SourceType& source, RangeType& range) const
  {
    redirect_to_appropriate_operator(source, range);
  }

private:
  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, RS, rS, rCS>, VS>& source,
      DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    l2_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>, VS>& source,
      DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    l2_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>, VS>&
          source,
      DiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    l2_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, RS, rS, rCS>, VS>& source,
      DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    lagrange_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>, VS>& source,
      DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    lagrange_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>, VS>&
          source,
      DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    lagrange_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>, VS>& source,
      DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    lagrange_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPS, pS, RS, rS, rCS>, VS>& source,
      DiscreteFunction<ContinuousLagrangeSpace::FemLocalfunctionsWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    lagrange_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<DiscontinuousLagrangeSpace::FemLocalfunctionsWrapper<GPS, pS, RS, rS, rCS>, VS>&
          source,
      DiscreteFunction<ContinuousLagrangeSpace::FemWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    lagrange_prolongation_operator_.apply(source, range);
  }

  template <class GPS, int pS, class RS, int rS, int rCS, class VS, class GPR, int pR, class RR, int rR, int rCR,
            class VR>
  inline void redirect_to_appropriate_operator(
      const ConstDiscreteFunction<ContinuousLagrangeSpace::PdelabWrapper<GPS, pS, RS, rS, rCS>, VS>& source,
      DiscreteFunction<ContinuousLagrangeSpace::PdelabWrapper<GPR, pR, RR, rR, rCR>, VR>& range) const
  {
    lagrange_prolongation_operator_.apply(source, range);
  }

  const L2<GridViewType> l2_prolongation_operator_;
  const Lagrange<GridViewType> lagrange_prolongation_operator_;
}; // class Generic


} // namespace ProlongationOperator
} // namespace GDT
} // namespace Dune

#endif // DUNE_GDT_OPERATOR_PROLONGATIONS_HH
