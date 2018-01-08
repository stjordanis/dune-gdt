// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2017 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2017)

#include <set>

#include <dune/geometry/type.hh>

#include <dune/grid/common/mcmgmapper.hh>

#include <dune/localfunctions/common/virtualinterface.hh>

#include <dune/xt/common/numeric_cast.hh>
#include <dune/xt/grid/type_traits.hh>
#include <dune/xt/functions/interfaces/local-functions.hh>

#include <dune/gdt/exceptions.hh>
#include <dune/gdt/spaces/mapper/interfaces.hh>

#ifndef DUNE_GDT_SPACES_MAPPER_DEFAULT_HH
#define DUNE_GDT_SPACES_MAPPER_DEFAULT_HH

namespace Dune {
namespace GDT {


// forward, required for the traits
template <class GL, class FiniteElementType>
class FixedOrderMultipleCodimMultipleGeomTypeMapper;


namespace internal {


template <class GL, class FiniteElementType>
class FixedOrderMultipleCodimMultipleGeomTypeMapperTraits
{
  static_assert(XT::Grid::is_layer<GL>::value, "");

  template <int dim_>
  struct GeometryTypeLayout
  {
    GeometryTypeLayout(std::set<GeometryType>&& types)
      : types_(std::move(types))
    {
    }

    GeometryTypeLayout(const GeometryTypeLayout<dim_>&) = default;
    GeometryTypeLayout(GeometryTypeLayout<dim_>&&) = default;

    bool contains(const GeometryType& gt) const
    {
      return types_.count(gt) > 0;
    }

    const std::set<GeometryType> types_;
  };

public:
  using derived_type = FixedOrderMultipleCodimMultipleGeomTypeMapper<GL, FiniteElementType>;
  using BackendType = MultipleCodimMultipleGeomTypeMapper<GL, GeometryTypeLayout>;
  using EntityType = XT::Grid::extract_entity_t<GL>;

private:
  friend class FixedOrderMultipleCodimMultipleGeomTypeMapper<GL, FiniteElementType>;
}; // class FixedOrderMultipleCodimMultipleGeomTypeMapperTraits


} // namespace internal


template <class GL, class FiniteElementType>
class FixedOrderMultipleCodimMultipleGeomTypeMapper
    : public MapperInterface<internal::FixedOrderMultipleCodimMultipleGeomTypeMapperTraits<GL, FiniteElementType>>
{
public:
  using Traits = internal::FixedOrderMultipleCodimMultipleGeomTypeMapperTraits<GL, FiniteElementType>;

private:
  using ThisType = FixedOrderMultipleCodimMultipleGeomTypeMapper<GL, FiniteElementType>;
  using BaseType = MapperInterface<Traits>;
  using D = typename GL::ctype;
  static const constexpr size_t d = GL::dimension;

public:
  using typename BaseType::EntityType;
  using typename BaseType::BackendType;

  FixedOrderMultipleCodimMultipleGeomTypeMapper(
      const GL& grid_layer,
      const std::shared_ptr<std::map<GeometryType, std::shared_ptr<FiniteElementType>>>& finite_elements)
    : finite_elements_(finite_elements)
    , mapper_(nullptr)
  {
    std::set<GeometryType> all_DoF_attached_geometry_types;
    // collect all entities (for all codims) which are used to attach DoFs to
    for (auto&& geometry_type : grid_layer.indexSet().types(0)) {
      // get the finite element for this geometry type
      const auto finite_element_search_result = finite_elements_->find(geometry_type);
      if (finite_element_search_result == finite_elements_->end())
        DUNE_THROW(mapper_error, "Missing finite element for the required geometry type " << geometry_type << "!");
      const auto& finite_element = *finite_element_search_result->second;
      // loop over all keys of this finite element
      const auto& reference_element = ReferenceElements<D, d>::general(geometry_type);
      const auto& coeffs = finite_element.localCoefficients();
      for (size_t ii = 0; ii < coeffs.size(); ++ii) {
        const auto& local_key = coeffs.localKey(ii);
        if (local_key.index() != 0) // Would require twisting of DoFs and possibly more knowledge from the FE
          DUNE_THROW(mapper_error, "This case is not covered yet, when we have more than one DoF per (sub)entity!");
        // find the (sub)entity for this key
        const auto sub_entity = local_key.subEntity();
        const auto codim = local_key.codim();
        const auto& subentity_geometry_type = reference_element.type(sub_entity, codim);
        // and add the respective geometry type
        all_DoF_attached_geometry_types.insert(subentity_geometry_type);
      }
    }
    if (all_DoF_attached_geometry_types.size() == 0)
      DUNE_THROW(mapper_error, "This must not happen, the finite elements report no DoFs attached to (sub)entities!");
    mapper_ = std::make_shared<BackendType>(
        grid_layer,
        std::move(typename Traits::template GeometryTypeLayout<d>(std::move(all_DoF_attached_geometry_types))));
  } // ... FixedOrderMultipleCodimMultipleGeomTypeMapper(...)

  FixedOrderMultipleCodimMultipleGeomTypeMapper(const ThisType&) = default;
  FixedOrderMultipleCodimMultipleGeomTypeMapper(ThisType&&) = default;
  FixedOrderMultipleCodimMultipleGeomTypeMapper& operator=(const ThisType&) = delete;
  FixedOrderMultipleCodimMultipleGeomTypeMapper& operator=(ThisType&&) = delete;

  const BackendType& backend() const
  {
    return *mapper_;
  }

  size_t size() const
  {
    return mapper_->size();
  }

  size_t maxNumDofs() const
  {
    size_t ret = 0;
    for (const auto& geometry_type_and_finite_element_pair : *finite_elements_) {
      const auto& finite_element = *geometry_type_and_finite_element_pair.second;
      ret = std::max(ret, XT::Common::numeric_cast<size_t>(finite_element.size()));
    }
    return ret;
  } // ... maxNumDofs(...)

  size_t numDofs(const EntityType& entity) const
  {
    const auto finite_element_search_result = finite_elements_->find(entity.geometry().type());
    if (finite_element_search_result == finite_elements_->end())
      DUNE_THROW(XT::Common::Exceptions::internal_error,
                 "This must not happen after the checks in the ctor, the grid layer did not report all geometry types!"
                 "\n   entity.geometry().type() = "
                     << entity.geometry().type());
    const auto& finite_element = *finite_element_search_result->second;
    return finite_element.size();
  } // ... numDofs(...)

  using BaseType::globalIndices;

  void globalIndices(const EntityType& entity, DynamicVector<size_t>& ret) const
  {
    const auto finite_element_search_result = finite_elements_->find(entity.geometry().type());
    if (finite_element_search_result == finite_elements_->end())
      DUNE_THROW(XT::Common::Exceptions::internal_error,
                 "This must not happen after the checks in the ctor, the grid layer did not report all geometry types!"
                 "\n   entity.geometry().type() = "
                     << entity.geometry().type());
    const auto& finite_element = *finite_element_search_result->second;
    const auto& local_coefficients = finite_element.localCoefficients();
    const auto local_size = local_coefficients.size();
    if (ret.size() < local_size)
      ret.resize(local_size, 0);
    for (size_t ii = 0; ii < local_size; ++ii) {
      const auto& local_key = local_coefficients.localKey(ii);
      // No need to assert local_key.index() == 0, has been checked in the ctor!
      ret[ii] = mapper_->subIndex(entity, local_key.subEntity(), local_key.codim());
    }
  } // ... globalIndices(...)

  size_t mapToGlobal(const EntityType& entity, const size_t local_index) const
  {
    const auto finite_element_search_result = finite_elements_->find(entity.geometry().type());
    if (finite_element_search_result == finite_elements_->end())
      DUNE_THROW(XT::Common::Exceptions::internal_error,
                 "This must not happen after the checks in the ctor, the grid layer did not report all geometry types!"
                 "\n   entity.geometry().type() = "
                     << entity.geometry().type());
    const auto& finite_element = *finite_element_search_result->second;
    const auto& local_coefficients = finite_element.localCoefficients();
    if (local_index >= local_coefficients.size())
      DUNE_THROW(Exception,
                 "finite_element.localCoefficients().size() = " << local_coefficients.size() << "\n   local_index = "
                                                                << local_index);
    const auto& local_key = local_coefficients.localKey(local_index);
    // No need to assert local_key.index() == 0, has been checked in the ctor!
    return mapper_->subIndex(entity, local_key.subEntity(), local_key.codim());
  } // ... mapToGlobal(...)

private:
  const std::shared_ptr<std::map<GeometryType, std::shared_ptr<FiniteElementType>>> finite_elements_;
  std::shared_ptr<BackendType> mapper_;
}; // class FixedOrderMultipleCodimMultipleGeomTypeMapper


} // namespace GDT
} // namespace Dune

#endif // DUNE_GDT_SPACES_MAPPER_DEFAULT_HH
