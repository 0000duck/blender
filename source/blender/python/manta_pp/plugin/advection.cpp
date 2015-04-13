




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
 * Plugins for pressure correction:
 * - solve_pressure
 *
 ******************************************************************************/

#include "vectorbase.h"
#include "grid.h"
#include "kernel.h"
#include <limits>

using namespace std;

namespace Manta { 


//! Semi-Lagrange interpolation kernel


template <class T>  struct SemiLagrange : public KernelBase { SemiLagrange(FlagGrid& flags, MACGrid& vel, Grid<T>& dst, Grid<T>& src, Real dt, bool isLevelset, int orderSpace) :  KernelBase(&flags,1) ,flags(flags),vel(vel),dst(dst),src(src),dt(dt),isLevelset(isLevelset),orderSpace(orderSpace)   { run(); }  inline void op(int i, int j, int k, FlagGrid& flags, MACGrid& vel, Grid<T>& dst, Grid<T>& src, Real dt, bool isLevelset, int orderSpace )  {
	// traceback position
	Vec3 pos = Vec3(i+0.5f,j+0.5f,k+0.5f) - vel.getCentered(i,j,k) * dt;
	dst(i,j,k) = src.getInterpolatedHi(pos, orderSpace);
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline MACGrid& getArg1() { return vel; } typedef MACGrid type1;inline Grid<T>& getArg2() { return dst; } typedef Grid<T> type2;inline Grid<T>& getArg3() { return src; } typedef Grid<T> type3;inline Real& getArg4() { return dt; } typedef Real type4;inline bool& getArg5() { return isLevelset; } typedef bool type5;inline int& getArg6() { return orderSpace; } typedef int type6; void run() {  const int _maxX = maxX; const int _maxY = maxY; for (int k=minZ; k< maxZ; k++) for (int j=1; j< _maxY; j++) for (int i=1; i< _maxX; i++) op(i,j,k, flags,vel,dst,src,dt,isLevelset,orderSpace);  } FlagGrid& flags; MACGrid& vel; Grid<T>& dst; Grid<T>& src; Real dt; bool isLevelset; int orderSpace;   };

//! Semi-Lagrange interpolation kernel for MAC grids


 struct SemiLagrangeMAC : public KernelBase { SemiLagrangeMAC(FlagGrid& flags, MACGrid& vel, MACGrid& dst, MACGrid& src, Real dt, int orderSpace) :  KernelBase(&flags,1) ,flags(flags),vel(vel),dst(dst),src(src),dt(dt),orderSpace(orderSpace)   { run(); }  inline void op(int i, int j, int k, FlagGrid& flags, MACGrid& vel, MACGrid& dst, MACGrid& src, Real dt, int orderSpace )  {
	// get currect velocity at MAC position
	// no need to shift xpos etc. as lookup field is also shifted
	Vec3 xpos = Vec3(i+0.5f,j+0.5f,k+0.5f) - vel.getAtMACX(i,j,k) * dt;
	Real vx = src.getInterpolatedComponentHi<0>(xpos, orderSpace);
	Vec3 ypos = Vec3(i+0.5f,j+0.5f,k+0.5f) - vel.getAtMACY(i,j,k) * dt;
	Real vy = src.getInterpolatedComponentHi<1>(ypos, orderSpace);
	Vec3 zpos = Vec3(i+0.5f,j+0.5f,k+0.5f) - vel.getAtMACZ(i,j,k) * dt;
	Real vz = src.getInterpolatedComponentHi<2>(zpos, orderSpace);
	
	dst(i,j,k) = Vec3(vx,vy,vz);
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline MACGrid& getArg1() { return vel; } typedef MACGrid type1;inline MACGrid& getArg2() { return dst; } typedef MACGrid type2;inline MACGrid& getArg3() { return src; } typedef MACGrid type3;inline Real& getArg4() { return dt; } typedef Real type4;inline int& getArg5() { return orderSpace; } typedef int type5; void run() {  const int _maxX = maxX; const int _maxY = maxY; for (int k=minZ; k< maxZ; k++) for (int j=1; j< _maxY; j++) for (int i=1; i< _maxX; i++) op(i,j,k, flags,vel,dst,src,dt,orderSpace);  } FlagGrid& flags; MACGrid& vel; MACGrid& dst; MACGrid& src; Real dt; int orderSpace;   };


//! Kernel: Correct based on forward and backward SL steps (for both centered & mac grids)



template <class T>  struct MacCormackCorrect : public KernelBase { MacCormackCorrect(FlagGrid& flags, Grid<T>& dst, Grid<T>& old, Grid<T>& fwd, Grid<T>& bwd, Real strength, bool isLevelSet, bool isMAC=false ) :  KernelBase(&flags,0) ,flags(flags),dst(dst),old(old),fwd(fwd),bwd(bwd),strength(strength),isLevelSet(isLevelSet),isMAC(isMAC)   { run(); }  inline void op(int idx, FlagGrid& flags, Grid<T>& dst, Grid<T>& old, Grid<T>& fwd, Grid<T>& bwd, Real strength, bool isLevelSet, bool isMAC=false  )  {
	dst[idx] = fwd[idx];

	if (flags.isFluid(idx)) {
		// only correct inside fluid region; note, strenth of correction can be modified here
		dst[idx] += strength * 0.5 * (old[idx] - bwd[idx]);
	}
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline Grid<T>& getArg1() { return dst; } typedef Grid<T> type1;inline Grid<T>& getArg2() { return old; } typedef Grid<T> type2;inline Grid<T>& getArg3() { return fwd; } typedef Grid<T> type3;inline Grid<T>& getArg4() { return bwd; } typedef Grid<T> type4;inline Real& getArg5() { return strength; } typedef Real type5;inline bool& getArg6() { return isLevelSet; } typedef bool type6;inline bool& getArg7() { return isMAC; } typedef bool type7; void run() {  const int _sz = size; for (int i=0; i < _sz; i++) op(i, flags,dst,old,fwd,bwd,strength,isLevelSet,isMAC);  } FlagGrid& flags; Grid<T>& dst; Grid<T>& old; Grid<T>& fwd; Grid<T>& bwd; Real strength; bool isLevelSet; bool isMAC;   };

//! Kernel: Correct based on forward and backward SL steps (for both centered & mac grids)



template <class T>  struct MacCormackCorrectMAC : public KernelBase { MacCormackCorrectMAC(FlagGrid& flags, Grid<T>& dst, Grid<T>& old, Grid<T>& fwd, Grid<T>& bwd, Real strength, bool isLevelSet, bool isMAC=false ) :  KernelBase(&flags,0) ,flags(flags),dst(dst),old(old),fwd(fwd),bwd(bwd),strength(strength),isLevelSet(isLevelSet),isMAC(isMAC)   { run(); }  inline void op(int i, int j, int k, FlagGrid& flags, Grid<T>& dst, Grid<T>& old, Grid<T>& fwd, Grid<T>& bwd, Real strength, bool isLevelSet, bool isMAC=false  )  {
	bool skip[3] = { false, false, false };

	if (!flags.isFluid(i,j,k)) skip[0] = skip[1] = skip[2] = true;
	if(isMAC) {
		if( (i>0) && (!flags.isFluid(i-1,j,k) )) skip[0] = true; 
		if( (j>0) && (!flags.isFluid(i,j-1,k) )) skip[1] = true; 
		if( (k>0) && (!flags.isFluid(i,j,k-1) )) skip[2] = true; 
	}

	for(int c=0; c<3; ++c ) {
		if ( skip[c] ) {
			dst(i,j,k)[c] = fwd(i,j,k)[c];
		} else { 
			// perform actual correction with given strength
			dst(i,j,k)[c] = fwd(i,j,k)[c] + strength * 0.5 * (old(i,j,k)[c] - bwd(i,j,k)[c]);
		}
	}
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline Grid<T>& getArg1() { return dst; } typedef Grid<T> type1;inline Grid<T>& getArg2() { return old; } typedef Grid<T> type2;inline Grid<T>& getArg3() { return fwd; } typedef Grid<T> type3;inline Grid<T>& getArg4() { return bwd; } typedef Grid<T> type4;inline Real& getArg5() { return strength; } typedef Real type5;inline bool& getArg6() { return isLevelSet; } typedef bool type6;inline bool& getArg7() { return isMAC; } typedef bool type7; void run() {  const int _maxX = maxX; const int _maxY = maxY; for (int k=minZ; k< maxZ; k++) for (int j=0; j< _maxY; j++) for (int i=0; i< _maxX; i++) op(i,j,k, flags,dst,old,fwd,bwd,strength,isLevelSet,isMAC);  } FlagGrid& flags; Grid<T>& dst; Grid<T>& old; Grid<T>& fwd; Grid<T>& bwd; Real strength; bool isLevelSet; bool isMAC;   };

// Helper to collect min/max in a template
template<class T> inline void getMinMax(T& minv, T& maxv, const T& val) {
	if (val < minv) minv = val;
	if (val > maxv) maxv = val;
}
template<> inline void getMinMax<Vec3>(Vec3& minv, Vec3& maxv, const Vec3& val) {
	getMinMax(minv.x, maxv.x, val.x);
	getMinMax(minv.y, maxv.y, val.y);
	getMinMax(minv.z, maxv.z, val.z);
}

	
//! Helper function for clamping non-mac grids
template<class T>
inline T doClampComponent(const Vec3i& gridSize, T dst, Grid<T>& orig, T fwd, const Vec3& pos, const Vec3& vel ) 
{
	T minv( std::numeric_limits<Real>::max()), maxv( -std::numeric_limits<Real>::max());

	// forward (and optionally) backward
	Vec3i positions[2];
	positions[0] = toVec3i(pos - vel);
	positions[1] = toVec3i(pos + vel);

	for(int l=0; l<2; ++l) {
		Vec3i& currPos = positions[l];

		// clamp forward lookup to grid
		const int i0 = clamp(currPos.x, 0, gridSize.x-1);
		const int j0 = clamp(currPos.y, 0, gridSize.y-1);
		const int k0 = clamp(currPos.z, 0, (orig.is3D() ? (gridSize.z-1) : 1) );
		const int i1 = i0+1, j1 = j0+1, k1= (orig.is3D() ? (k0+1) : k0);
		if( (!orig.isInBounds(Vec3i(i0,j0,k0),0)) || (!orig.isInBounds(Vec3i(i1,j1,k1),0)) )
			return fwd; 

		// find min/max around source pos
		getMinMax(minv, maxv, orig(i0,j0,k0));
		getMinMax(minv, maxv, orig(i1,j0,k0));
		getMinMax(minv, maxv, orig(i0,j1,k0));
		getMinMax(minv, maxv, orig(i1,j1,k0));

		if(orig.is3D()) {
		getMinMax(minv, maxv, orig(i0,j0,k1));
		getMinMax(minv, maxv, orig(i1,j0,k1));
		getMinMax(minv, maxv, orig(i0,j1,k1));
		getMinMax(minv, maxv, orig(i1,j1,k1)); } 
	}

	dst = clamp(dst, minv, maxv);
	return dst;
}
	
//! Helper function for clamping MAC grids
template<int c> 
inline Real doClampComponentMAC(const Vec3i& gridSize, Real dst, MACGrid& orig, Real fwd, const Vec3& pos, const Vec3& vel ) 
{
	Real minv = std::numeric_limits<Real>::max(), maxv = -std::numeric_limits<Real>::max();

	// forward (and optionally) backward
	Vec3i positions[2];
	positions[0] = toVec3i(pos - vel);
	positions[1] = toVec3i(pos + vel);

	for(int l=0; l<2; ++l) {
		Vec3i& currPos = positions[l];

		// clamp forward lookup to grid
		const int i0 = clamp(currPos.x, 0, gridSize.x-1);
		const int j0 = clamp(currPos.y, 0, gridSize.y-1);
		const int k0 = clamp(currPos.z, 0, (orig.is3D() ? (gridSize.z-1) : 1) );
		const int i1 = i0+1, j1 = j0+1, k1= (orig.is3D() ? (k0+1) : k0);
		if( (!orig.isInBounds(Vec3i(i0,j0,k0),0)) || (!orig.isInBounds(Vec3i(i1,j1,k1),0)) )
			return fwd; 

		// find min/max around source pos
		getMinMax(minv, maxv, orig(i0,j0,k0)[c]);
		getMinMax(minv, maxv, orig(i1,j0,k0)[c]);
		getMinMax(minv, maxv, orig(i0,j1,k0)[c]);
		getMinMax(minv, maxv, orig(i1,j1,k0)[c]);

		if(orig.is3D()) {
		getMinMax(minv, maxv, orig(i0,j0,k1)[c]);
		getMinMax(minv, maxv, orig(i1,j0,k1)[c]);
		getMinMax(minv, maxv, orig(i0,j1,k1)[c]);
		getMinMax(minv, maxv, orig(i1,j1,k1)[c]); } 
	}

	dst = clamp(dst, minv, maxv);
	return dst;
}

//! Kernel: Clamp obtained value to min/max in source area, and reset values that point out of grid or into boundaries
//          (note - MAC grids are handled below)


template <class T>  struct MacCormackClamp : public KernelBase { MacCormackClamp(FlagGrid& flags, MACGrid& vel, Grid<T>& dst, Grid<T>& orig, Grid<T>& fwd, Real dt) :  KernelBase(&flags,1) ,flags(flags),vel(vel),dst(dst),orig(orig),fwd(fwd),dt(dt)   { run(); }  inline void op(int i, int j, int k, FlagGrid& flags, MACGrid& vel, Grid<T>& dst, Grid<T>& orig, Grid<T>& fwd, Real dt )  {
	T     dval       = dst(i,j,k);
	Vec3i gridUpper  = flags.getSize() - 1;
	
	dval = doClampComponent<T>(gridUpper, dval, orig, fwd(i,j,k), Vec3(i,j,k), vel.getCentered(i,j,k) * dt );

	// lookup forward/backward , round to closest NB
	Vec3i posFwd = toVec3i( Vec3(i,j,k) + Vec3(0.5,0.5,0.5) - vel.getCentered(i,j,k) * dt );
	Vec3i posBwd = toVec3i( Vec3(i,j,k) + Vec3(0.5,0.5,0.5) + vel.getCentered(i,j,k) * dt );
	
	// test if lookups point out of grid or into obstacle (note doClampComponent already checks sides, below is needed for valid flags access)
	if (posFwd.x < 0 || posFwd.y < 0 || posFwd.z < 0 ||
		posBwd.x < 0 || posBwd.y < 0 || posBwd.z < 0 ||
		posFwd.x > gridUpper.x || posFwd.y > gridUpper.y || ((posFwd.z > gridUpper.z)&&flags.is3D()) ||
		posBwd.x > gridUpper.x || posBwd.y > gridUpper.y || ((posBwd.z > gridUpper.z)&&flags.is3D()) ||
		flags.isObstacle(posFwd) || flags.isObstacle(posBwd) ) 
	{
		dval = fwd(i,j,k);
	}
	dst(i,j,k) = dval;
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline MACGrid& getArg1() { return vel; } typedef MACGrid type1;inline Grid<T>& getArg2() { return dst; } typedef Grid<T> type2;inline Grid<T>& getArg3() { return orig; } typedef Grid<T> type3;inline Grid<T>& getArg4() { return fwd; } typedef Grid<T> type4;inline Real& getArg5() { return dt; } typedef Real type5; void run() {  const int _maxX = maxX; const int _maxY = maxY; for (int k=minZ; k< maxZ; k++) for (int j=1; j< _maxY; j++) for (int i=1; i< _maxX; i++) op(i,j,k, flags,vel,dst,orig,fwd,dt);  } FlagGrid& flags; MACGrid& vel; Grid<T>& dst; Grid<T>& orig; Grid<T>& fwd; Real dt;   };

//! Kernel: same as MacCormackClamp above, but specialized version for MAC grids


 struct MacCormackClampMAC : public KernelBase { MacCormackClampMAC(FlagGrid& flags, MACGrid& vel, MACGrid& dst, MACGrid& orig, MACGrid& fwd, Real dt) :  KernelBase(&flags,1) ,flags(flags),vel(vel),dst(dst),orig(orig),fwd(fwd),dt(dt)   { run(); }  inline void op(int i, int j, int k, FlagGrid& flags, MACGrid& vel, MACGrid& dst, MACGrid& orig, MACGrid& fwd, Real dt )  {
	Vec3  pos(i,j,k);
	Vec3  dval       = dst(i,j,k);
	Vec3  dfwd       = fwd(i,j,k);
	Vec3i gridUpper  = flags.getSize() - 1;
	
	dval.x = doClampComponentMAC<0>(gridUpper, dval.x, orig, dfwd.x, pos, vel.getAtMACX(i,j,k) * dt);
	dval.y = doClampComponentMAC<1>(gridUpper, dval.y, orig, dfwd.y, pos, vel.getAtMACY(i,j,k) * dt);
	dval.z = doClampComponentMAC<2>(gridUpper, dval.z, orig, dfwd.z, pos, vel.getAtMACZ(i,j,k) * dt);

	// note - the MAC version currently does not check whether source points were inside an obstacle! (unlike centered version)
	// this would need to be done for each face separately to stay symmetric...

	dst(i,j,k) = dval;
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline MACGrid& getArg1() { return vel; } typedef MACGrid type1;inline MACGrid& getArg2() { return dst; } typedef MACGrid type2;inline MACGrid& getArg3() { return orig; } typedef MACGrid type3;inline MACGrid& getArg4() { return fwd; } typedef MACGrid type4;inline Real& getArg5() { return dt; } typedef Real type5; void run() {  const int _maxX = maxX; const int _maxY = maxY; for (int k=minZ; k< maxZ; k++) for (int j=1; j< _maxY; j++) for (int i=1; i< _maxX; i++) op(i,j,k, flags,vel,dst,orig,fwd,dt);  } FlagGrid& flags; MACGrid& vel; MACGrid& dst; MACGrid& orig; MACGrid& fwd; Real dt;   };

/*static inline bool isNotFluid(FlagGrid& flags, int i, int j, int k)
{
	if ( flags.isFluid(i,j,k)   ) return false;
	if ( flags.isFluid(i-1,j,k) ) return false; 
	if ( flags.isFluid(i,j-1,k) ) return false; 
	if ( flags.is3D() ) {
		if ( flags.isFluid(i,j,k-1) ) return false;
	}
	return true;
}

static inline bool isNotFluidMAC(FlagGrid& flags, int i, int j, int k)
{
	if ( flags.isFluid(i,j,k)   ) return false;
	return true;
}*/


//! template function for performing SL advection
template<class GridType> 
void fnAdvectSemiLagrange(FluidSolver* parent, FlagGrid& flags, MACGrid& vel, GridType& orig, int order, Real strength, int orderSpace) {
	typedef typename GridType::BASETYPE T;
	
	Real dt = parent->getDt();
	bool levelset = orig.getType() & GridBase::TypeLevelset;
	
	// forward step
	GridType fwd(parent);
	SemiLagrange<T> (flags, vel, fwd, orig, dt, levelset, orderSpace);
	
	if (order == 1) {
		orig.swap(fwd);
	}
	else if (order == 2) { // MacCormack
		GridType bwd(parent);
		GridType newGrid(parent);
	
		// bwd <- backwards step
		SemiLagrange<T> (flags, vel, bwd, fwd, -dt, levelset, orderSpace);
		
		// newGrid <- compute correction
		MacCormackCorrect<T> (flags, newGrid, orig, fwd, bwd, strength, levelset);
		
		// clamp values
		MacCormackClamp<T> (flags, vel, newGrid, orig, fwd, dt);
		
		orig.swap(newGrid);
	}
}

//! template function for performing SL advection: specialized version for MAC grids
template<> 
void fnAdvectSemiLagrange<MACGrid>(FluidSolver* parent, FlagGrid& flags, MACGrid& vel, MACGrid& orig, int order, Real strength, int orderSpace) {
	Real dt = parent->getDt();
	
	// forward step
	MACGrid fwd(parent);    
	SemiLagrangeMAC (flags, vel, fwd, orig, dt, orderSpace);
	
	if (orderSpace != 1) { debMsg("Warning higher order for MAC grids not yet implemented...",1); }

	if (order == 1) {
		orig.swap(fwd);
	}
	else if (order == 2) { // MacCormack 
		MACGrid bwd(parent);
		MACGrid newGrid(parent);
		
		// bwd <- backwards step
		SemiLagrangeMAC (flags, vel, bwd, fwd, -dt, orderSpace);
		
		// newGrid <- compute correction
		MacCormackCorrectMAC<Vec3> (flags, newGrid, orig, fwd, bwd, strength, false, true);
		
		// clamp values
		MacCormackClampMAC (flags, vel, newGrid, orig, fwd, dt); 
		
		orig.swap(newGrid);
	}
}

//! Perform semi-lagrangian advection of target Real- or Vec3 grid


void advectSemiLagrange(FlagGrid* flags, MACGrid* vel, GridBase* grid, int order = 1, Real strength = 1.0, int orderSpace = 1 ) {    
	assertMsg(order==1 || order==2, "AdvectSemiLagrange: Only order 1 (regular SL) and 2 (MacCormack) supported");
	
	// determine type of grid    
	if (grid->getType() & GridBase::TypeReal) {
		fnAdvectSemiLagrange< Grid<Real> >(flags->getParent(), *flags, *vel, *((Grid<Real>*) grid), order, strength, orderSpace);
	}
	else if (grid->getType() & GridBase::TypeMAC) {    
		fnAdvectSemiLagrange< MACGrid >(flags->getParent(), *flags, *vel, *((MACGrid*) grid), order, strength, orderSpace);
	}
	else if (grid->getType() & GridBase::TypeVec3) {    
		fnAdvectSemiLagrange< Grid<Vec3> >(flags->getParent(), *flags, *vel, *((Grid<Vec3>*) grid), order, strength, orderSpace);
	}
	else
		errMsg("AdvectSemiLagrange: Grid Type is not supported (only Real, Vec3, MAC, Levelset)");    
} static PyObject* _W_0 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); pbPreparePlugin(parent, "advectSemiLagrange" ); PyObject *_retval = 0; { ArgLocker _lock; FlagGrid* flags = _args.getPtr<FlagGrid >("flags",0,&_lock); MACGrid* vel = _args.getPtr<MACGrid >("vel",1,&_lock); GridBase* grid = _args.getPtr<GridBase >("grid",2,&_lock); int order = _args.getOpt<int >("order",3,1,&_lock); Real strength = _args.getOpt<Real >("strength",4,1.0,&_lock); int orderSpace = _args.getOpt<int >("orderSpace",5,1 ,&_lock);   _retval = getPyNone(); advectSemiLagrange(flags,vel,grid,order,strength,orderSpace);  _args.check(); } pbFinalizePlugin(parent,"advectSemiLagrange" ); return _retval; } catch(std::exception& e) { pbSetError("advectSemiLagrange",e.what()); return 0; } } static const Pb::Register _RP_advectSemiLagrange ("","advectSemiLagrange",_W_0); 

} // end namespace DDF 



