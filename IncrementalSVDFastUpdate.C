/******************************************************************************
 *
 * Copyright (c) 2013-2016, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory
 * Written by William Arrighi wjarrighi@llnl.gov
 * CODE-686965
 * All rights reserved.
 *
 * This file is part of libROM.
 * For details, see https://computation.llnl.gov/librom
 * Please also read README_BSD_NOTICE.
 *
 * Redistribution and use in source and binary forms, with or without
 * modifications, are permitted provided that the following conditions are met:
 *
 *    o Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the disclaimer below.
 *    o Redistribution in binary form must reproduce the above copyright
 *      notice, this list of conditions and the disclaimer (as noted below) in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    o Neither the name of the LLNS/LLNL nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY,
 * LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OR SUCH DAMAGE.
 *
 *****************************************************************************/

// Description: The concrete implementation of the incremental SVD algorithm
//              using Matthew Brand's "fast update" method.

#include "IncrementalSVDFastUpdate.h"

#include "mpi.h"

#include <cmath>
#include <limits>

namespace CAROM {

IncrementalSVDFastUpdate::IncrementalSVDFastUpdate(
   int dim,
   double linearity_tol,
   bool skip_linearly_dependent,
   int samples_per_time_interval,
   bool debug_algorithm) :
   IncrementalSVD(dim,
      linearity_tol,
      skip_linearly_dependent,
      samples_per_time_interval,
      debug_algorithm),
   d_U(0),
   d_Up(0)
{
}

IncrementalSVDFastUpdate::~IncrementalSVDFastUpdate()
{
   // Delete data members.
   if (d_U) {
      delete d_U;
   }
   if (d_Up) {
      delete d_Up;
   }
}

void
IncrementalSVDFastUpdate::buildInitialSVD(
   const double* u,
   double time)
{
   CAROM_ASSERT(u != 0);
   CAROM_ASSERT(time >= 0.0);

   // We have a new time interval.

   // If this is not the first time interval then delete d_basis, d_U, d_Up,
   // and d_S of the just completed time interval.
   int num_time_intervals =
      static_cast<int>(d_time_interval_start_times.size());
   if (num_time_intervals > 0) {
      delete d_basis;
      delete d_U;
      delete d_Up;
      delete d_S;
   }
   d_time_interval_start_times.resize(num_time_intervals+1);
   d_time_interval_start_times[num_time_intervals] = time;

   // Build d_S for this new time interval.
   d_S = new Matrix(1, 1, false);
   Vector u_vec(u, d_dim, true);
   double norm_u = u_vec.norm();
   d_S->item(0, 0) = norm_u;

   // Build d_Up for this new time interval.
   d_Up = new Matrix(1, 1, false);
   d_Up->item(0, 0) = 1.0;

   // Build d_U for this new time interval.
   d_U = new Matrix(d_dim, 1, true);
   for (int i = 0; i < d_dim; ++i) {
      d_U->item(i, 0) = u[i]/norm_u;
   }

   // Compute the basis vectors for this time interval.
   computeBasis();

   // We now have the first sample for the new time interval.
   d_num_samples = 1;
}

void
IncrementalSVDFastUpdate::computeBasis()
{
   d_basis = d_U->mult(d_Up);
}

void
IncrementalSVDFastUpdate::addLinearlyDependentSample(
   const Matrix* A,
   const Matrix* sigma)
{
   CAROM_ASSERT(A != 0);
   CAROM_ASSERT(sigma != 0);

   // Chop a row and a column off of A to form Amod.  Also form
   // d_S by chopping a row and a column off of sigma.
   Matrix Amod(d_num_samples, d_num_samples, false);
   for (int row = 0; row < d_num_samples; ++row){
      for (int col = 0; col < d_num_samples; ++col) {
         Amod.item(row, col) = A->item(row, col);
         d_S->item(row, col) = sigma->item(row, col);
      }
   }

   // Multiply d_Up and Amod and put result into d_Up.
   Matrix* Up_times_Amod = d_Up->mult(Amod);
   delete d_Up;
   d_Up = Up_times_Amod;

   // Reorthogonalize if necessary.
   if (fabs(checkOrthogonality(d_Up)) >
       std::numeric_limits<double>::epsilon()*d_num_samples) {
      reOrthogonalize(d_Up);
   }
}

void
IncrementalSVDFastUpdate::addNewSample(
   const Vector* j,
   const Matrix* A,
   Matrix* sigma)
{
   CAROM_ASSERT(j != 0);
   CAROM_ASSERT(A != 0);
   CAROM_ASSERT(sigma != 0);

   // Add j as a new column of d_U.
   Matrix* newU = new Matrix(d_dim, d_num_samples+1, true);
   for (int row = 0; row < d_dim; ++row) {
      for (int col = 0; col < d_num_samples; ++col) {
         newU->item(row, col) = d_U->item(row, col);
      }
      newU->item(row, d_num_samples) = j->item(row);
   }
   delete d_U;
   d_U = newU;

   // Form a temporary matrix, tmp, by add another row and column to
   // d_Up.  Only the last value in the new row/column is non-zero and it is 1.
   Matrix tmp(d_num_samples+1, d_num_samples+1, false);
   for (int row = 0; row < d_num_samples; ++row) {
      for (int col = 0; col < d_num_samples; ++col) {
         tmp.item(row, col) = d_Up->item(row, col);
      }
      tmp.item(row, d_num_samples) = 0.0;
   }
   for (int col = 0; col < d_num_samples; ++col) {
     tmp.item(d_num_samples, col) = 0.0;
   }
   tmp.item(d_num_samples, d_num_samples) = 1.0;

   // d_Up = tmp*A
   delete d_Up;
   d_Up = tmp.mult(A);

   // d_S = sigma.
   delete d_S;
   d_S = sigma;

   // We now have another sample.
   ++d_num_samples;

   // Reorthogonalize if necessary.
   long int max_U_dim;
   if (d_num_samples > d_total_dim) {
      max_U_dim = d_num_samples;
   }
   else {
      max_U_dim = d_total_dim;
   }
   if (fabs(checkOrthogonality(d_U)) >
       std::numeric_limits<double>::epsilon()*static_cast<double>(max_U_dim)) {
      reOrthogonalize(d_U);
   }
   if (fabs(checkOrthogonality(d_Up)) >
       std::numeric_limits<double>::epsilon()*d_num_samples) {
      reOrthogonalize(d_Up);
   }
}

}
