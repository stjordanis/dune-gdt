// This file is part of the dune-gdt project:
//   http://users.dune-project.org/projects/dune-gdt
// Copyright holders: Felix Albrecht
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#ifndef DUNE_GDT_MAPPER_INTERFACE_HH
#define DUNE_GDT_MAPPER_INTERFACE_HH

#include <dune/common/dynvector.hh>

#include <dune/stuff/common/crtp.hh>

namespace Dune {
namespace GDT {


template <class Traits>
class MapperInterface : public Stuff::CRTPInterface<MapperInterface<Traits>, Traits>
{
public:
  typedef typename Traits::derived_type derived_type;
  typedef typename Traits::BackendType BackendType;

  const BackendType& backend() const
  {
    CHECK_CRTP(this->as_imp(*this).backend());
    return this->as_imp(*this).backend();
  }

  size_t size() const
  {
    CHECK_CRTP(this->as_imp(*this).size());
    return this->as_imp(*this).size();
  }

  size_t maxNumDofs() const
  {
    CHECK_CRTP(this->as_imp(*this).maxNumDofs());
    return this->as_imp(*this).maxNumDofs();
  }

  template <class EntityType>
  size_t numDofs(const EntityType& entity) const
  {
    CHECK_CRTP(this->as_imp(*this).numDofs(entity));
    return this->as_imp(*this).numDofs(entity);
  }

  template <class EntityType>
  void globalIndices(const EntityType& entity, Dune::DynamicVector<size_t>& ret) const
  {
    CHECK_AND_CALL_CRTP(this->as_imp(*this).globalIndices(entity, ret));
  }

  template <class EntityType>
  Dune::DynamicVector<size_t> globalIndices(const EntityType& entity) const
  {
    Dune::DynamicVector<size_t> ret(numDofs(entity), 0);
    globalIndices(entity, ret);
    return ret;
  }

  template <class EntityType>
  size_t mapToGlobal(const EntityType& entity, const size_t& localIndex) const
  {
    CHECK_CRTP(this->as_imp(*this).mapToGlobal(entity, localIndex));
    return this->as_imp(*this).mapToGlobal(entity, localIndex);
  }
}; // class MapperInterface


} // namespace GDT
} // namespace Dune


#endif // DUNE_GDT_MAPPER_INTERFACE_HH
