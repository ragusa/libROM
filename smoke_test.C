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

// Description: Simple test of the incremental fast update algorithm and
//              incremental sampler.

#include "IncrementalSVDBasisGenerator.h"

#include "mpi.h"

#include <stdio.h>

int
main(
  int argc,
  char* argv[])
{
   // Initialize MPI and get the number of processors and this processor's
   // rank.
   MPI_Init(&argc, &argv);
   int size;
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   int rank;
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);

   // Given the number of processors and the rank of this processor set the
   // dimension of the problem.
   int dim;
   if (size == 1) {
      dim = 6;
   }
   else if (size == 2) {
      dim = 3;
   }
   else if (size == 3) {
      dim = 2;
   }
   else if (size == 6) {
      dim = 1;
   }
   else {
      printf("Too many procs\n");
      return 1;
   }

   // Construct the incremental basis generator to use the fast update
   // incremental algorithm and the incremental sampler.
   CAROM::IncrementalSVDBasisGenerator inc_basis_generator(dim,
      1.0e-2,
      false,
      true,
      1.0e-6,
      2,
      1.0e-2,
      0.11,
      "",
      CAROM::Database::HDF5,
      0.1,
      0.8,
      5.0,
      true);

   // Define the values for the first sample.
   double vals0[6] = {1.0, 6.0, 3.0, 8.0, 17.0, 9.0};

   // Define the values for the second sample.
   double vals1[6] = {2.0, 7.0, 4.0, 9.0, 18.0, 10.0};

   bool status = false;

   // Take the first sample.
   if (inc_basis_generator.isNextSample(0.0)) {
      status = inc_basis_generator.takeSample(&vals0[dim*rank], 0.0, 0.11);
      if (status) {
         inc_basis_generator.computeNextSampleTime(&vals0[dim*rank],
            &vals0[dim*rank],
            0.0);
      }
   }

   // Take the second sample.
   if (status && inc_basis_generator.isNextSample(0.11)) {
      status = inc_basis_generator.takeSample(&vals1[dim*rank], 0.11, 0.11);
      if (status) {
         inc_basis_generator.computeNextSampleTime(&vals1[dim*rank],
            &vals1[dim*rank],
            0.11);
      }
   }

   // Finalize MPI and return.
   MPI_Finalize();
   return !status;
}
