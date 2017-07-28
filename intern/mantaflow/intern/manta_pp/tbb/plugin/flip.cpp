




// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).




#line 1 "/Users/sebbas/Developer/Mantaflow/mantaflowDevelop/mantaflowgit/source/plugin/flip.cpp"
/******************************************************************************
 *
 * MantaFlow fluid solver framework 
 * Copyright 2011 Tobias Pfaff, Nils Thuerey 
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL) 
 * http://www.gnu.org/licenses
 *
 * FLIP (fluid implicit particles)
 * for use with particle data fields
 *
 ******************************************************************************/

#include "particle.h"
#include "grid.h"
#include "randomstream.h"
#include "levelset.h"

using namespace std;
namespace Manta {



// init

//! note - this is a simplified version , sampleLevelsetWithParticles has more functionality


void sampleFlagsWithParticles( FlagGrid& flags, BasicParticleSystem& parts, int discretization, Real randomness ) {
	bool is3D = flags.is3D();
	Real jlen = randomness / discretization;
	Vec3 disp (1.0 / discretization, 1.0 / discretization, 1.0/discretization);
	RandomStream mRand(9832);
 
	FOR_IJK_BND(flags, 0) {
		if ( flags.isObstacle(i,j,k) ) continue;
		if ( flags.isFluid(i,j,k) ) {
			Vec3 pos (i,j,k);
			for (int dk=0; dk<(is3D ? discretization : 1); dk++)
			for (int dj=0; dj<discretization; dj++)
			for (int di=0; di<discretization; di++) {
				Vec3 subpos = pos + disp * Vec3(0.5+di, 0.5+dj, 0.5+dk);
				subpos += jlen * (Vec3(1,1,1) - 2.0 * mRand.getVec3());
				if(!is3D) subpos[2] = 0.5; 
				parts.addBuffered( subpos);
			}
		}
	}
	parts.insertBufferedParticles();
} static PyObject* _W_0 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "sampleFlagsWithParticles" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",0,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",1,&_lock); int discretization = _args.get<int >("discretization",2,&_lock); Real randomness = _args.get<Real >("randomness",3,&_lock);   _retval = getPyNone(); sampleFlagsWithParticles(flags,parts,discretization,randomness);  _args.check(); } pbFinalizePlugin(parent,"sampleFlagsWithParticles", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("sampleFlagsWithParticles",e.what()); return 0; } } static const Pb::Register _RP_sampleFlagsWithParticles ("","sampleFlagsWithParticles",_W_0);  extern "C" { void PbRegister_sampleFlagsWithParticles() { KEEP_UNUSED(_RP_sampleFlagsWithParticles); } } 

//! sample a level set with particles, use reset to clear the particle buffer,
//! and skipEmpty for a continuous inflow (in the latter case, only empty cells will
//! be re-filled once they empty when calling sampleLevelsetWithParticles during 
//! the main loop).


void sampleLevelsetWithParticles( LevelsetGrid& phi, FlagGrid& flags, BasicParticleSystem& parts, int discretization, Real randomness, bool reset=false, bool refillEmpty=false ) {
	bool is3D = phi.is3D();
	Real jlen = randomness / discretization;
	Vec3 disp (1.0 / discretization, 1.0 / discretization, 1.0/discretization);
	RandomStream mRand(9832);
 
	if(reset) {
		parts.clear(); 
		parts.doCompress();
	}

	FOR_IJK_BND(phi, 0) {
		if ( flags.isObstacle(i,j,k) ) continue;
		if ( refillEmpty && flags.isFluid(i,j,k) ) continue;
		if ( phi(i,j,k) < 1.733 ) {
			Vec3 pos (i,j,k);
			for (int dk=0; dk<(is3D ? discretization : 1); dk++)
			for (int dj=0; dj<discretization; dj++)
			for (int di=0; di<discretization; di++) {
				Vec3 subpos = pos + disp * Vec3(0.5+di, 0.5+dj, 0.5+dk);
				subpos += jlen * (Vec3(1,1,1) - 2.0 * mRand.getVec3());
				if(!is3D) subpos[2] = 0.5; 
				if( phi.getInterpolated(subpos) > 0. ) continue; 
				parts.addBuffered( subpos);
			}
		}
	}

	parts.insertBufferedParticles();
} static PyObject* _W_1 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "sampleLevelsetWithParticles" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; LevelsetGrid& phi = *_args.getPtr<LevelsetGrid >("phi",0,&_lock); FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",1,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",2,&_lock); int discretization = _args.get<int >("discretization",3,&_lock); Real randomness = _args.get<Real >("randomness",4,&_lock); bool reset = _args.getOpt<bool >("reset",5,false,&_lock); bool refillEmpty = _args.getOpt<bool >("refillEmpty",6,false ,&_lock);   _retval = getPyNone(); sampleLevelsetWithParticles(phi,flags,parts,discretization,randomness,reset,refillEmpty);  _args.check(); } pbFinalizePlugin(parent,"sampleLevelsetWithParticles", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("sampleLevelsetWithParticles",e.what()); return 0; } } static const Pb::Register _RP_sampleLevelsetWithParticles ("","sampleLevelsetWithParticles",_W_1);  extern "C" { void PbRegister_sampleLevelsetWithParticles() { KEEP_UNUSED(_RP_sampleLevelsetWithParticles); } } 

//! mark fluid cells and helpers
 struct knClearFluidFLags : public KernelBase { knClearFluidFLags(FlagGrid& flags, int dummy=0) :  KernelBase(&flags,0) ,flags(flags),dummy(dummy)   { runMessage(); run(); }  inline void op(int i, int j, int k, FlagGrid& flags, int dummy=0 ) const {
	if (flags.isFluid(i,j,k)) {
		flags(i,j,k) = (flags(i,j,k) | FlagGrid::TypeEmpty) & ~FlagGrid::TypeFluid;
	}
}   inline FlagGrid& getArg0() { return flags; } typedef FlagGrid type0;inline int& getArg1() { return dummy; } typedef int type1; void runMessage() { debMsg("Executing kernel knClearFluidFLags ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {  const int _maxX = maxX; const int _maxY = maxY; if (maxZ>1) { for (int k=__r.begin(); k!=(int)__r.end(); k++) for (int j=0; j<_maxY; j++) for (int i=0; i<_maxX; i++) op(i,j,k,flags,dummy); } else { const int k=0; for (int j=__r.begin(); j!=(int)__r.end(); j++) for (int i=0; i<_maxX; i++) op(i,j,k,flags,dummy); }  } void run() {  if (maxZ>1) tbb::parallel_for (tbb::blocked_range<IndexInt>(minZ, maxZ), *this); else tbb::parallel_for (tbb::blocked_range<IndexInt>(0, maxY), *this);  }  FlagGrid& flags; int dummy;   };

 struct knSetNbObstacle : public KernelBase { knSetNbObstacle(FlagGrid& nflags, const FlagGrid& flags, const Grid<Real>& phiObs) :  KernelBase(&nflags,1) ,nflags(nflags),flags(flags),phiObs(phiObs)   { runMessage(); run(); }  inline void op(int i, int j, int k, FlagGrid& nflags, const FlagGrid& flags, const Grid<Real>& phiObs ) const {
	if ( phiObs(i,j,k)>0. ) return;
	if (flags.isEmpty(i,j,k)) {
		bool set=false;
		if( (flags.isFluid(i-1,j,k)) && (phiObs(i+1,j,k)<=0.) ) set=true;
		if( (flags.isFluid(i+1,j,k)) && (phiObs(i-1,j,k)<=0.) ) set=true;
		if( (flags.isFluid(i,j-1,k)) && (phiObs(i,j+1,k)<=0.) ) set=true;
		if( (flags.isFluid(i,j+1,k)) && (phiObs(i,j-1,k)<=0.) ) set=true;
		if(flags.is3D()) {
		if( (flags.isFluid(i,j,k-1)) && (phiObs(i,j,k+1)<=0.) ) set=true;
		if( (flags.isFluid(i,j,k+1)) && (phiObs(i,j,k-1)<=0.) ) set=true;
		}
		if(set) nflags(i,j,k) = (flags(i,j,k) | FlagGrid::TypeFluid) & ~FlagGrid::TypeEmpty;
	}
}   inline FlagGrid& getArg0() { return nflags; } typedef FlagGrid type0;inline const FlagGrid& getArg1() { return flags; } typedef FlagGrid type1;inline const Grid<Real>& getArg2() { return phiObs; } typedef Grid<Real> type2; void runMessage() { debMsg("Executing kernel knSetNbObstacle ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {  const int _maxX = maxX; const int _maxY = maxY; if (maxZ>1) { for (int k=__r.begin(); k!=(int)__r.end(); k++) for (int j=1; j<_maxY; j++) for (int i=1; i<_maxX; i++) op(i,j,k,nflags,flags,phiObs); } else { const int k=0; for (int j=__r.begin(); j!=(int)__r.end(); j++) for (int i=1; i<_maxX; i++) op(i,j,k,nflags,flags,phiObs); }  } void run() {  if (maxZ>1) tbb::parallel_for (tbb::blocked_range<IndexInt>(minZ, maxZ), *this); else tbb::parallel_for (tbb::blocked_range<IndexInt>(1, maxY), *this);  }  FlagGrid& nflags; const FlagGrid& flags; const Grid<Real>& phiObs;   };
void markFluidCells(BasicParticleSystem& parts, FlagGrid& flags, Grid<Real>* phiObs = NULL) {
	// remove all fluid cells
	knClearFluidFLags(flags, 0);
	
	// mark all particles in flaggrid as fluid
	for(IndexInt idx=0;idx<parts.size();idx++) {
		if (!parts.isActive(idx)) continue;
		Vec3i p = toVec3i( parts.getPos(idx) );
		if (flags.isInBounds(p) && flags.isEmpty(p))
			flags(p) = (flags(p) | FlagGrid::TypeFluid) & ~FlagGrid::TypeEmpty;
	}

	// special for second order obstacle BCs, check empty cells in boundary region
	if(phiObs) {
		FlagGrid tmp(flags);
		knSetNbObstacle(tmp, flags, *phiObs);
		flags.swap(tmp);
	}
} static PyObject* _W_2 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "markFluidCells" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",0,&_lock); FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",1,&_lock); Grid<Real>* phiObs = _args.getPtrOpt<Grid<Real> >("phiObs",2,NULL,&_lock);   _retval = getPyNone(); markFluidCells(parts,flags,phiObs);  _args.check(); } pbFinalizePlugin(parent,"markFluidCells", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("markFluidCells",e.what()); return 0; } } static const Pb::Register _RP_markFluidCells ("","markFluidCells",_W_2);  extern "C" { void PbRegister_markFluidCells() { KEEP_UNUSED(_RP_markFluidCells); } } 

// for testing purposes only...
void testInitGridWithPos(Grid<Real>& grid) {
	FOR_IJK(grid) { grid(i,j,k) = norm( Vec3(i,j,k) ); }
} static PyObject* _W_3 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "testInitGridWithPos" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; Grid<Real>& grid = *_args.getPtr<Grid<Real> >("grid",0,&_lock);   _retval = getPyNone(); testInitGridWithPos(grid);  _args.check(); } pbFinalizePlugin(parent,"testInitGridWithPos", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("testInitGridWithPos",e.what()); return 0; } } static const Pb::Register _RP_testInitGridWithPos ("","testInitGridWithPos",_W_3);  extern "C" { void PbRegister_testInitGridWithPos() { KEEP_UNUSED(_RP_testInitGridWithPos); } } 



//! helper to calculate particle radius factor to cover the diagonal of a cell in 2d/3d
inline Real calculateRadiusFactor(Grid<Real>& grid, Real factor) {
	return (grid.is3D() ? sqrt(3.) : sqrt(2.) ) * (factor+.01); // note, a 1% safety factor is added here
} 

//! re-sample particles based on an input levelset 
// optionally skip seeding new particles in "exclude" SDF



void adjustNumber( BasicParticleSystem& parts, MACGrid& vel, FlagGrid& flags, int minParticles, int maxParticles, LevelsetGrid& phi, Real radiusFactor=1. , Real narrowBand=-1. , Grid<Real>* exclude=NULL ) {
	// which levelset to use as threshold
	const Real SURFACE_LS = -1.0 * calculateRadiusFactor(phi, radiusFactor);
	Grid<int> tmp( vel.getParent() );
	std::ostringstream out;

	// count particles in cells, and delete excess particles
	for (IndexInt idx=0; idx<(int)parts.size(); idx++) {
		if (parts.isActive(idx)) {
			Vec3i p = toVec3i( parts.getPos(idx) );
			if (!tmp.isInBounds(p) ) {
				parts.kill(idx); // out of domain, remove
				continue;
			}

			Real phiv = phi.getInterpolated( parts.getPos(idx) );
			if( phiv > 0 ) { parts.kill(idx); continue; }
			if( narrowBand>0. && phiv < -narrowBand) { parts.kill(idx); continue; }

			bool atSurface = false;
			if (phiv > SURFACE_LS) atSurface = true;
			int num = tmp(p);
			
			// dont delete particles in non fluid cells here, the particles are "always right"
			if ( num > maxParticles && (!atSurface) ) {
				parts.kill(idx);
			} else {
				tmp(p) = num+1;
			}
		}
	}

	// seed new particles
	RandomStream mRand(9832);
	FOR_IJK(tmp) {
		int cnt = tmp(i,j,k);
		
		// skip cells near surface
		if (phi(i,j,k) > SURFACE_LS) continue;
		if( narrowBand>0. && phi(i,j,k) < -narrowBand ) { continue; }
		if( exclude && ( (*exclude)(i,j,k) < 0.) ) { continue; }

		if (flags.isFluid(i,j,k) && cnt < minParticles) {
			for (int m=cnt; m < minParticles; m++) { 
				Vec3 pos = Vec3(i,j,k) + mRand.getVec3();
				//Vec3 pos (i + 0.5, j + 0.5, k + 0.5); // cell center
				parts.addBuffered( pos ); 
			}
		}
	}

	parts.doCompress();
	parts.insertBufferedParticles();
} static PyObject* _W_4 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "adjustNumber" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",0,&_lock); MACGrid& vel = *_args.getPtr<MACGrid >("vel",1,&_lock); FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",2,&_lock); int minParticles = _args.get<int >("minParticles",3,&_lock); int maxParticles = _args.get<int >("maxParticles",4,&_lock); LevelsetGrid& phi = *_args.getPtr<LevelsetGrid >("phi",5,&_lock); Real radiusFactor = _args.getOpt<Real >("radiusFactor",6,1. ,&_lock); Real narrowBand = _args.getOpt<Real >("narrowBand",7,-1. ,&_lock); Grid<Real>* exclude = _args.getPtrOpt<Grid<Real> >("exclude",8,NULL ,&_lock);   _retval = getPyNone(); adjustNumber(parts,vel,flags,minParticles,maxParticles,phi,radiusFactor,narrowBand,exclude);  _args.check(); } pbFinalizePlugin(parent,"adjustNumber", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("adjustNumber",e.what()); return 0; } } static const Pb::Register _RP_adjustNumber ("","adjustNumber",_W_4);  extern "C" { void PbRegister_adjustNumber() { KEEP_UNUSED(_RP_adjustNumber); } } 

// simple and slow helper conversion to show contents of int grids like a real grid in the ui
// (use eg to quickly display contents of the particle-index grid)

void debugIntToReal( Grid<int>& source, Grid<Real>& dest, Real factor=1. ) {
	FOR_IJK( source ) { dest(i,j,k) = (Real)source(i,j,k) * factor; }
} static PyObject* _W_5 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "debugIntToReal" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; Grid<int>& source = *_args.getPtr<Grid<int> >("source",0,&_lock); Grid<Real>& dest = *_args.getPtr<Grid<Real> >("dest",1,&_lock); Real factor = _args.getOpt<Real >("factor",2,1. ,&_lock);   _retval = getPyNone(); debugIntToReal(source,dest,factor);  _args.check(); } pbFinalizePlugin(parent,"debugIntToReal", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("debugIntToReal",e.what()); return 0; } } static const Pb::Register _RP_debugIntToReal ("","debugIntToReal",_W_5);  extern "C" { void PbRegister_debugIntToReal() { KEEP_UNUSED(_RP_debugIntToReal); } } 

// build a grid that contains indices for a particle system
// the particles in a cell i,j,k are particles[index(i,j,k)] to particles[index(i+1,j,k)-1]
// (ie,  particles[index(i+1,j,k)] alreadu belongs to cell i+1,j,k)


void gridParticleIndex( BasicParticleSystem& parts, ParticleIndexSystem& indexSys, FlagGrid& flags, Grid<int>& index, Grid<int>* counter=NULL) {
	bool delCounter = false;
	if(!counter) { counter = new Grid<int>(  flags.getParent() ); delCounter=true; }
	else         { counter->clear(); }
	
	// count particles in cells, and delete excess particles
	index.clear();
	int inactive = 0;
	for (IndexInt idx=0; idx<(IndexInt)parts.size(); idx++) {
		if (parts.isActive(idx)) {
			// check index for validity...
			Vec3i p = toVec3i( parts.getPos(idx) );
			if (! index.isInBounds(p)) { inactive++; continue; }

			index(p)++;
		} else {
			inactive++;
		}
	}

	// note - this one might be smaller...
	indexSys.resize( parts.size()-inactive );

	// convert per cell number to continuous index
	IndexInt idx=0;
	FOR_IJK( index ) {
		int num = index(i,j,k);
		index(i,j,k) = idx;
		idx += num;
	}

	// add particles to indexed array, we still need a per cell particle counter
	for (IndexInt idx=0; idx<(IndexInt)parts.size(); idx++) {
		if (!parts.isActive(idx)) continue;
		Vec3i p = toVec3i( parts.getPos(idx) );
		if (! index.isInBounds(p)) { continue; }

		// initialize position and index into original array
		//indexSys[ index(p)+(*counter)(p) ].pos        = parts[idx].pos;
		indexSys[ index(p)+(*counter)(p) ].sourceIndex = idx;
		(*counter)(p)++;
	}

	if(delCounter) delete counter;
} static PyObject* _W_6 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "gridParticleIndex" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",0,&_lock); ParticleIndexSystem& indexSys = *_args.getPtr<ParticleIndexSystem >("indexSys",1,&_lock); FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",2,&_lock); Grid<int>& index = *_args.getPtr<Grid<int> >("index",3,&_lock); Grid<int>* counter = _args.getPtrOpt<Grid<int> >("counter",4,NULL,&_lock);   _retval = getPyNone(); gridParticleIndex(parts,indexSys,flags,index,counter);  _args.check(); } pbFinalizePlugin(parent,"gridParticleIndex", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("gridParticleIndex",e.what()); return 0; } } static const Pb::Register _RP_gridParticleIndex ("","gridParticleIndex",_W_6);  extern "C" { void PbRegister_gridParticleIndex() { KEEP_UNUSED(_RP_gridParticleIndex); } } 




 struct ComputeUnionLevelsetPindex : public KernelBase { ComputeUnionLevelsetPindex(Grid<int>& index, BasicParticleSystem& parts, ParticleIndexSystem& indexSys, LevelsetGrid& phi, Real radius=1.) :  KernelBase(&index,0) ,index(index),parts(parts),indexSys(indexSys),phi(phi),radius(radius)   { runMessage(); run(); }  inline void op(int i, int j, int k, Grid<int>& index, BasicParticleSystem& parts, ParticleIndexSystem& indexSys, LevelsetGrid& phi, Real radius=1. ) const {
	const Vec3 gridPos = Vec3(i,j,k) + Vec3(0.5); // shifted by half cell
	Real phiv = radius * 1.0;  // outside

	int r  = int(radius) + 1;
	int rZ = phi.is3D() ? r : 0;
	for(int zj=k-rZ; zj<=k+rZ; zj++) 
	for(int yj=j-r ; yj<=j+r ; yj++) 
	for(int xj=i-r ; xj<=i+r ; xj++) {
		if (!phi.isInBounds(Vec3i(xj,yj,zj))) continue;

		// note, for the particle indices in indexSys the access is periodic (ie, dont skip for eg inBounds(sx,10,10)
		IndexInt isysIdxS = index.index(xj,yj,zj);
		IndexInt pStart = index(isysIdxS), pEnd=0;
		if(phi.isInBounds(isysIdxS+1)) pEnd = index(isysIdxS+1);
		else                           pEnd = indexSys.size();

		// now loop over particles in cell
		for(IndexInt p=pStart; p<pEnd; ++p) {
			const IndexInt psrc = indexSys[p].sourceIndex;
			const Vec3 pos = parts[psrc].pos; 
			phiv = std::min( phiv , fabs( norm(gridPos-pos) )-radius );
		}
	}
	phi(i,j,k) = phiv;
}   inline Grid<int>& getArg0() { return index; } typedef Grid<int> type0;inline BasicParticleSystem& getArg1() { return parts; } typedef BasicParticleSystem type1;inline ParticleIndexSystem& getArg2() { return indexSys; } typedef ParticleIndexSystem type2;inline LevelsetGrid& getArg3() { return phi; } typedef LevelsetGrid type3;inline Real& getArg4() { return radius; } typedef Real type4; void runMessage() { debMsg("Executing kernel ComputeUnionLevelsetPindex ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {  const int _maxX = maxX; const int _maxY = maxY; if (maxZ>1) { for (int k=__r.begin(); k!=(int)__r.end(); k++) for (int j=0; j<_maxY; j++) for (int i=0; i<_maxX; i++) op(i,j,k,index,parts,indexSys,phi,radius); } else { const int k=0; for (int j=__r.begin(); j!=(int)__r.end(); j++) for (int i=0; i<_maxX; i++) op(i,j,k,index,parts,indexSys,phi,radius); }  } void run() {  if (maxZ>1) tbb::parallel_for (tbb::blocked_range<IndexInt>(minZ, maxZ), *this); else tbb::parallel_for (tbb::blocked_range<IndexInt>(0, maxY), *this);  }  Grid<int>& index; BasicParticleSystem& parts; ParticleIndexSystem& indexSys; LevelsetGrid& phi; Real radius;   };
 


void unionParticleLevelset( BasicParticleSystem& parts, ParticleIndexSystem& indexSys, FlagGrid& flags, Grid<int>& index, LevelsetGrid& phi, Real radiusFactor=1. ) {
	// use half a cell diagonal as base radius
	const Real radius = 0.5 * calculateRadiusFactor(phi, radiusFactor);
	// no reset of phi necessary here 
	ComputeUnionLevelsetPindex(index, parts, indexSys, phi, radius);

	phi.setBound(0.5, 0);
} static PyObject* _W_7 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "unionParticleLevelset" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",0,&_lock); ParticleIndexSystem& indexSys = *_args.getPtr<ParticleIndexSystem >("indexSys",1,&_lock); FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",2,&_lock); Grid<int>& index = *_args.getPtr<Grid<int> >("index",3,&_lock); LevelsetGrid& phi = *_args.getPtr<LevelsetGrid >("phi",4,&_lock); Real radiusFactor = _args.getOpt<Real >("radiusFactor",5,1. ,&_lock);   _retval = getPyNone(); unionParticleLevelset(parts,indexSys,flags,index,phi,radiusFactor);  _args.check(); } pbFinalizePlugin(parent,"unionParticleLevelset", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("unionParticleLevelset",e.what()); return 0; } } static const Pb::Register _RP_unionParticleLevelset ("","unionParticleLevelset",_W_7);  extern "C" { void PbRegister_unionParticleLevelset() { KEEP_UNUSED(_RP_unionParticleLevelset); } } 






 struct ComputeAveragedLevelsetWeight : public KernelBase { ComputeAveragedLevelsetWeight(BasicParticleSystem& parts, Grid<int>& index, ParticleIndexSystem& indexSys, LevelsetGrid& phi, Real radius=1.) :  KernelBase(&index,0) ,parts(parts),index(index),indexSys(indexSys),phi(phi),radius(radius)   { runMessage(); run(); }  inline void op(int i, int j, int k, BasicParticleSystem& parts, Grid<int>& index, ParticleIndexSystem& indexSys, LevelsetGrid& phi, Real radius=1. ) const {
	const Vec3 gridPos = Vec3(i,j,k) + Vec3(0.5); // shifted by half cell
	Real phiv = radius * 1.0; // outside 

	// loop over neighborhood, similar to ComputeUnionLevelsetPindex
	const Real sradiusInv = 1. / (4. * radius * radius) ;
	int   r = int(1. * radius) + 1;
	int   rZ = phi.is3D() ? r : 0;
	// accumulators
	Real  wacc = 0.;
	Vec3  pacc = Vec3(0.);
	Real  racc = 0.;

	for(int zj=k-rZ; zj<=k+rZ; zj++) 
	for(int yj=j-r ; yj<=j+r ; yj++) 
	for(int xj=i-r ; xj<=i+r ; xj++) {
		if (! phi.isInBounds(Vec3i(xj,yj,zj)) ) continue;

		IndexInt isysIdxS = index.index(xj,yj,zj);
		IndexInt pStart = index(isysIdxS), pEnd=0;
		if(phi.isInBounds(isysIdxS+1)) pEnd = index(isysIdxS+1);
		else                           pEnd = indexSys.size();
		for(IndexInt p=pStart; p<pEnd; ++p) {
			IndexInt   psrc = indexSys[p].sourceIndex;
			Vec3  pos  = parts[psrc].pos; 
			Real  s    = normSquare(gridPos-pos) * sradiusInv;
			//Real  w = std::max(0., cubed(1.-s) );
			Real  w = std::max(0., (1.-s) ); // a bit smoother
			wacc += w;
			racc += radius * w;
			pacc += pos    * w;
		} 
	}

	if(wacc > VECTOR_EPSILON) {
		racc /= wacc;
		pacc /= wacc;
		phiv = fabs( norm(gridPos-pacc) )-racc;
	}
	phi(i,j,k) = phiv;
}   inline BasicParticleSystem& getArg0() { return parts; } typedef BasicParticleSystem type0;inline Grid<int>& getArg1() { return index; } typedef Grid<int> type1;inline ParticleIndexSystem& getArg2() { return indexSys; } typedef ParticleIndexSystem type2;inline LevelsetGrid& getArg3() { return phi; } typedef LevelsetGrid type3;inline Real& getArg4() { return radius; } typedef Real type4; void runMessage() { debMsg("Executing kernel ComputeAveragedLevelsetWeight ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {  const int _maxX = maxX; const int _maxY = maxY; if (maxZ>1) { for (int k=__r.begin(); k!=(int)__r.end(); k++) for (int j=0; j<_maxY; j++) for (int i=0; i<_maxX; i++) op(i,j,k,parts,index,indexSys,phi,radius); } else { const int k=0; for (int j=__r.begin(); j!=(int)__r.end(); j++) for (int i=0; i<_maxX; i++) op(i,j,k,parts,index,indexSys,phi,radius); }  } void run() {  if (maxZ>1) tbb::parallel_for (tbb::blocked_range<IndexInt>(minZ, maxZ), *this); else tbb::parallel_for (tbb::blocked_range<IndexInt>(0, maxY), *this);  }  BasicParticleSystem& parts; Grid<int>& index; ParticleIndexSystem& indexSys; LevelsetGrid& phi; Real radius;   };

template<class T> T smoothingValue(Grid<T> val, int i, int j, int k, T center) {
	return val(i,j,k);
}

// smoothing, and  

template <class T>  struct knSmoothGrid : public KernelBase { knSmoothGrid(Grid<T>& me, Grid<T>& tmp, Real factor) :  KernelBase(&me,1) ,me(me),tmp(tmp),factor(factor)   { runMessage(); run(); }  inline void op(int i, int j, int k, Grid<T>& me, Grid<T>& tmp, Real factor ) const {
	T val = me(i,j,k) + 
			me(i+1,j,k) + me(i-1,j,k) + 
			me(i,j+1,k) + me(i,j-1,k) ;
	if(me.is3D()) {
		val += me(i,j,k+1) + me(i,j,k-1);
	}
	tmp(i,j,k) = val * factor;
}   inline Grid<T>& getArg0() { return me; } typedef Grid<T> type0;inline Grid<T>& getArg1() { return tmp; } typedef Grid<T> type1;inline Real& getArg2() { return factor; } typedef Real type2; void runMessage() { debMsg("Executing kernel knSmoothGrid ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {  const int _maxX = maxX; const int _maxY = maxY; if (maxZ>1) { for (int k=__r.begin(); k!=(int)__r.end(); k++) for (int j=1; j<_maxY; j++) for (int i=1; i<_maxX; i++) op(i,j,k,me,tmp,factor); } else { const int k=0; for (int j=__r.begin(); j!=(int)__r.end(); j++) for (int i=1; i<_maxX; i++) op(i,j,k,me,tmp,factor); }  } void run() {  if (maxZ>1) tbb::parallel_for (tbb::blocked_range<IndexInt>(minZ, maxZ), *this); else tbb::parallel_for (tbb::blocked_range<IndexInt>(1, maxY), *this);  }  Grid<T>& me; Grid<T>& tmp; Real factor;   };


template <class T>  struct knSmoothGridNeg : public KernelBase { knSmoothGridNeg(Grid<T>& me, Grid<T>& tmp, Real factor) :  KernelBase(&me,1) ,me(me),tmp(tmp),factor(factor)   { runMessage(); run(); }  inline void op(int i, int j, int k, Grid<T>& me, Grid<T>& tmp, Real factor ) const {
	T val = me(i,j,k) + 
			me(i+1,j,k) + me(i-1,j,k) + 
			me(i,j+1,k) + me(i,j-1,k) ;
	if(me.is3D()) {
		val += me(i,j,k+1) + me(i,j,k-1);
	}
	val *= factor;
	if(val<tmp(i,j,k)) tmp(i,j,k) = val;
	else               tmp(i,j,k) = me(i,j,k);
}   inline Grid<T>& getArg0() { return me; } typedef Grid<T> type0;inline Grid<T>& getArg1() { return tmp; } typedef Grid<T> type1;inline Real& getArg2() { return factor; } typedef Real type2; void runMessage() { debMsg("Executing kernel knSmoothGridNeg ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {  const int _maxX = maxX; const int _maxY = maxY; if (maxZ>1) { for (int k=__r.begin(); k!=(int)__r.end(); k++) for (int j=1; j<_maxY; j++) for (int i=1; i<_maxX; i++) op(i,j,k,me,tmp,factor); } else { const int k=0; for (int j=__r.begin(); j!=(int)__r.end(); j++) for (int i=1; i<_maxX; i++) op(i,j,k,me,tmp,factor); }  } void run() {  if (maxZ>1) tbb::parallel_for (tbb::blocked_range<IndexInt>(minZ, maxZ), *this); else tbb::parallel_for (tbb::blocked_range<IndexInt>(1, maxY), *this);  }  Grid<T>& me; Grid<T>& tmp; Real factor;   };

 



void averagedParticleLevelset( BasicParticleSystem& parts, ParticleIndexSystem& indexSys, FlagGrid& flags, Grid<int>& index, LevelsetGrid& phi, Real radiusFactor=1. , int smoothen=1 , int smoothenNeg=1 ) {
	// use half a cell diagonal as base radius
	const Real radius = 0.5 * calculateRadiusFactor(phi, radiusFactor); 
	ComputeAveragedLevelsetWeight(parts,  index, indexSys, phi, radius);

	// post-process level-set
	for(int i=0; i<std::max(smoothen,smoothenNeg); ++i) {
		LevelsetGrid tmp(flags.getParent());
		if(i<smoothen) { 
			knSmoothGrid    <Real> (phi,tmp, 1./(phi.is3D() ? 7. : 5.) );
			phi.swap(tmp);
		}
		if(i<smoothenNeg) { 
			knSmoothGridNeg <Real> (phi,tmp, 1./(phi.is3D() ? 7. : 5.) );
			phi.swap(tmp);
		}
	} 
	phi.setBound(0.5, 0);
} static PyObject* _W_8 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "averagedParticleLevelset" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",0,&_lock); ParticleIndexSystem& indexSys = *_args.getPtr<ParticleIndexSystem >("indexSys",1,&_lock); FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",2,&_lock); Grid<int>& index = *_args.getPtr<Grid<int> >("index",3,&_lock); LevelsetGrid& phi = *_args.getPtr<LevelsetGrid >("phi",4,&_lock); Real radiusFactor = _args.getOpt<Real >("radiusFactor",5,1. ,&_lock); int smoothen = _args.getOpt<int >("smoothen",6,1 ,&_lock); int smoothenNeg = _args.getOpt<int >("smoothenNeg",7,1 ,&_lock);   _retval = getPyNone(); averagedParticleLevelset(parts,indexSys,flags,index,phi,radiusFactor,smoothen,smoothenNeg);  _args.check(); } pbFinalizePlugin(parent,"averagedParticleLevelset", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("averagedParticleLevelset",e.what()); return 0; } } static const Pb::Register _RP_averagedParticleLevelset ("","averagedParticleLevelset",_W_8);  extern "C" { void PbRegister_averagedParticleLevelset() { KEEP_UNUSED(_RP_averagedParticleLevelset); } } 




 struct knPushOutofObs : public KernelBase { knPushOutofObs(BasicParticleSystem& parts, FlagGrid& flags, Grid<Real>& phiObs, Real shift=0.05, Real thresh=0.) :  KernelBase(parts.size()) ,parts(parts),flags(flags),phiObs(phiObs),shift(shift),thresh(thresh)   { runMessage(); run(); }   inline void op(IndexInt idx, BasicParticleSystem& parts, FlagGrid& flags, Grid<Real>& phiObs, Real shift=0.05, Real thresh=0. ) const {
	if (!parts.isActive(idx)) return;
	Vec3i p = toVec3i( parts.getPos(idx) );

	if (!flags.isInBounds(p)) return;
	Real v = phiObs.getInterpolated(parts.getPos(idx));
	if(v < thresh) {
		Vec3 grad = getGradient( phiObs, p.x,p.y,p.z );
		if( normalize(grad) < VECTOR_EPSILON ) return;
		parts.setPos(idx, parts.getPos(idx) + shift * grad );
	}
}    inline BasicParticleSystem& getArg0() { return parts; } typedef BasicParticleSystem type0;inline FlagGrid& getArg1() { return flags; } typedef FlagGrid type1;inline Grid<Real>& getArg2() { return phiObs; } typedef Grid<Real> type2;inline Real& getArg3() { return shift; } typedef Real type3;inline Real& getArg4() { return thresh; } typedef Real type4; void runMessage() { debMsg("Executing kernel knPushOutofObs ", 3); debMsg("Kernel range" <<  " size "<<  size  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {   for (IndexInt idx=__r.begin(); idx!=(IndexInt)__r.end(); idx++) op(idx, parts,flags,phiObs,shift,thresh);   } void run() {   tbb::parallel_for (tbb::blocked_range<IndexInt>(0, size), *this);   }  BasicParticleSystem& parts; FlagGrid& flags; Grid<Real>& phiObs; Real shift; Real thresh;   };
//! slightly push particles out of obstacle levelset
void pushOutofObs(BasicParticleSystem& parts, FlagGrid& flags, Grid<Real>& phiObs, Real shift=0.05, Real thresh=0.) {
	knPushOutofObs(parts, flags, phiObs, shift, thresh);
} static PyObject* _W_9 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "pushOutofObs" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",0,&_lock); FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",1,&_lock); Grid<Real>& phiObs = *_args.getPtr<Grid<Real> >("phiObs",2,&_lock); Real shift = _args.getOpt<Real >("shift",3,0.05,&_lock); Real thresh = _args.getOpt<Real >("thresh",4,0.,&_lock);   _retval = getPyNone(); pushOutofObs(parts,flags,phiObs,shift,thresh);  _args.check(); } pbFinalizePlugin(parent,"pushOutofObs", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("pushOutofObs",e.what()); return 0; } } static const Pb::Register _RP_pushOutofObs ("","pushOutofObs",_W_9);  extern "C" { void PbRegister_pushOutofObs() { KEEP_UNUSED(_RP_pushOutofObs); } } 


//******************************************************************************
// grid interpolation functions


template <class T>  struct knSafeDivReal : public KernelBase { knSafeDivReal(Grid<T>& me, const Grid<Real>& other, Real cutoff=VECTOR_EPSILON) :  KernelBase(&me,0) ,me(me),other(other),cutoff(cutoff)   { runMessage(); run(); }   inline void op(IndexInt idx, Grid<T>& me, const Grid<Real>& other, Real cutoff=VECTOR_EPSILON ) const { 
	if(other[idx]<cutoff) {
		me[idx] = 0.;
	} else {
		T div( other[idx] );
		me[idx] = safeDivide(me[idx], div ); 
	}
}    inline Grid<T>& getArg0() { return me; } typedef Grid<T> type0;inline const Grid<Real>& getArg1() { return other; } typedef Grid<Real> type1;inline Real& getArg2() { return cutoff; } typedef Real type2; void runMessage() { debMsg("Executing kernel knSafeDivReal ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {   for (IndexInt idx=__r.begin(); idx!=(IndexInt)__r.end(); idx++) op(idx, me,other,cutoff);   } void run() {   tbb::parallel_for (tbb::blocked_range<IndexInt>(0, size), *this);   }  Grid<T>& me; const Grid<Real>& other; Real cutoff;   };

// Set velocities on the grid from the particle system


 struct knStompVec3PerComponent : public KernelBase { knStompVec3PerComponent(Grid<Vec3>& grid, Real threshold) :  KernelBase(&grid,0) ,grid(grid),threshold(threshold)   { runMessage(); run(); }   inline void op(IndexInt idx, Grid<Vec3>& grid, Real threshold ) const {
	if(grid[idx][0] < threshold) grid[idx][0] = 0.;
	if(grid[idx][1] < threshold) grid[idx][1] = 0.;
	if(grid[idx][2] < threshold) grid[idx][2] = 0.;
}    inline Grid<Vec3>& getArg0() { return grid; } typedef Grid<Vec3> type0;inline Real& getArg1() { return threshold; } typedef Real type1; void runMessage() { debMsg("Executing kernel knStompVec3PerComponent ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {   for (IndexInt idx=__r.begin(); idx!=(IndexInt)__r.end(); idx++) op(idx, grid,threshold);   } void run() {   tbb::parallel_for (tbb::blocked_range<IndexInt>(0, size), *this);   }  Grid<Vec3>& grid; Real threshold;   };




 struct knMapLinearVec3ToMACGrid : public KernelBase { knMapLinearVec3ToMACGrid( BasicParticleSystem& p, FlagGrid& flags, MACGrid& vel, Grid<Vec3>& tmp, ParticleDataImpl<Vec3>& pvel ) :  KernelBase(p.size()) ,p(p),flags(flags),vel(vel),tmp(tmp),pvel(pvel)   { runMessage(); run(); }   inline void op(IndexInt idx,  BasicParticleSystem& p, FlagGrid& flags, MACGrid& vel, Grid<Vec3>& tmp, ParticleDataImpl<Vec3>& pvel  )  {
	unusedParameter(flags);
	if (!p.isActive(idx)) return;
	vel.setInterpolated( p[idx].pos, pvel[idx], &tmp[0] );
}    inline BasicParticleSystem& getArg0() { return p; } typedef BasicParticleSystem type0;inline FlagGrid& getArg1() { return flags; } typedef FlagGrid type1;inline MACGrid& getArg2() { return vel; } typedef MACGrid type2;inline Grid<Vec3>& getArg3() { return tmp; } typedef Grid<Vec3> type3;inline ParticleDataImpl<Vec3>& getArg4() { return pvel; } typedef ParticleDataImpl<Vec3> type4; void runMessage() { debMsg("Executing kernel knMapLinearVec3ToMACGrid ", 3); debMsg("Kernel range" <<  " size "<<  size  << " "   , 4); }; void run() {   const IndexInt _sz = size; for (IndexInt i = 0; i < _sz; i++) op(i, p,flags,vel,tmp,pvel);   } BasicParticleSystem& p; FlagGrid& flags; MACGrid& vel; Grid<Vec3>& tmp; ParticleDataImpl<Vec3>& pvel;   };

// optionally , this function can use an existing vec3 grid to store the weights
// this is useful in combination with the simple extrapolation function


void mapPartsToMAC( FlagGrid& flags, MACGrid& vel , MACGrid& velOld , BasicParticleSystem& parts , ParticleDataImpl<Vec3>& partVel , Grid<Vec3>* weight=NULL ) {
	// interpol -> grid. tmpgrid for particle contribution weights
	bool freeTmp = false;
	if(!weight) {
		weight = new Grid<Vec3>(flags.getParent());
		freeTmp = true;
	} else {
		weight->clear(); // make sure we start with a zero grid!
	}
	vel.clear();
	knMapLinearVec3ToMACGrid( parts, flags, vel, *weight, partVel );

	// stomp small values in weight to zero to prevent roundoff errors
	knStompVec3PerComponent( *weight, VECTOR_EPSILON );
	vel.safeDivide(*weight);
	
	// store original state
	velOld.copyFrom( vel );
	if(freeTmp) delete weight;
} static PyObject* _W_10 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "mapPartsToMAC" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",0,&_lock); MACGrid& vel = *_args.getPtr<MACGrid >("vel",1,&_lock); MACGrid& velOld = *_args.getPtr<MACGrid >("velOld",2,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",3,&_lock); ParticleDataImpl<Vec3>& partVel = *_args.getPtr<ParticleDataImpl<Vec3> >("partVel",4,&_lock); Grid<Vec3>* weight = _args.getPtrOpt<Grid<Vec3> >("weight",5,NULL ,&_lock);   _retval = getPyNone(); mapPartsToMAC(flags,vel,velOld,parts,partVel,weight);  _args.check(); } pbFinalizePlugin(parent,"mapPartsToMAC", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("mapPartsToMAC",e.what()); return 0; } } static const Pb::Register _RP_mapPartsToMAC ("","mapPartsToMAC",_W_10);  extern "C" { void PbRegister_mapPartsToMAC() { KEEP_UNUSED(_RP_mapPartsToMAC); } } 




template <class T>  struct knMapLinear : public KernelBase { knMapLinear( BasicParticleSystem& p, FlagGrid& flags, Grid<T>& target, Grid<Real>& gtmp, ParticleDataImpl<T>& psource ) :  KernelBase(p.size()) ,p(p),flags(flags),target(target),gtmp(gtmp),psource(psource)   { runMessage(); run(); }   inline void op(IndexInt idx,  BasicParticleSystem& p, FlagGrid& flags, Grid<T>& target, Grid<Real>& gtmp, ParticleDataImpl<T>& psource  )  {
	unusedParameter(flags);
	if (!p.isActive(idx)) return;
	target.setInterpolated( p[idx].pos, psource[idx], gtmp );
}    inline BasicParticleSystem& getArg0() { return p; } typedef BasicParticleSystem type0;inline FlagGrid& getArg1() { return flags; } typedef FlagGrid type1;inline Grid<T>& getArg2() { return target; } typedef Grid<T> type2;inline Grid<Real>& getArg3() { return gtmp; } typedef Grid<Real> type3;inline ParticleDataImpl<T>& getArg4() { return psource; } typedef ParticleDataImpl<T> type4; void runMessage() { debMsg("Executing kernel knMapLinear ", 3); debMsg("Kernel range" <<  " size "<<  size  << " "   , 4); }; void run() {   const IndexInt _sz = size; for (IndexInt i = 0; i < _sz; i++) op(i, p,flags,target,gtmp,psource);   } BasicParticleSystem& p; FlagGrid& flags; Grid<T>& target; Grid<Real>& gtmp; ParticleDataImpl<T>& psource;   }; 
template<class T>
void mapLinearRealHelper( FlagGrid& flags, Grid<T>& target , 
		BasicParticleSystem& parts , ParticleDataImpl<T>& source ) 
{
	Grid<Real> tmp(flags.getParent());
	target.clear();
	knMapLinear<T>( parts, flags, target, tmp, source ); 
	knSafeDivReal<T>( target, tmp );
}

void mapPartsToGrid( FlagGrid& flags, Grid<Real>& target , BasicParticleSystem& parts , ParticleDataImpl<Real>& source ) {
	mapLinearRealHelper<Real>(flags,target,parts,source);
} static PyObject* _W_11 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "mapPartsToGrid" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",0,&_lock); Grid<Real>& target = *_args.getPtr<Grid<Real> >("target",1,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",2,&_lock); ParticleDataImpl<Real>& source = *_args.getPtr<ParticleDataImpl<Real> >("source",3,&_lock);   _retval = getPyNone(); mapPartsToGrid(flags,target,parts,source);  _args.check(); } pbFinalizePlugin(parent,"mapPartsToGrid", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("mapPartsToGrid",e.what()); return 0; } } static const Pb::Register _RP_mapPartsToGrid ("","mapPartsToGrid",_W_11);  extern "C" { void PbRegister_mapPartsToGrid() { KEEP_UNUSED(_RP_mapPartsToGrid); } } 
void mapPartsToGridVec3( FlagGrid& flags, Grid<Vec3>& target , BasicParticleSystem& parts , ParticleDataImpl<Vec3>& source ) {
	mapLinearRealHelper<Vec3>(flags,target,parts,source);
} static PyObject* _W_12 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "mapPartsToGridVec3" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",0,&_lock); Grid<Vec3>& target = *_args.getPtr<Grid<Vec3> >("target",1,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",2,&_lock); ParticleDataImpl<Vec3>& source = *_args.getPtr<ParticleDataImpl<Vec3> >("source",3,&_lock);   _retval = getPyNone(); mapPartsToGridVec3(flags,target,parts,source);  _args.check(); } pbFinalizePlugin(parent,"mapPartsToGridVec3", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("mapPartsToGridVec3",e.what()); return 0; } } static const Pb::Register _RP_mapPartsToGridVec3 ("","mapPartsToGridVec3",_W_12);  extern "C" { void PbRegister_mapPartsToGridVec3() { KEEP_UNUSED(_RP_mapPartsToGridVec3); } } 
// integers need "max" mode, not yet implemented
//PYTHON() void mapPartsToGridInt ( FlagGrid& flags, Grid<int >& target , BasicParticleSystem& parts , ParticleDataImpl<int >& source ) {
//	mapLinearRealHelper<int >(flags,target,parts,source);
//}



template <class T>  struct knMapFromGrid : public KernelBase { knMapFromGrid( BasicParticleSystem& p, Grid<T>& gsrc, ParticleDataImpl<T>& target ) :  KernelBase(p.size()) ,p(p),gsrc(gsrc),target(target)   { runMessage(); run(); }   inline void op(IndexInt idx,  BasicParticleSystem& p, Grid<T>& gsrc, ParticleDataImpl<T>& target  ) const {
	if (!p.isActive(idx)) return;
	target[idx] = gsrc.getInterpolated( p[idx].pos );
}    inline BasicParticleSystem& getArg0() { return p; } typedef BasicParticleSystem type0;inline Grid<T>& getArg1() { return gsrc; } typedef Grid<T> type1;inline ParticleDataImpl<T>& getArg2() { return target; } typedef ParticleDataImpl<T> type2; void runMessage() { debMsg("Executing kernel knMapFromGrid ", 3); debMsg("Kernel range" <<  " size "<<  size  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {   for (IndexInt idx=__r.begin(); idx!=(IndexInt)__r.end(); idx++) op(idx, p,gsrc,target);   } void run() {   tbb::parallel_for (tbb::blocked_range<IndexInt>(0, size), *this);   }  BasicParticleSystem& p; Grid<T>& gsrc; ParticleDataImpl<T>& target;   }; 
void mapGridToParts( Grid<Real>& source , BasicParticleSystem& parts , ParticleDataImpl<Real>& target ) {
	knMapFromGrid<Real>(parts, source, target);
} static PyObject* _W_13 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "mapGridToParts" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; Grid<Real>& source = *_args.getPtr<Grid<Real> >("source",0,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",1,&_lock); ParticleDataImpl<Real>& target = *_args.getPtr<ParticleDataImpl<Real> >("target",2,&_lock);   _retval = getPyNone(); mapGridToParts(source,parts,target);  _args.check(); } pbFinalizePlugin(parent,"mapGridToParts", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("mapGridToParts",e.what()); return 0; } } static const Pb::Register _RP_mapGridToParts ("","mapGridToParts",_W_13);  extern "C" { void PbRegister_mapGridToParts() { KEEP_UNUSED(_RP_mapGridToParts); } } 
void mapGridToPartsVec3( Grid<Vec3>& source , BasicParticleSystem& parts , ParticleDataImpl<Vec3>& target ) {
	knMapFromGrid<Vec3>(parts, source, target);
} static PyObject* _W_14 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "mapGridToPartsVec3" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; Grid<Vec3>& source = *_args.getPtr<Grid<Vec3> >("source",0,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",1,&_lock); ParticleDataImpl<Vec3>& target = *_args.getPtr<ParticleDataImpl<Vec3> >("target",2,&_lock);   _retval = getPyNone(); mapGridToPartsVec3(source,parts,target);  _args.check(); } pbFinalizePlugin(parent,"mapGridToPartsVec3", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("mapGridToPartsVec3",e.what()); return 0; } } static const Pb::Register _RP_mapGridToPartsVec3 ("","mapGridToPartsVec3",_W_14);  extern "C" { void PbRegister_mapGridToPartsVec3() { KEEP_UNUSED(_RP_mapGridToPartsVec3); } } 


// Get velocities from grid



 struct knMapLinearMACGridToVec3_PIC : public KernelBase { knMapLinearMACGridToVec3_PIC( BasicParticleSystem& p, FlagGrid& flags, MACGrid& vel, ParticleDataImpl<Vec3>& pvel ) :  KernelBase(p.size()) ,p(p),flags(flags),vel(vel),pvel(pvel)   { runMessage(); run(); }   inline void op(IndexInt idx,  BasicParticleSystem& p, FlagGrid& flags, MACGrid& vel, ParticleDataImpl<Vec3>& pvel  ) const {
	if (!p.isActive(idx)) return;
	// pure PIC
	pvel[idx] = vel.getInterpolated( p[idx].pos );
}    inline BasicParticleSystem& getArg0() { return p; } typedef BasicParticleSystem type0;inline FlagGrid& getArg1() { return flags; } typedef FlagGrid type1;inline MACGrid& getArg2() { return vel; } typedef MACGrid type2;inline ParticleDataImpl<Vec3>& getArg3() { return pvel; } typedef ParticleDataImpl<Vec3> type3; void runMessage() { debMsg("Executing kernel knMapLinearMACGridToVec3_PIC ", 3); debMsg("Kernel range" <<  " size "<<  size  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {   for (IndexInt idx=__r.begin(); idx!=(IndexInt)__r.end(); idx++) op(idx, p,flags,vel,pvel);   } void run() {   tbb::parallel_for (tbb::blocked_range<IndexInt>(0, size), *this);   }  BasicParticleSystem& p; FlagGrid& flags; MACGrid& vel; ParticleDataImpl<Vec3>& pvel;   };

void mapMACToParts(FlagGrid& flags, MACGrid& vel , BasicParticleSystem& parts , ParticleDataImpl<Vec3>& partVel ) {
	knMapLinearMACGridToVec3_PIC( parts, flags, vel, partVel );
} static PyObject* _W_15 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "mapMACToParts" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",0,&_lock); MACGrid& vel = *_args.getPtr<MACGrid >("vel",1,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",2,&_lock); ParticleDataImpl<Vec3>& partVel = *_args.getPtr<ParticleDataImpl<Vec3> >("partVel",3,&_lock);   _retval = getPyNone(); mapMACToParts(flags,vel,parts,partVel);  _args.check(); } pbFinalizePlugin(parent,"mapMACToParts", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("mapMACToParts",e.what()); return 0; } } static const Pb::Register _RP_mapMACToParts ("","mapMACToParts",_W_15);  extern "C" { void PbRegister_mapMACToParts() { KEEP_UNUSED(_RP_mapMACToParts); } } 

// with flip delta interpolation 


 struct knMapLinearMACGridToVec3_FLIP : public KernelBase { knMapLinearMACGridToVec3_FLIP( BasicParticleSystem& p, FlagGrid& flags, MACGrid& vel, MACGrid& oldVel, ParticleDataImpl<Vec3>& pvel , Real flipRatio) :  KernelBase(p.size()) ,p(p),flags(flags),vel(vel),oldVel(oldVel),pvel(pvel),flipRatio(flipRatio)   { runMessage(); run(); }   inline void op(IndexInt idx,  BasicParticleSystem& p, FlagGrid& flags, MACGrid& vel, MACGrid& oldVel, ParticleDataImpl<Vec3>& pvel , Real flipRatio ) const {
	if (!p.isActive(idx)) return; 
	Vec3 v     =        vel.getInterpolated(p[idx].pos);
	Vec3 delta = v - oldVel.getInterpolated(p[idx].pos); 
	pvel[idx] = flipRatio * (pvel[idx] + delta) + (1.0 - flipRatio) * v;    
}    inline BasicParticleSystem& getArg0() { return p; } typedef BasicParticleSystem type0;inline FlagGrid& getArg1() { return flags; } typedef FlagGrid type1;inline MACGrid& getArg2() { return vel; } typedef MACGrid type2;inline MACGrid& getArg3() { return oldVel; } typedef MACGrid type3;inline ParticleDataImpl<Vec3>& getArg4() { return pvel; } typedef ParticleDataImpl<Vec3> type4;inline Real& getArg5() { return flipRatio; } typedef Real type5; void runMessage() { debMsg("Executing kernel knMapLinearMACGridToVec3_FLIP ", 3); debMsg("Kernel range" <<  " size "<<  size  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {   for (IndexInt idx=__r.begin(); idx!=(IndexInt)__r.end(); idx++) op(idx, p,flags,vel,oldVel,pvel,flipRatio);   } void run() {   tbb::parallel_for (tbb::blocked_range<IndexInt>(0, size), *this);   }  BasicParticleSystem& p; FlagGrid& flags; MACGrid& vel; MACGrid& oldVel; ParticleDataImpl<Vec3>& pvel; Real flipRatio;   };


void flipVelocityUpdate(FlagGrid& flags, MACGrid& vel , MACGrid& velOld , BasicParticleSystem& parts , ParticleDataImpl<Vec3>& partVel , Real flipRatio ) {
	knMapLinearMACGridToVec3_FLIP( parts, flags, vel, velOld, partVel, flipRatio );
} static PyObject* _W_16 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "flipVelocityUpdate" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",0,&_lock); MACGrid& vel = *_args.getPtr<MACGrid >("vel",1,&_lock); MACGrid& velOld = *_args.getPtr<MACGrid >("velOld",2,&_lock); BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",3,&_lock); ParticleDataImpl<Vec3>& partVel = *_args.getPtr<ParticleDataImpl<Vec3> >("partVel",4,&_lock); Real flipRatio = _args.get<Real >("flipRatio",5,&_lock);   _retval = getPyNone(); flipVelocityUpdate(flags,vel,velOld,parts,partVel,flipRatio);  _args.check(); } pbFinalizePlugin(parent,"flipVelocityUpdate", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("flipVelocityUpdate",e.what()); return 0; } } static const Pb::Register _RP_flipVelocityUpdate ("","flipVelocityUpdate",_W_16);  extern "C" { void PbRegister_flipVelocityUpdate() { KEEP_UNUSED(_RP_flipVelocityUpdate); } } 


//******************************************************************************
// narrow band 


 struct knCombineVels : public KernelBase { knCombineVels(MACGrid& vel, Grid<Vec3>& w, MACGrid& combineVel, LevelsetGrid* phi, Real narrowBand, Real thresh ) :  KernelBase(&vel,0) ,vel(vel),w(w),combineVel(combineVel),phi(phi),narrowBand(narrowBand),thresh(thresh)   { runMessage(); run(); }  inline void op(int i, int j, int k, MACGrid& vel, Grid<Vec3>& w, MACGrid& combineVel, LevelsetGrid* phi, Real narrowBand, Real thresh  ) const {
	int idx = vel.index(i,j,k);

	for(int c=0; c<3; ++c)
	{
			// Correct narrow-band FLIP
			Vec3 pos(i,j,k);
			pos[(c+1)%3] += Real(0.5);
			pos[(c+2)%3] += Real(0.5);
			Real p = phi->getInterpolated(pos);

			if (p < -narrowBand) { vel[idx][c] = 0; continue; }

			if (w[idx][c] > thresh) {
				combineVel[idx][c] = vel[idx][c];
				vel[idx][c] = -1;
			}
			else
			{
				vel[idx][c] = 0;
			}
	}
}   inline MACGrid& getArg0() { return vel; } typedef MACGrid type0;inline Grid<Vec3>& getArg1() { return w; } typedef Grid<Vec3> type1;inline MACGrid& getArg2() { return combineVel; } typedef MACGrid type2;inline LevelsetGrid* getArg3() { return phi; } typedef LevelsetGrid type3;inline Real& getArg4() { return narrowBand; } typedef Real type4;inline Real& getArg5() { return thresh; } typedef Real type5; void runMessage() { debMsg("Executing kernel knCombineVels ", 3); debMsg("Kernel range" <<  " x "<<  maxX  << " y "<< maxY  << " z "<< minZ<<" - "<< maxZ  << " "   , 4); }; void operator() (const tbb::blocked_range<IndexInt>& __r) const {  const int _maxX = maxX; const int _maxY = maxY; if (maxZ>1) { for (int k=__r.begin(); k!=(int)__r.end(); k++) for (int j=0; j<_maxY; j++) for (int i=0; i<_maxX; i++) op(i,j,k,vel,w,combineVel,phi,narrowBand,thresh); } else { const int k=0; for (int j=__r.begin(); j!=(int)__r.end(); j++) for (int i=0; i<_maxX; i++) op(i,j,k,vel,w,combineVel,phi,narrowBand,thresh); }  } void run() {  if (maxZ>1) tbb::parallel_for (tbb::blocked_range<IndexInt>(minZ, maxZ), *this); else tbb::parallel_for (tbb::blocked_range<IndexInt>(0, maxY), *this);  }  MACGrid& vel; Grid<Vec3>& w; MACGrid& combineVel; LevelsetGrid* phi; Real narrowBand; Real thresh;   };

//! narrow band velocity combination

void combineGridVel( MACGrid& vel, Grid<Vec3>& weight, MACGrid& combineVel, LevelsetGrid* phi=NULL, Real narrowBand=0.0, Real thresh=0.0) {
	knCombineVels(vel, weight, combineVel, phi, narrowBand, thresh);
} static PyObject* _W_17 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "combineGridVel" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; MACGrid& vel = *_args.getPtr<MACGrid >("vel",0,&_lock); Grid<Vec3>& weight = *_args.getPtr<Grid<Vec3> >("weight",1,&_lock); MACGrid& combineVel = *_args.getPtr<MACGrid >("combineVel",2,&_lock); LevelsetGrid* phi = _args.getPtrOpt<LevelsetGrid >("phi",3,NULL,&_lock); Real narrowBand = _args.getOpt<Real >("narrowBand",4,0.0,&_lock); Real thresh = _args.getOpt<Real >("thresh",5,0.0,&_lock);   _retval = getPyNone(); combineGridVel(vel,weight,combineVel,phi,narrowBand,thresh);  _args.check(); } pbFinalizePlugin(parent,"combineGridVel", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("combineGridVel",e.what()); return 0; } } static const Pb::Register _RP_combineGridVel ("","combineGridVel",_W_17);  extern "C" { void PbRegister_combineGridVel() { KEEP_UNUSED(_RP_combineGridVel); } } 





void sampleSndParts(BasicParticleSystem& parts, FlagGrid& flags, MACGrid& vel, LevelsetGrid& phi, ParticleDataImpl<Vec3>& partVel, ParticleDataImpl<int>& partType, Real dropVelThresh, Real bubbleRise, Real floatAmount, Real tracerAmount, int minParticles, int maxParticles, Vec3 gravity, bool drops=true, bool floats=false, bool tracers=false, bool bubbles=true) {
	Real dt = flags.getParent()->getDt();
	Vec3 grav = gravity * flags.getParent()->getDt() / flags.getDx();
	Grid<int> tmp( vel.getParent() );
	RandomStream mRand(9832);

	if (!drops && !floats && !tracers)
		 debMsg("No particle type enabled. Not generating any particles.", 1);

	const Real DROP_THRESH   = -0.866; // -sqrt(3/4)
	const Real BUBBLE_THRESH = -3.5;
	const Real FLOAT_THRESH  = 0.866;  // sqrt(3/4)

	// Delete invalid particles. Then set new position to those that survived
	for (IndexInt idx=0; idx<(int)parts.size(); idx++) {
		if (parts.isActive(idx)) {
			Vec3 p1 = parts.getPos(idx);
			Vec3 p2 = parts.getPos(idx) + partVel[idx] * dt; // next particle position (euler step)
			Vec3i p1i = toVec3i(p1);
			Vec3i p2i = toVec3i(p2);

			// Try to save float / tracer particle by pushing it into the valid region
			Real phiv = phi.getInterpolated( parts.getPos(idx) );
			if (( partType[idx] & ParticleBase::PFLOATER && (phiv > FLOAT_THRESH || phiv < -FLOAT_THRESH)) ||
				( partType[idx] & ParticleBase::PTRACER && phiv > 0. ))
			{
				Vec3 grad = getGradient( phi, p1i.x,p1i.y,p1i.z );
				if ( normalize(grad) > VECTOR_EPSILON )
				{
					int direction = (phiv > 0.) ? -1 : 1;
					parts.setPos(idx, parts.getPos(idx) + direction * phiv * grad );
				}
			}

			// Kill particles depending on type. Especially those that were not converted (see above) to other particle type
			if ( partType[idx] & ParticleBase::PDROPLET && phiv < BUBBLE_THRESH ) { parts.kill(idx); continue; }
			if ( partType[idx] & ParticleBase::PFLOATER && (phiv > 0. || phiv < -FLOAT_THRESH)) { parts.kill(idx); continue; }
			if ( partType[idx] & ParticleBase::PBUBBLE && phiv > 0. ) { parts.kill(idx); continue; }
			if ( partType[idx] & ParticleBase::PTRACER && phiv > 0. ) { parts.kill(idx); continue; }

			// Kill out of domain particles
			if (!tmp.isInBounds(p1i)) {
				parts.kill(idx);
				continue;
			}

			// Kill excess particles in cell
			int num = tmp(p1i);
			if (num > maxParticles) {
				parts.kill(idx);
			} else {
				tmp(p1i) = num+1;
			}

			// Kill particle if next position is invalid, ie outside or in obstacle
			if (!tmp.isInBounds(p2i) || flags.isObstacle(p2i)) {
				parts.kill(idx);
			}

			// This particle is valid. Set its next position
			parts.setPos(idx, p2);
		}
	}

	if (drops) {
		// Generate new drop particles
		FOR_IJK_BND(phi, 0) {
			if ( flags.isObstacle(i,j,k) ) continue;

			// Only generate particles at surface and slightly inside fluid
			if ( phi(i,j,k) > DROP_THRESH && phi(i,j,k) < 0. ) continue;

			// Surrounding fluid vel fast enough to generate drop particle?
			if (fabs(vel(i,j,k).x) < dropVelThresh && fabs(vel(i,j,k).y) < dropVelThresh && fabs(vel(i,j,k).z) < dropVelThresh) continue;

			// Already sufficient particles in cell?
			int cnt = tmp(i,j,k);
			if (cnt >= minParticles) continue;

			// Only generate drops in convex regions
			Vec3 grad = getGradient(phi, i,j,k);
			Vec3 velC = vel.getCentered(i,j,k);
			if ( dot( getNormalized(grad), getNormalized(velC) ) < 0.75) continue;

			if (flags.isFluid(i,j,k) || flags.isEmpty(i,j,k)) {
				// Get phi for next particle position
				Vec3 pos = Vec3(i,j,k);
				const Vec3 pos2 = pos + vel(i,j,k) * dt;
				Real phiv2 = phi.getInterpolated(pos2);

				// Ensure that new particle is only added if its next position is valid: Splash only occurs outside fluid
				if (phiv2 > 0.) {
					parts.addBuffered(pos + mRand.getVec3());
				}
			}
		}
		parts.doCompress();
		parts.insertBufferedParticles();

		// Update forces: gravity and particle velocity
		for (IndexInt idx=0; idx<(int)parts.size(); idx++) {
			if (parts.isActive(idx)) {

				// Update particle type and set initial velocity for new particles
				if (parts.getStatus(idx) & ParticleBase::PNEW) {
					partVel[idx] = vel.getInterpolated( parts[idx].pos );
					partType[idx] = ParticleBase::PDROPLET;
				}

				// Drop particles are outside fluid (or slightly inside after hitting surface) and susceptible to full gravity
				if (partType[idx] & ParticleBase::PDROPLET) {
					partVel[idx] += grav;
				}
			}
		}
	}

	if (bubbles) {
		// Generate bubble particles (converted from drops). Then update forces: gravity and particle velocity
		for (IndexInt idx=0; idx<(int)parts.size(); idx++) {
			if (parts.isActive(idx)) {
				Real phiv = phi.getInterpolated( parts.getPos(idx) );

				// Throttle drop particle, is inside fluid now. Is candidate for bubble
				if (partType[idx] & ParticleBase::PDROPLET && phiv < -FLOAT_THRESH) {
					partVel[idx] = partVel[idx] * 0.5;
				}

				// Update particle type (convert to bubble) and set initial velocity (now use fluid vel)
				if (partType[idx] & ParticleBase::PDROPLET && phiv < BUBBLE_THRESH) {
					partVel[idx] = vel.getInterpolated( parts[idx].pos );
					partType[idx] = ParticleBase::PBUBBLE;
				}

				// Let bubbles rise up
				if (partType[idx] & ParticleBase::PBUBBLE) {
					partVel[idx] += bubbleRise * grav * (-1);
				}
			}
		}
	}

	if (floats) {
		// Generate new float particles
		FOR_IJK_BND(phi, 0) {
			if ( flags.isObstacle(i,j,k) ) continue;

			// Only generate particles at surface and slightly inside fluid
			if ( phi(i,j,k) > FLOAT_THRESH || phi(i,j,k) < -FLOAT_THRESH ) continue;

			// Only seed if random num exceeds given amount probability
			if (mRand.getFloat(0., 1.) > floatAmount) continue;

			if ( flags.isFluid(i,j,k) || flags.isEmpty(i,j,k) ) {
				// Get phi for next particle position
				Vec3 pos = Vec3(i,j,k);
				const Vec3 pos2 = pos + vel(i,j,k) * dt;
				Real phiv = phi.getInterpolated(pos2);

				// Ensure that new particle is only added if its next position is valid:
				if (phiv <= FLOAT_THRESH && phiv >=  -FLOAT_THRESH) {
					parts.addBuffered(pos + mRand.getVec3());
				}
			}
		}
		parts.doCompress();
		parts.insertBufferedParticles();

		// Update forces: gravity and particle velocity
		for (IndexInt idx=0; idx<(int)parts.size(); idx++) {
			if (parts.isActive(idx)) {
				Real phiv = phi.getInterpolated( parts.getPos(idx) );

				// Update particle type
				if (parts.getStatus(idx) & ParticleBase::PNEW) {
					partType[idx] = ParticleBase::PFLOATER;
				}

				// TODO (sebbas): Currently unreliable? Drop particles might get converted into floats to early?
				// Update particle type (convert to float)
				if (partType[idx] & ParticleBase::PBUBBLE && phiv > -FLOAT_THRESH) {
					partType[idx] = ParticleBase::PFLOATER;
				}

				// Float particles move with fluid flow
				if (partType[idx] & ParticleBase::PFLOATER) {
					partVel[idx] = vel.getInterpolated( parts[idx].pos );
				}
			}
		}
	}

	if (tracers) {
		// Generate new tracer particles
		FOR_IJK_BND(phi, 0) {
			if ( flags.isObstacle(i,j,k) ) continue;

			// Only generate particles inside fluid
			if ( phi(i,j,k) > 0. ) continue;

			// Only seed if random num exceeds given amount probability
			if (mRand.getFloat(0., 1.) > tracerAmount) continue;

			if ( flags.isFluid(i,j,k) || flags.isEmpty(i,j,k) ) {
				// Get phi for next particle position
				Vec3 pos = Vec3(i,j,k);
				const Vec3 pos2 = pos + vel(i,j,k) * dt;
				Real phiv = phi.getInterpolated(pos2);

				// Ensure that new particle is only added if its next position is valid
				if (phiv < 0.) {
					parts.addBuffered(pos + mRand.getVec3());
				}
			}
		}
		parts.doCompress();
		parts.insertBufferedParticles();

		// Update forces: gravity and particle velocity
		for (IndexInt idx=0; idx<(int)parts.size(); idx++) {
			if (parts.isActive(idx)) {

				// Update particle type
				if (parts.getStatus(idx) & ParticleBase::PNEW) {
					partType[idx] = ParticleBase::PTRACER;
				}

				if (partType[idx] & ParticleBase::PTRACER) {
					partVel[idx] = vel.getInterpolated( parts[idx].pos );
				}
			}
		}
	}
} static PyObject* _W_18 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver *parent = _args.obtainParent(); bool noTiming = _args.getOpt<bool>("notiming", -1, 0); pbPreparePlugin(parent, "sampleSndParts" , !noTiming ); PyObject *_retval = 0; { ArgLocker _lock; BasicParticleSystem& parts = *_args.getPtr<BasicParticleSystem >("parts",0,&_lock); FlagGrid& flags = *_args.getPtr<FlagGrid >("flags",1,&_lock); MACGrid& vel = *_args.getPtr<MACGrid >("vel",2,&_lock); LevelsetGrid& phi = *_args.getPtr<LevelsetGrid >("phi",3,&_lock); ParticleDataImpl<Vec3>& partVel = *_args.getPtr<ParticleDataImpl<Vec3> >("partVel",4,&_lock); ParticleDataImpl<int>& partType = *_args.getPtr<ParticleDataImpl<int> >("partType",5,&_lock); Real dropVelThresh = _args.get<Real >("dropVelThresh",6,&_lock); Real bubbleRise = _args.get<Real >("bubbleRise",7,&_lock); Real floatAmount = _args.get<Real >("floatAmount",8,&_lock); Real tracerAmount = _args.get<Real >("tracerAmount",9,&_lock); int minParticles = _args.get<int >("minParticles",10,&_lock); int maxParticles = _args.get<int >("maxParticles",11,&_lock); Vec3 gravity = _args.get<Vec3 >("gravity",12,&_lock); bool drops = _args.getOpt<bool >("drops",13,true,&_lock); bool floats = _args.getOpt<bool >("floats",14,false,&_lock); bool tracers = _args.getOpt<bool >("tracers",15,false,&_lock); bool bubbles = _args.getOpt<bool >("bubbles",16,true,&_lock);   _retval = getPyNone(); sampleSndParts(parts,flags,vel,phi,partVel,partType,dropVelThresh,bubbleRise,floatAmount,tracerAmount,minParticles,maxParticles,gravity,drops,floats,tracers,bubbles);  _args.check(); } pbFinalizePlugin(parent,"sampleSndParts", !noTiming ); return _retval; } catch(std::exception& e) { pbSetError("sampleSndParts",e.what()); return 0; } } static const Pb::Register _RP_sampleSndParts ("","sampleSndParts",_W_18);  extern "C" { void PbRegister_sampleSndParts() { KEEP_UNUSED(_RP_sampleSndParts); } } 


} // namespace



