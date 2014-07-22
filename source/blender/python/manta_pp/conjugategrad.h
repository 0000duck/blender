




// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).




/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey 
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL) 
 * http://www.gnu.org/licenses
 *
 * Conjugate gradient solver
 *
 ******************************************************************************/

#ifndef _CONJUGATEGRADIENT_H
#define _CONJUGATEGRADIENT_H

#include "vectorbase.h"
#include "grid.h"
#include "kernel.h"

namespace Manta { 

static const bool CG_DEBUG = false;

//! Basic CG interface 
class GridCgInterface {
	public:
		enum PreconditionType { PC_None=0, PC_ICP, PC_mICP };
		
		GridCgInterface() : mUseResNorm(true) {};
		virtual ~GridCgInterface() {};

		// solving functions
		virtual bool iterate() = 0;
		virtual void solve(int maxIter) = 0;

		// precond
		virtual void setPreconditioner(PreconditionType method, Grid<Real> *A0, Grid<Real> *Ai, Grid<Real> *Aj, Grid<Real> *Ak) = 0;

		// access
		virtual Real getSigma() const = 0;
		virtual Real getIterations() const = 0;
		virtual Real getResNorm() const = 0;
		virtual void setAccuracy(Real set) = 0;
		virtual Real getAccuracy() const = 0;

		void setUseResNorm(bool set) { mUseResNorm = set; }

	protected:

		// use norm of residual, or max value for threshold?
		bool mUseResNorm; 
};


//! Run single iteration of the cg solver
/*! the template argument determines the type of matrix multiplication,
	typically a ApplyMatrix kernel, another one is needed e.g. for the
	mesh-based wave equation solver */
template<class APPLYMAT>
class GridCg : public GridCgInterface {
	public:
		//! constructor
		GridCg(Grid<Real>& dst, Grid<Real>& rhs, Grid<Real>& residual, Grid<Real>& search, FlagGrid& flags, Grid<Real>& tmp, 
				Grid<Real>* A0, Grid<Real>* pAi, Grid<Real>* pAj, Grid<Real>* pAk);
		~GridCg() {}
		
		void doInit();
		bool iterate();
		void solve(int maxIter);
		//! init pointers, and copy values from "normal" matrix
		void setPreconditioner(PreconditionType method, Grid<Real> *A0, Grid<Real> *Ai, Grid<Real> *Aj, Grid<Real> *Ak);
		
		// Accessors        
		Real getSigma() const { return mSigma; }
		Real getIterations() const { return mIterations; }

		Real getResNorm() const { return mResNorm; }

		void setAccuracy(Real set) { mAccuracy=set; }
		Real getAccuracy() const { return mAccuracy; }

	protected:
		bool mInited;
		int mIterations;
		// grids
		Grid<Real>& mDst;
		Grid<Real>& mRhs;
		Grid<Real>& mResidual;
		Grid<Real>& mSearch;
		FlagGrid& mFlags;
		Grid<Real>& mTmp;

		Grid<Real> *mpA0, *mpAi, *mpAj, *mpAk;

		PreconditionType mPcMethod;
		//! preconditioning grids
		Grid<Real> *mpPCA0, *mpPCAi, *mpPCAj, *mpPCAk;

		//! sigma / residual
		Real mSigma;
		//! accuracy of solver (max. residuum)
		Real mAccuracy;
		//! norm of the residual
		Real mResNorm;
}; // GridCg


//! Kernel: Apply symmetric stored Matrix



 struct ApplyMatrix : public KernelBase { ApplyMatrix(FlagGrid& flags, Grid<Real>& dst, Grid<Real>& src, Grid<Real>& A0, Grid<Real>& Ai, Grid<Real>& Aj, Grid<Real>& Ak) :  KernelBase(&flags,0) ,flags(flags),dst(dst),src(src),A0(A0),Ai(Ai),Aj(Aj),Ak(Ak)   { run(); }  inline void op(int idx, FlagGrid& flags, Grid<Real>& dst, Grid<Real>& src, Grid<Real>& A0, Grid<Real>& Ai, Grid<Real>& Aj, Grid<Real>& Ak )  {
	if (!flags.isFluid(idx)) {
		dst[idx] = src[idx];
		return;
	}    
	dst[idx] =  src[idx] * A0[idx]
				+ src[idx-X] * Ai[idx-X]
				+ src[idx+X] * Ai[idx]
				+ src[idx-Y] * Aj[idx-Y]
				+ src[idx+Y] * Aj[idx]
				+ src[idx-Z] * Ak[idx-Z] 
				+ src[idx+Z] * Ak[idx];
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline Grid<Real>& getArg1() { return dst; } typedef Grid<Real> type1;inline Grid<Real>& getArg2() { return src; } typedef Grid<Real> type2;inline Grid<Real>& getArg3() { return A0; } typedef Grid<Real> type3;inline Grid<Real>& getArg4() { return Ai; } typedef Grid<Real> type4;inline Grid<Real>& getArg5() { return Aj; } typedef Grid<Real> type5;inline Grid<Real>& getArg6() { return Ak; } typedef Grid<Real> type6; void run() {  const int _sz = size; for (int i=0; i < _sz; i++) op(i, flags,dst,src,A0,Ai,Aj,Ak);  } FlagGrid& flags; Grid<Real>& dst; Grid<Real>& src; Grid<Real>& A0; Grid<Real>& Ai; Grid<Real>& Aj; Grid<Real>& Ak;   };

//! Kernel: Apply symmetric stored Matrix. 2D version



 struct ApplyMatrix2D : public KernelBase { ApplyMatrix2D(FlagGrid& flags, Grid<Real>& dst, Grid<Real>& src, Grid<Real>& A0, Grid<Real>& Ai, Grid<Real>& Aj, Grid<Real>& Ak) :  KernelBase(&flags,0) ,flags(flags),dst(dst),src(src),A0(A0),Ai(Ai),Aj(Aj),Ak(Ak)   { run(); }  inline void op(int idx, FlagGrid& flags, Grid<Real>& dst, Grid<Real>& src, Grid<Real>& A0, Grid<Real>& Ai, Grid<Real>& Aj, Grid<Real>& Ak )  {
	unusedParameter(Ak); // only there for parameter compatibility with ApplyMatrix
	
	if (!flags.isFluid(idx)) {
		dst[idx] = src[idx];
		return;
	}    
	dst[idx] =  src[idx] * A0[idx]
				+ src[idx-X] * Ai[idx-X]
				+ src[idx+X] * Ai[idx]
				+ src[idx-Y] * Aj[idx-Y]
				+ src[idx+Y] * Aj[idx];
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline Grid<Real>& getArg1() { return dst; } typedef Grid<Real> type1;inline Grid<Real>& getArg2() { return src; } typedef Grid<Real> type2;inline Grid<Real>& getArg3() { return A0; } typedef Grid<Real> type3;inline Grid<Real>& getArg4() { return Ai; } typedef Grid<Real> type4;inline Grid<Real>& getArg5() { return Aj; } typedef Grid<Real> type5;inline Grid<Real>& getArg6() { return Ak; } typedef Grid<Real> type6; void run() {  const int _sz = size; for (int i=0; i < _sz; i++) op(i, flags,dst,src,A0,Ai,Aj,Ak);  } FlagGrid& flags; Grid<Real>& dst; Grid<Real>& src; Grid<Real>& A0; Grid<Real>& Ai; Grid<Real>& Aj; Grid<Real>& Ak;   };

//! Kernel: Construct the matrix for the poisson equation

 struct MakeLaplaceMatrix : public KernelBase { MakeLaplaceMatrix(FlagGrid& flags, Grid<Real>& A0, Grid<Real>& Ai, Grid<Real>& Aj, Grid<Real>& Ak) :  KernelBase(&flags,1) ,flags(flags),A0(A0),Ai(Ai),Aj(Aj),Ak(Ak)   { run(); }  inline void op(int i, int j, int k, FlagGrid& flags, Grid<Real>& A0, Grid<Real>& Ai, Grid<Real>& Aj, Grid<Real>& Ak )  {
	if (!flags.isFluid(i,j,k))
		return;
	
	// center
	if (!flags.isObstacle(i-1,j,k)) A0(i,j,k) += 1.;
	if (!flags.isObstacle(i+1,j,k)) A0(i,j,k) += 1.;
	if (!flags.isObstacle(i,j-1,k)) A0(i,j,k) += 1.;
	if (!flags.isObstacle(i,j+1,k)) A0(i,j,k) += 1.;
	if (flags.is3D() && !flags.isObstacle(i,j,k-1)) A0(i,j,k) += 1.;
	if (flags.is3D() && !flags.isObstacle(i,j,k+1)) A0(i,j,k) += 1.;
	
	if (flags.isFluid(i+1,j,k)) Ai(i,j,k) = -1.;
	if (flags.isFluid(i,j+1,k)) Aj(i,j,k) = -1.;
	if (flags.is3D() && flags.isFluid(i,j,k+1)) Ak(i,j,k) = -1.;    
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline Grid<Real>& getArg1() { return A0; } typedef Grid<Real> type1;inline Grid<Real>& getArg2() { return Ai; } typedef Grid<Real> type2;inline Grid<Real>& getArg3() { return Aj; } typedef Grid<Real> type3;inline Grid<Real>& getArg4() { return Ak; } typedef Grid<Real> type4; void run() {  const int _maxX = maxX; const int _maxY = maxY; for (int k=minZ; k< maxZ; k++) for (int j=1; j< _maxY; j++) for (int i=1; i< _maxX; i++) op(i,j,k, flags,A0,Ai,Aj,Ak);  } FlagGrid& flags; Grid<Real>& A0; Grid<Real>& Ai; Grid<Real>& Aj; Grid<Real>& Ak;   };




} // namespace

#endif 


