#ifndef included_IncrementalSVD_h
#define included_IncrementalSVD_h

#include "Matrix.h"
#include <vector>

namespace CAROM {

// An abstract class which embodies the incremental SVD algorithm.  The API is
// intentionally small.  One may increment the SVD, get the tolerance used
// to determine if an increment is new, and get the dimension of the system.
class IncrementalSVD
{
   public:
      // Constructor.
      IncrementalSVD(
         int dim,
         double redundancy_tol,
         bool skip_redundant,
         int increments_per_time_interval);

      // Destructor.
      virtual
      ~IncrementalSVD();

      // Increment the SVD with the new state, u_in, at the given time.
      virtual
      void
      increment(
         const double* u_in,
         double time) = 0;

      // Returns the dimension of the system.
      int
      getDim() const
      {
         return d_dim;
      }

      // Returns the rank of the processor being run on.
      int
      getRank() const
      {
         return d_rank;
      }

      // Returns the number of processors being run on.
      int
      getSize() const
      {
         return d_size;
      }

      // Returns the basis vectors for the current time interval, d_U, as a
      // Matrix.
      virtual
      const Matrix*
      getBasis() = 0;

      // Returns the number of time intervals on which different sets of basis
      // vectors are defined.
      int
      getNumBasisTimeIntervals() const
      {
         return d_num_time_intervals;
      }

      // Returns the start time for the requested time interval.
      double
      getBasisIntervalStartTime(
         int which_interval) const
      {
         CAROM_ASSERT(0 <= which_interval);
         CAROM_ASSERT(which_interval < d_num_time_intervals);
         return d_time_interval_start_times[which_interval];
      }

      // Returns true if the next state will result in a new time interval.
      bool
      isNewTimeInterval() const
      {
         return (d_num_increments == 0) ||
                (d_num_increments >= d_increments_per_time_interval);
      }

   protected:
      // Construct the Q matrix which will be passed to svd.
      void
      constructQ(
         double*& Q,
         const Vector* l,
         double k);

      // Given a matrix, A, returns the 3 components of the singular value
      // decomposition.
      void
      svd(
         double* A,
         Matrix*& U,
         Matrix*& S);

      // Dimension of the system.
      int d_dim;

      // Number of increments stored.
      int d_num_increments;

      // Error tolerance.
      double d_epsilon;

      // If true, skip redundant increments.
      bool d_skip_redundant;

      // The number of increments to be collected for each time interval.
      int d_increments_per_time_interval;

      // The matrix S.  S is not distributed and the entire matrix exists on
      // each processor.
      Matrix* d_S;

      // The basis vectors for the current time interval distributed across all
      // processors.  d_basis is the part of the distributed basis vector local
      // to the processor owning this object.
      Matrix* d_basis;

      // The number of time intervals gathered.
      int d_num_time_intervals;

      // The simulation time at which each time interval starts.
      std::vector<double> d_time_interval_start_times;

      // Rank of process this object lives on.
      int d_rank;

      // Total number of processors.
      int d_size;

      // MPI message tag.
      static const int COMMUNICATE_U;

   private:
      // Unimplemented default constructor.
      IncrementalSVD();

      // Unimplemented copy constructor.
      IncrementalSVD(
         const IncrementalSVD& other);

      // Unimplemented assignment operator.
      IncrementalSVD&
      operator = (
         const IncrementalSVD& rhs);
};

}

#endif