// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2017 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2017)

#ifndef DUNE_GDT_SPACES_RT_DEFAULT_HH
#define DUNE_GDT_SPACES_RT_DEFAULT_HH

#include <memory>
#include <vector>

#include <dune/geometry/referenceelements.hh>
#include <dune/geometry/type.hh>

#include <dune/grid/common/rangegenerators.hh>

#include <dune/localfunctions/raviartthomas.hh>
#include <dune/localfunctions/common/virtualinterface.hh>
#include <dune/localfunctions/common/virtualwrappers.hh>

#include <dune/xt/common/exceptions.hh>
#include <dune/xt/common/numeric_cast.hh>

#include <dune/gdt/spaces/basefunctionset/interface.hh>
#include <dune/gdt/spaces/mapper/default.hh>
#include <dune/gdt/spaces/rt/interface.hh>

namespace Dune {
namespace GDT {


// forwards, required for the traits
template <class E, class R = double>
class RaviartThomasBasefunctionSet;

template <class GL, int p, class R = double>
class RtSpace;


namespace internal {


template <class E, class R>
class RaviartThomasBasefunctionSetTraits
{
  using LocalFunctionTraits =
      XT::Functions::LocalfunctionSetInterface<E, typename E::Geometry::ctype, E::dimension, R, E::dimension>;

public:
  using derived_type = RaviartThomasBasefunctionSet<E, R>;
  using EntityType = E;
  using BackendType = RaviartThomasSimplexLocalFiniteElement<E::dimension, typename E::Geometry::ctype, R>;
};


template <class GL, int p, class R>
class RtSpaceTraits
{
  static_assert(XT::Grid::is_layer<GL>::value, "");
  static_assert(p == 0, "Not implemented yet!");

public:
  using derived_type = RtSpace<GL, p, R>;
  static const constexpr int polOrder = p;
  static const constexpr size_t dimDomain = GL::dimension;
  static const constexpr size_t dimRange = dimDomain;
  static const constexpr size_t dimRangeCols = 1;
  static const constexpr bool continuous = false;
  using GridLayerType = GL;
  using BaseFunctionSetType = RaviartThomasBasefunctionSet<XT::Grid::extract_entity_t<GL>, R>;
  using MapperType =
      FixedOrderMultipleCodimMultipleGeomTypeMapper<GL,
                                                    RaviartThomasSimplexLocalFiniteElement<dimDomain,
                                                                                           typename GL::ctype,
                                                                                           R>>;
  using RangeFieldType = R;
  using BackendType = double;
  static const constexpr XT::Grid::Backends layer_backend = XT::Grid::Backends::view;
  using CommunicatorType = double;
}; // class RtSpaceTraits


} // namespace internal


template <class E, class R>
class RaviartThomasBasefunctionSet : public BaseFunctionSetInterface<internal::RaviartThomasBasefunctionSetTraits<E, R>,
                                                                     typename E::Geometry::ctype,
                                                                     E::dimension,
                                                                     R,
                                                                     E::dimension,
                                                                     1>
{
public:
  using Traits = internal::RaviartThomasBasefunctionSetTraits<E, R>;

private:
  using BaseType = BaseFunctionSetInterface<Traits, typename E::Geometry::ctype, E::dimension, R, E::dimension, 1>;
  using ThisType = RaviartThomasBasefunctionSet<E, R>;

public:
  using typename BaseType::BackendType;
  using typename BaseType::EntityType;
  using typename BaseType::DomainType;
  using typename BaseType::RangeType;
  using typename BaseType::JacobianRangeType;
  using typename BaseType::D;
  using BaseType::d;

  RaviartThomasBasefunctionSet(const EntityType& en,
                               const BackendType& finite_element,
                               const std::vector<bool>& switches)
    : BaseType(en)
    , finite_element_(finite_element)
    , switches_(switches)
  {
    if (switches_.size() != finite_element_.size())
      DUNE_THROW(XT::Common::Exceptions::shapes_do_not_match,
                 "finite_element.size() = " << finite_element_.size() << "\n   switches.size() = " << switches_.size());
  }

  RaviartThomasBasefunctionSet(const ThisType&) = default;
  RaviartThomasBasefunctionSet(ThisType&&) = default;

  ThisType& operator=(const ThisType&) = delete;
  ThisType& operator=(ThisType&&) = delete;

  const BackendType& backend() const
  {
    return finite_element_;
  }

  size_t size() const override final
  {
    return finite_element_.localBasis().size();
  }

  size_t order(const XT::Common::Parameter& = {}) const override final
  {
    return finite_element_.localBasis().order();
  }

  using BaseType::evaluate;

  void evaluate(const DomainType& xx,
                std::vector<RangeType>& ret,
                const XT::Common::Parameter& /*mu*/ = {}) const override final
  {
    assert(this->is_a_valid_point(xx));
    // evaluate shape functions
    finite_element_.localBasis().evaluateFunction(xx, ret);
    // flip shape functions to ensure continuity of normal component
    for (size_t ii = 0; ii < finite_element_.localBasis().size(); ++ii)
      if (switches_[ii])
        ret[ii] *= -1;
    // apply piola transformation
    const auto J_T = this->entity().geometry().jacobianTransposed(xx);
    const auto det_J_T = std::abs(J_T.determinant());
    RangeType tmp_value;
    for (size_t ii = 0; ii < finite_element_.localBasis().size(); ++ii) {
      J_T.mtv(ret[ii], tmp_value);
      tmp_value /= det_J_T;
      ret[ii] = tmp_value;
    }
  } // ... evaluate(...)

  using BaseType::jacobian;

  void jacobian(const DomainType& xx,
                std::vector<JacobianRangeType>& ret,
                const XT::Common::Parameter& /*mu*/ = {}) const override final
  {
    assert(this->is_a_valid_point(xx));
    // evaluate jacobian of shape functions
    finite_element_.localBasis().evaluateJacobian(xx, ret);
    // flip shape functions to ensure continuity of normal component
    for (size_t ii = 0; ii < finite_element_.localBasis().size(); ++ii)
      if (switches_[ii])
        ret[ii] *= -1;
    // apply piola transformation
    const auto J_T = this->entity().geometry().jacobianTransposed(xx);
    const auto det_J_T = std::abs(J_T.determinant());
    const auto J_inv_T = this->entity().geometry().jacobianInverseTransposed(xx);
    auto tmp_jacobian_row = ret[0][0];
    for (size_t ii = 0; ii < finite_element_.localBasis().size(); ++ii) {
      for (size_t jj = 0; jj < d; ++jj) {
        J_inv_T.mtv(ret[ii][jj], tmp_jacobian_row);
        J_T.mtv(tmp_jacobian_row, ret[ii][jj]);
        ret[ii][jj] /= det_J_T;
      }
    }
  } // ... jacobian(...)

private:
  const BackendType& finite_element_;
  const std::vector<bool>& switches_;
}; // class RaviartThomasBasefunctionSet


template <class GL, int p, class R>
class RtSpace : public RtSpaceInterface<internal::RtSpaceTraits<GL, p, R>, GL::dimension, GL::dimension>
{
public:
  using Traits = internal::RtSpaceTraits<GL, p, R>;

private:
  using BaseType = RtSpaceInterface<Traits, GL::dimension, GL::dimension>;
  using ThisType = RtSpace<GL, p, R>;
  using D = typename GL::ctype;
  static const constexpr size_t d = BaseType::dimDomain;
  using FiniteElementType = RaviartThomasSimplexLocalFiniteElement<d, D, R>;

public:
  using typename BaseType::GridLayerType;
  using typename BaseType::EntityType;
  using typename BaseType::MapperType;
  using typename BaseType::BaseFunctionSetType;

  RtSpace(GridLayerType grd_lr)
    : grid_layer_(grd_lr)
    , communicator_(0)
    , backend_(0)
    , finite_elements_(new std::map<GeometryType, std::shared_ptr<FiniteElementType>>())
    , local_DoF_indices_(new std::map<GeometryType, std::vector<size_t>>())
    , switches_(new std::vector<std::vector<bool>>(grid_layer_.indexSet().size(0)))
    , mapper_(nullptr)
  {
    const auto& index_set = grid_layer_.indexSet();
    // create finite elements
    for (auto&& geometry_type : index_set.types(0)) {
      auto finite_element = std::make_shared<FiniteElementType>(geometry_type, p);
      finite_elements_->insert(std::make_pair(geometry_type, finite_element));
      // this is only valid for p0 elements
      local_DoF_indices_->insert(std::make_pair(geometry_type, std::vector<size_t>(finite_element->size())));
    }
    // compute local-key-to-intersection relationship
    for (const auto& geometry_type_and_finite_element_ptr : *finite_elements_) {
      const auto& geometry_type = geometry_type_and_finite_element_ptr.first;
      const auto& finite_element = *geometry_type_and_finite_element_ptr.second;
      const auto& coeffs = finite_element.localCoefficients();
      auto& local_key_to_intersection_map = (*local_DoF_indices_)[geometry_type];
      for (size_t ii = 0; ii < coeffs.size(); ++ii) {
        const auto& local_key = coeffs.localKey(ii);
        if (local_key.index() != 0)
          DUNE_THROW(XT::Common::Exceptions::internal_error, "This must not happen for p0!");
        if (local_key.codim() != 1)
          DUNE_THROW(XT::Common::Exceptions::internal_error, "This must not happen for p0 on simplices!");
        local_key_to_intersection_map[ii] = local_key.subEntity();
      }
    }
    // compute switches to ensure continuity of the normal component
    for (auto&& entity : elements(grid_layer_)) {
      const auto geometry_type = entity.geometry().type();
      const auto& finite_element = *finite_elements_->at(geometry_type);
      const auto& coeffs = finite_element.localCoefficients();
      const auto entity_index = index_set.index(entity);
      (*switches_)[entity_index] = std::vector<bool>(coeffs.size(), false);
      auto& local_switches = (*switches_)[entity_index];
      for (auto&& intersection : intersections(grid_layer_, entity)) {
        if (intersection.neighbor() && entity_index < index_set.index(intersection.outside())) {
          const auto intersection_index = XT::Common::numeric_cast<unsigned int>(intersection.indexInInside());
          for (size_t ii = 0; ii < coeffs.size(); ++ii) {
            const auto& local_key = coeffs.localKey(ii);
            const auto DoF_subentity_index = local_key.subEntity();
            if (local_key.codim() == 1 && DoF_subentity_index == intersection_index)
              local_switches[DoF_subentity_index] = true;
          }
        }
      }
    }
    // create mapper
    mapper_ = std::make_shared<MapperType>(grid_layer_, finite_elements_);
  } // RtSpace(...)

  RtSpace(const ThisType&) = default;
  RtSpace(ThisType&&) = default;

  ThisType& operator=(const ThisType&) = delete;
  ThisType& operator=(ThisType&&) = delete;

  const GridLayerType& grid_layer() const
  {
    return grid_layer_;
  }

  const double& backend() const
  {
    return backend_;
  }

  const MapperType& mapper() const
  {
    return *mapper_;
  }

  BaseFunctionSetType base_function_set(const EntityType& entity) const
  {
    const auto finite_element_search_result = finite_elements_->find(entity.geometry().type());
    if (finite_element_search_result == finite_elements_->end())
      DUNE_THROW(XT::Common::Exceptions::internal_error,
                 "This must not happen after the checks in the ctor, the grid layer did not report all geometry types!"
                 "\n   entity.geometry().type() = "
                     << entity.geometry().type());
    const auto& finite_element = *finite_element_search_result->second;
    return BaseFunctionSetType(entity, finite_element, (*switches_)[grid_layer_.indexSet().index(entity)]);
  }

  double& communicator() const
  {
    DUNE_THROW(NotImplemented, "");
    return communicator_;
  }

  // this makes sense only for for p0 (and only on simplices?)
  std::vector<size_t> local_DoF_indices(const EntityType& entity) const
  {
    const auto search_result = local_DoF_indices_->find(entity.geometry().type());
    if (search_result == local_DoF_indices_->end())
      DUNE_THROW(XT::Common::Exceptions::internal_error,
                 "This must not happen after the checks in the ctor, the grid layer did not report all geometry types!"
                 "\n   entity.geometry().type() = "
                     << entity.geometry().type());
    return search_result->second;
  } // ... local_DoF_indices(...)

private:
  const GridLayerType grid_layer_;
  mutable double communicator_;
  const double backend_;
  std::shared_ptr<std::map<GeometryType, std::shared_ptr<FiniteElementType>>> finite_elements_;
  std::shared_ptr<std::map<GeometryType, std::vector<size_t>>> local_DoF_indices_;
  std::shared_ptr<std::vector<std::vector<bool>>> switches_;
  std::shared_ptr<MapperType> mapper_;
}; // class RtSpace


} // namespace GDT
} // namespace Dune

#endif // DUNE_GDT_SPACES_RT_DEFAULT_HH
