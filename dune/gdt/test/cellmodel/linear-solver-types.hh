// This file is part of the dune-gdt project:
//   https://github.com/dune-community/dune-gdt
// Copyright 2010-2018 dune-gdt developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Tobias Leibner (2019)

#ifndef DUNE_GDT_TEST_CELLMODEL_LINEAR_SOLVER_TYPES_HH
#define DUNE_GDT_TEST_CELLMODEL_LINEAR_SOLVER_TYPES_HH

enum class PfieldLinearSolverType
{
  direct,
  gmres,
  fgmres_gmres,
  fgmres_bicgstab,
  schur_gmres,
  schur_fgmres_gmres,
  schur_fgmres_bicgstab
};

enum class PfieldMassMatrixSolverType
{
  sparse_lu,
  cg,
  cg_incomplete_cholesky
};

#endif // DUNE_GDT_TEST_CELLMODEL_LINEAR_SOLVER_TYPES_HH
