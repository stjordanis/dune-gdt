// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2018 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2015 - 2018)
//   René Fritze     (2014, 2016 - 2018)
//   Tobias Leibner  (2014, 2016)

#ifndef DUNE_GDT_SPACES_PARALLEL_COMMUNICATION_HH
#define DUNE_GDT_SPACES_PARALLEL_COMMUNICATION_HH

#include <dune/common/parallel/communicator.hh>

#if HAVE_DUNE_ISTL
#  include <dune/istl/owneroverlapcopy.hh>
#endif

#include <dune/xt/common/parallel/helper.hh>

#include "helper.hh"

namespace Dune {
namespace GDT {


template <class ViewImp,
          bool is_parallel = Dune::XT::UseParallelCommunication<
              typename XT::Grid::extract_grid<ViewImp>::type::CollectiveCommunication>::value>
struct DofCommunicationChooser
{
  typedef Dune::XT::SequentialCommunication Type;

  static Type* create(const ViewImp& /*gridView*/)
  {
    return new Type;
  }

  template <class SpaceBackend>
  static bool prepare(const SpaceBackend& /*space_backend*/, Type& /*communicator*/)
  {
    return false;
  }
}; // struct DofCommunicationChooser


#if HAVE_MPI && HAVE_DUNE_ISTL


template <class ViewImp>
struct DofCommunicationChooser<ViewImp, true>
{
private:
  // this is necessary because alugrid's id is not integral
  using RealGlobalId = typename XT::Grid::extract_grid_t<ViewImp>::GlobalIdSet::IdType;
  using RealLocalId = typename XT::Grid::extract_grid_t<ViewImp>::LocalIdSet::IdType;

public:
  using GlobalId = std::conditional_t<std::is_arithmetic<RealGlobalId>::value, RealGlobalId, uint64_t>;
  using LocalId = std::conditional_t<std::is_arithmetic<RealLocalId>::value, RealLocalId, int>;
  using Type = OwnerOverlapCopyCommunication<GlobalId, LocalId>;
  using type = Type;

  static Type* create(const ViewImp& gridView)
  {
    return new Type(gridView.comm(), SolverCategory::overlapping);
  }

  template <class GV, size_t r, size_t rD, class R>
  static bool prepare(const SpaceInterface<GV, r, rD, R>& space, Type& communicator)
  {
    GDT::GenericParallelHelper<GV, r, rD, R>(space, 1).setup_parallel_indexset(communicator);
    return true;
  } // ... prepare(...)

}; // struct DofCommunicationChooser< ..., true >


#endif // HAVE_MPI && HAVE_DUNE_ISTL

} // namespace GDT
} // namespace Dune

#endif // DUNE_GDT_SPACES_PARALLEL_COMMUNICATION_HH
