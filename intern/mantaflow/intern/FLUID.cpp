/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sebastian Barschkis (sebbas)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file mantaflow/intern/FLUID.cpp
 *  \ingroup mantaflow
 */

#include <sstream>
#include <fstream>
#include <iostream>
#include <zlib.h>

#include "FLUID.h"
#include "manta.h"
#include "Python.h"
#include "shared_script.h"
#include "smoke_script.h"
#include "liquid_script.h"

#include "BLI_path_util.h"
#include "BLI_utildefines.h"
#include "BLI_fileops.h"

#include "DNA_scene_types.h"
#include "DNA_modifier_types.h"
#include "DNA_smoke_types.h"

std::atomic<bool> FLUID::mantaInitialized(false);
std::atomic<int> FLUID::solverID(0);
int FLUID::with_debug(0);

FLUID::FLUID(int *res, SmokeModifierData *smd) : mCurrentID(++solverID)
{
	if (with_debug)
		std::cout << "FLUID: " << mCurrentID << std::endl;

	smd->domain->fluid = this;
	smd->domain->manta_solver_res = 3; // Why do we need to set this explicitly? When not set, fluidsolver throws exception (occurs when loading a new .blend file)
	
	mUsingHeat    = smd->domain->active_fields & SM_ACTIVE_HEAT;
	mUsingFire    = smd->domain->active_fields & SM_ACTIVE_FIRE;
	mUsingColors  = smd->domain->active_fields & SM_ACTIVE_COLORS;
	mUsingHighRes = smd->domain->flags & MOD_SMOKE_HIGHRES;
	mUsingLiquid  = smd->domain->type == MOD_SMOKE_DOMAIN_TYPE_LIQUID;
	mUsingSmoke   = smd->domain->type == MOD_SMOKE_DOMAIN_TYPE_GAS;
	
	// Make sure that string vector does not contain any previous commands
	mCommands.clear();

	// Simulation constants
	mTempAmb            = 0; // TODO: Maybe use this later for buoyancy calculation
	mResX               = res[0];
	mResY               = res[1];
	mResZ               = res[2];
	mMaxRes             = MAX3(mResX, mResY, mResZ);
	mConstantScaling    = 64.0f / mMaxRes;
	mConstantScaling    = (mConstantScaling < 1.0f) ? 1.0f : mConstantScaling;
	mTotalCells         = mResX * mResY * mResZ;

	// Smoke low res grids
	mDensity        = NULL;
	mHeat           = NULL;
	mVelocityX      = NULL;
	mVelocityY      = NULL;
	mVelocityZ      = NULL;
	mObVelocityX    = NULL;
	mObVelocityY    = NULL;
	mObVelocityZ    = NULL;
	mForceX         = NULL;
	mForceY         = NULL;
	mForceZ         = NULL;
	mFlame          = NULL;
	mFuel           = NULL;
	mReact          = NULL;
	mColorR         = NULL;
	mColorG         = NULL;
	mColorB         = NULL;
	mObstacle       = NULL;

	// Smoke high res grids
	mDensityHigh    = NULL;
	mFlameHigh      = NULL;
	mFuelHigh       = NULL;
	mReactHigh      = NULL;
	mColorRHigh     = NULL;
	mColorGHigh     = NULL;
	mColorBHigh     = NULL;
	mTextureU       = NULL;
	mTextureV       = NULL;
	mTextureW       = NULL;
	mTextureU2      = NULL;
	mTextureV2      = NULL;
	mTextureW2      = NULL;

	// Liquid low res grids
	mPhiIn          = NULL;
	mPhiObs         = NULL;
	mPhiOut         = NULL;

	mNumVertices  = 0;
	mNumNormals   = 0;
	mNumTriangles = 0;

	// Particles
	mNumParticles = 0;
	mParticleDimX = 0;
	mParticleDimY = 0;
	mParticleDimZ = 0;

	// Only start Mantaflow once. No need to start whenever new FLUID objected is allocated
	if (!mantaInitialized)
		initializeMantaflow();

	// Initialize Mantaflow variables in Python
	// Liquid
	if (mUsingLiquid) {
		initDomain(smd);
		initLiquid(smd);

		updatePointers();
		
		if (mUsingHighRes) {
			// Make sure that string vector does not contain any previous commands
			mCommands.clear();

			// simulation constants
			int amplify     = smd->domain->amplify + 1;
			mResXHigh       = amplify * mResX;
			mResYHigh       = amplify * mResY;
			mResZHigh       = amplify * mResZ;
			mTotalCellsHigh	= mResXHigh * mResYHigh * mResZHigh;
			
			// Initialize Mantaflow variables in Python
			initDomainHigh(smd);
			initLiquidHigh(smd);

			updatePointersHigh();
		}

		return;
	}
	
	// Smoke
	if (mUsingSmoke) {
		initDomain(smd);
		initSmoke(smd);
		if (mUsingHeat)   initHeat(smd);
		if (mUsingFire)   initFire(smd);
		if (mUsingColors) initColors(smd);

		updatePointers(); // Needs to be after heat, fire, color init

		if (mUsingHighRes) {
			// Make sure that string vector does not contain any previous commands
			mCommands.clear();

			// simulation constants
			int amplify     = smd->domain->amplify + 1;
			mResXHigh       = amplify * mResX;
			mResYHigh       = amplify * mResY;
			mResZHigh       = amplify * mResZ;
			mTotalCellsHigh	= mResXHigh * mResYHigh * mResZHigh;
			
			// Initialize Mantaflow variables in Python
			initDomainHigh(smd);
			initSmokeHigh(smd);
			if (mUsingFire)   initFireHigh(smd);
			if (mUsingColors) initColorsHigh(smd);

			updatePointersHigh(); // Needs to be after fire, color init
		}
	}
}

void FLUID::initDomain(SmokeModifierData *smd)
{
	std::string tmpString = manta_import
		+ manta_debuglevel
		+ fluid_variables_low
		+ fluid_solver_low
		+ fluid_adaptive_time_stepping_low;
	std::string finalString = parseScript(tmpString, smd);
	mCommands.clear();
	mCommands.push_back(finalString);
	
	// Set manta debug level
	std::ostringstream debuglevel;
	debuglevel <<  "set_manta_debuglevel(" << with_debug << ")";
	mCommands.push_back(debuglevel.str());
	runPythonString(mCommands);
}

void FLUID::initDomainHigh(SmokeModifierData *smd)
{
	std::string tmpString = fluid_variables_high
		+ fluid_solver_high
		+ fluid_adaptive_time_stepping_high;
	std::string finalString = parseScript(tmpString, smd);
	mCommands.clear();
	mCommands.push_back(finalString);
	
	runPythonString(mCommands);
}

void FLUID::initSmoke(SmokeModifierData *smd)
{
	std::string tmpString = smoke_alloc_low
		+ smoke_variables_low
		+ smoke_bounds_low
		+ smoke_adaptive_step
		+ smoke_export_low
		+ smoke_pre_step_low
		+ smoke_step_low
		+ smoke_post_step_low;
	std::string finalString = parseScript(tmpString, smd);
	mCommands.clear();
	mCommands.push_back(finalString);
	
	runPythonString(mCommands);
}

void FLUID::initSmokeHigh(SmokeModifierData *smd)
{
	std::string tmpString = smoke_alloc_high
		+ smoke_variables_high
		+ smoke_uv_setup
		+ smoke_bounds_high
		+ smoke_wavelet_turbulence_noise
		+ smoke_export_high
		+ smoke_pre_step_high
		+ smoke_step_high
		+ smoke_post_step_high;
	std::string finalString = parseScript(tmpString, smd);
	mCommands.clear();
	mCommands.push_back(finalString);

	runPythonString(mCommands);
	mUsingHighRes = true;
}

void FLUID::initHeat(SmokeModifierData *smd)
{
	if (!mHeat) {
		std::string tmpString = smoke_alloc_heat_low
			+ smoke_with_heat;
		std::string finalString = parseScript(tmpString, smd);
		mCommands.clear();
		mCommands.push_back(finalString);
		
		runPythonString(mCommands);
		mUsingHeat = true;
	}
}

void FLUID::initFire(SmokeModifierData *smd)
{
	if (!mFuel) {
		std::string tmpString = smoke_alloc_fire_low
			+ smoke_with_fire;
		std::string finalString = parseScript(tmpString, smd);
		mCommands.clear();
		mCommands.push_back(finalString);

		runPythonString(mCommands);
		mUsingFire = true;
	}
}

void FLUID::initFireHigh(SmokeModifierData *smd)
{
	if (!mFuelHigh) {
		std::string tmpString = smoke_alloc_fire_high
			+ smoke_with_fire;
		std::string finalString = parseScript(tmpString, smd);
		mCommands.clear();
		mCommands.push_back(finalString);

		runPythonString(mCommands);
		mUsingFire = true;
	}
}

void FLUID::initColors(SmokeModifierData *smd)
{
	if (!mColorR) {
		std::string tmpString = smoke_alloc_colors_low
			+ smoke_init_colors_low
			+ smoke_with_colors;
		std::string finalString = parseScript(tmpString, smd);
		mCommands.clear();
		mCommands.push_back(finalString);

		runPythonString(mCommands);
		mUsingColors = true;
	}
}

void FLUID::initColorsHigh(SmokeModifierData *smd)
{
	if (!mColorRHigh) {
		std::string tmpString = smoke_alloc_colors_high
			+ smoke_init_colors_high
			+ smoke_with_colors;
		std::string finalString = parseScript(tmpString, smd);
		mCommands.clear();
		mCommands.push_back(finalString);

		runPythonString(mCommands);
		mUsingColors = true;
	}
}

void FLUID::initLiquid(SmokeModifierData *smd)
{
	if (!mPhiIn) {
		std::string tmpString = liquid_alloc_low
			+ liquid_variables_low
			+ liquid_bounds_low
			+ liquid_init_phi
			+ liquid_save_mesh_low
			+ liquid_save_particles_low
			+ liquid_export_low
			+ liquid_import_low
			+ liquid_adaptive_step
			+ liquid_pre_step_low
			+ liquid_step_low
			+ liquid_post_step_low;
		std::string finalString = parseScript(tmpString, smd);
		mCommands.clear();
		mCommands.push_back(finalString);

		runPythonString(mCommands);
		mUsingLiquid = true;
	}
}

void FLUID::initLiquidHigh(SmokeModifierData *smd)
{
	std::string tmpString = liquid_alloc_high
		+ liquid_variables_high
		+ liquid_bounds_high
		+ liquid_save_mesh_high
		+ liquid_export_high
		+ liquid_import_high
		+ liquid_step_high;
	std::string finalString = parseScript(tmpString, smd);
	mCommands.clear();
	mCommands.push_back(finalString);

	runPythonString(mCommands);
	mUsingHighRes = true;
}

void FLUID::step(int framenr)
{
	// manta_write_effectors(this);                         // TODO in Mantaflow

	// Run manta step and handover current frame number
	mCommands.clear();
	std::ostringstream manta_step;
	manta_step <<  "manta_step_" << mCurrentID << "(" << framenr << ")";
	mCommands.push_back(manta_step.str());

	runPythonString(mCommands);
}

FLUID::~FLUID()
{
	if (with_debug)
		std::cout << "FLUID: " << mCurrentID << std::endl;

	// Destruction string for Python
	std::string tmpString = "";

	// Fluid
	tmpString += fluid_delete_variables_low;
	tmpString += fluid_delete_variables_high;

	// Liquid
	tmpString += liquid_delete_variables_low;
	tmpString += liquid_delete_grids_low;

	tmpString += liquid_delete_variables_high;
	tmpString += liquid_delete_grids_high;

	// Smoke
	tmpString += smoke_delete_variables_low;
	tmpString += smoke_delete_grids_low;
	tmpString += smoke_delete_heat_low;
	tmpString += smoke_delete_fire_low;
	tmpString += smoke_delete_colors_low;

	tmpString += smoke_delete_variables_high;
	tmpString += smoke_delete_grids_high;
	tmpString += smoke_delete_fire_high;
	tmpString += smoke_delete_colors_high;

	// Cleanup multigrid
	tmpString += fluid_multigrid_cleanup_low;
	if (mUsingHighRes) tmpString += fluid_multigrid_cleanup_high;

	// Make sure that everything is garbage collected
	tmpString += gc_collect;

	// Solvers always have to be the last objects to be deleted
	tmpString += fluid_delete_solver_low;
	if (mUsingHighRes) tmpString += fluid_delete_solver_high;

	// Just in case: gc again
	tmpString += gc_collect;

	// Safe to pass NULL argument since only looking up IDs
	std::string finalString = parseScript(tmpString, NULL);
	mCommands.clear();
	mCommands.push_back(finalString);
	runPythonString(mCommands);

	// Reset pointers to avoid dangling pointers
	mDensity        = NULL;
	mHeat           = NULL;
	mVelocityX      = NULL;
	mVelocityY      = NULL;
	mVelocityZ      = NULL;
	mObVelocityX    = NULL;
	mObVelocityY    = NULL;
	mObVelocityZ    = NULL;
	mForceX         = NULL;
	mForceY         = NULL;
	mForceZ         = NULL;
	mFlame          = NULL;
	mFuel           = NULL;
	mReact          = NULL;
	mColorR         = NULL;
	mColorG         = NULL;
	mColorB         = NULL;
	mObstacle       = NULL;

	mDensityHigh    = NULL;
	mFlameHigh      = NULL;
	mFuelHigh       = NULL;
	mReactHigh      = NULL;
	mColorRHigh     = NULL;
	mColorGHigh     = NULL;
	mColorBHigh     = NULL;
	mTextureU       = NULL;
	mTextureV       = NULL;
	mTextureW       = NULL;
	mTextureU2      = NULL;
	mTextureV2      = NULL;
	mTextureW2      = NULL;

	// Liquid
	mPhiIn  = NULL;
	mPhiObs = NULL;
	mPhiOut = NULL;

	// Reset flags
	mUsingHeat    = false;
	mUsingFire    = false;
	mUsingColors  = false;
	mUsingHighRes = false;
}

void FLUID::runPythonString(std::vector<std::string> commands)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();
	for (std::vector<std::string>::iterator it = commands.begin(); it != commands.end(); ++it) {
		std::string command = *it;

#ifdef WIN32
		// special treatment for windows when running python code
		size_t cmdLength = command.length();
		char* buffer = new char[cmdLength+1];
		memcpy(buffer, command.data(), cmdLength);

		buffer[cmdLength] = '\0';
		PyRun_SimpleString(buffer);
		delete[] buffer;
#else
		PyRun_SimpleString(command.c_str());
#endif
	}
	PyGILState_Release(gilstate);
}

void FLUID::initializeMantaflow()
{
	if (with_debug)
		std::cout << "Initializing  Mantaflow" << std::endl;

	std::string filename = "manta_scene_" + std::to_string(mCurrentID) + ".py";
	std::vector<std::string> fill = std::vector<std::string>();
	
	// Initialize extension classes and wrappers
	srand(0);
	PyGILState_STATE gilstate = PyGILState_Ensure();
	Pb::setup(filename, fill);  // Namespace from Mantaflow (registry)
	PyGILState_Release(gilstate);
	mantaInitialized = true;
}

void FLUID::terminateMantaflow()
{
	if (with_debug)
		std::cout << "Terminating Mantaflow" << std::endl;

	PyGILState_STATE gilstate = PyGILState_Ensure();
	Pb::finalize();  // Namespace from Mantaflow (registry)
	PyGILState_Release(gilstate);
	mantaInitialized = false;
}

std::string FLUID::getRealValue(const std::string& varName,  SmokeModifierData *smd)
{
	std::ostringstream ss;
	bool is2D = false;
	ModifierData *md;
	int closedDomain;
	
	if (smd) {
		is2D = (smd->domain->manta_solver_res == 2);
		md = ((ModifierData*) smd);
	}

	if (varName == "USING_COLORS")
		ss << (smd->domain->active_fields & SM_ACTIVE_COLORS ? "True" : "False");
	else if (varName == "USING_HEAT")
		ss << (smd->domain->active_fields & SM_ACTIVE_HEAT ? "True" : "False");
	else if (varName == "USING_FIRE")
		ss << (smd->domain->active_fields & SM_ACTIVE_FIRE ? "True" : "False");
	else if (varName == "USING_HIGHRES")
		ss << (smd->domain->flags & MOD_SMOKE_HIGHRES ? "True" : "False");
	else if (varName == "SOLVER_DIM")
		ss << smd->domain->manta_solver_res;
	else if (varName == "DO_OPEN") {
		closedDomain = (MOD_SMOKE_BORDER_BACK | MOD_SMOKE_BORDER_FRONT |
						 MOD_SMOKE_BORDER_LEFT | MOD_SMOKE_BORDER_RIGHT |
						 MOD_SMOKE_BORDER_BOTTOM | MOD_SMOKE_BORDER_TOP);
		ss << (((smd->domain->border_collisions & closedDomain) == closedDomain) ? "False" : "True");
	} else if (varName == "BOUNDCONDITIONS") {
		if (smd->domain->manta_solver_res == 2) {
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_LEFT) == 0) ss << "x";
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_RIGHT) == 0) ss << "X";
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_FRONT) == 0) ss << "y";
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_BACK) == 0) ss << "Y";
		}
		if (smd->domain->manta_solver_res == 3) {
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_LEFT) == 0) ss << "x";
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_RIGHT) == 0) ss << "X";
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_FRONT) == 0) ss << "y";
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_BACK) == 0) ss << "Y";
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_BOTTOM) == 0) ss << "z";
			if ((smd->domain->border_collisions & MOD_SMOKE_BORDER_TOP) == 0) ss << "Z";
		}
	} else if (varName == "RES")
		ss << smd->domain->maxres;
	else if (varName == "RESX")
		ss << mResX;
	else if (varName == "RESY")
		if (is2D) {	ss << mResZ;}
		else { 		ss << mResY;}
	else if (varName == "RESZ") {
		if (is2D){	ss << 1;}
		else { 		ss << mResZ;}
	} else if (varName == "DT_FACTOR")
		ss << smd->domain->time_scale;
	else if (varName == "CFL")
		ss << smd->domain->cfl_condition;
	else if (varName == "FPS")
		ss << md->scene->r.frs_sec / md->scene->r.frs_sec_base;
	else if (varName == "VORTICITY")
		ss << smd->domain->vorticity / mConstantScaling;
	else if (varName == "UPRES")
		ss << smd->domain->amplify + 1;
	else if (varName == "HRESX")
		ss << mResXHigh;
	else if (varName == "HRESY") {
		if (is2D) {	ss << mResZHigh;}
		else { 		ss << mResYHigh;}
	} else if (varName == "HRESZ") {
		if (is2D) {	ss << 1;}
		else { 		ss << mResZHigh;}
	} else if (varName == "WLT_STR")
		ss << smd->domain->strength;
	else if (varName == "NOISE_POSSCALE")
		ss << smd->domain->noise_pos_scale;
	else if (varName == "NOISE_TIMEANIM")
		ss << smd->domain->noise_time_anim;
	else if (varName == "COLOR_R")
		ss << smd->domain->active_color[0];
	else if (varName == "COLOR_G")
		ss << smd->domain->active_color[1];
	else if (varName == "COLOR_B")
		ss << smd->domain->active_color[2];
	else if (varName == "ADVECT_ORDER")
		ss << 2;
	else if (varName == "ALPHA")
		ss << smd->domain->alpha;
	else if (varName == "BETA")
		ss << smd->domain->beta;
	else if (varName == "BURNING_RATE")
		ss << smd->domain->burning_rate;
	else if (varName == "FLAME_SMOKE")
		ss << smd->domain->flame_smoke;
	else if (varName == "IGNITION_TEMP")
		ss << smd->domain->flame_ignition;
	else if (varName == "MAX_TEMP")
		ss << smd->domain->flame_max_temp;
	else if (varName == "FLAME_SMOKE_COLOR_X")
		ss << smd->domain->flame_smoke_color[0];
	else if (varName == "FLAME_SMOKE_COLOR_Y")
		ss << smd->domain->flame_smoke_color[1];
	else if (varName == "FLAME_SMOKE_COLOR_Z")
		ss << smd->domain->flame_smoke_color[2];
	else if (varName == "CURRENT_FRAME")
		ss << md->scene->r.cfra - 1;
	else if (varName == "PARTICLE_RANDOMNESS")
		ss << smd->domain->particle_randomness;
	else if (varName == "PARTICLE_NUMBER")
		ss << smd->domain->particle_number;
	else if (varName == "PARTICLE_RADIUS")
		ss << smd->domain->particle_radius;
	else if (varName == "PARTICLE_BAND_WIDTH")
		ss << smd->domain->particle_band_width;
	else if (varName == "GRAVITY_X")
		ss << smd->domain->gravity[0];
	else if (varName == "GRAVITY_Y")
		ss << smd->domain->gravity[1];
	else if (varName == "GRAVITY_Z")
		ss << smd->domain->gravity[2];
	else if (varName == "MANTA_EXPORT_PATH") {
		char parent_dir[1024];
		BLI_split_dir_part(smd->domain->manta_filepath, parent_dir, sizeof(parent_dir));
		ss << parent_dir;
	} else if (varName == "PRECONDITIONER") {
		if (smd->domain->preconditioner == MOD_SMOKE_PC_NONE) ss << "PcNone";
		else if (smd->domain->preconditioner == MOD_SMOKE_PC_MIC) ss << "PcMIC";
		else if (smd->domain->preconditioner == MOD_SMOKE_PC_MG_DYNAMIC) ss << "PcMGDynamic";
		else if (smd->domain->preconditioner == MOD_SMOKE_PC_MG_STATIC) ss << "PcMGStatic";
	} else if (varName == "ID")
		ss << mCurrentID;
	else if (varName == "USING_ADAPTIVETIME")
		ss << (smd->domain->flags & MOD_SMOKE_ADAPTIVE_TIME ? "True" : "False");
	else
		std::cout << "ERROR: Unknown option: " << varName << std::endl;
	return ss.str();
}

std::string FLUID::parseLine(const std::string& line, SmokeModifierData *smd)
{
	if (line.size() == 0) return "";
	std::string res = "";
	int currPos = 0, start_del = 0, end_del = -1;
	bool readingVar = false;
	const char delimiter = '$';
	while (currPos < line.size()) {
		if (line[currPos] == delimiter && !readingVar) {
			readingVar  = true;
			start_del   = currPos + 1;
			res        += line.substr(end_del + 1, currPos - end_del -1);
		}
		else if (line[currPos] == delimiter && readingVar) {
			readingVar  = false;
			end_del     = currPos;
			res        += getRealValue(line.substr(start_del, currPos - start_del), smd);
		}
		currPos ++;
	}
	res += line.substr(end_del+1, line.size()- end_del);
	return res;
}

std::string FLUID::parseScript(const std::string& setup_string, SmokeModifierData *smd)
{
	std::istringstream f(setup_string);
	std::ostringstream res;
	std::string line = "";
	while(getline(f, line)) {
		res << parseLine(line, smd) << "\n";
	}
	return res.str();
}

void FLUID::exportSmokeScript(SmokeModifierData *smd)
{
	bool highres = smd->domain->flags & MOD_SMOKE_HIGHRES;
	bool heat    = smd->domain->active_fields & SM_ACTIVE_HEAT;
	bool colors  = smd->domain->active_fields & SM_ACTIVE_COLORS;
	bool fire    = smd->domain->active_fields & SM_ACTIVE_FIRE;

	std::string manta_script;

	manta_script += manta_import
		+ fluid_variables_low
		+ fluid_solver_low
		+ fluid_adaptive_time_stepping_low
		+ smoke_alloc_low
		+ smoke_bounds_low
		+ smoke_variables_low;
	
	if (heat)
		manta_script += smoke_alloc_heat_low;
	if (colors)
		manta_script += smoke_alloc_colors_low;
	if (fire)
		manta_script += smoke_alloc_fire_low;

	if (highres) {
		manta_script += fluid_variables_high
			+ fluid_solver_high
			+ fluid_adaptive_time_stepping_high
			+ smoke_variables_high
			+ smoke_alloc_high
			+ smoke_uv_setup
			+ smoke_bounds_high
			+ smoke_wavelet_turbulence_noise;

		if (colors)
			manta_script += smoke_alloc_colors_high;
		if (fire)
			manta_script += smoke_alloc_fire_high;
	}
	
	manta_script += smoke_import_low;
	if (highres)
		manta_script += smoke_import_high;
	
	manta_script += smoke_pre_step_low;
	if (highres)
		manta_script += smoke_pre_step_high;
	
	manta_script += smoke_post_step_low;
	if (highres)
		manta_script += smoke_post_step_high;

	manta_script += smoke_step_low;
	if (highres)
		manta_script += smoke_step_high;
	
	manta_script += smoke_adaptive_step
			+ smoke_inflow_low
			+ smoke_standalone_load
			+ fluid_standalone;
	
	// Fill in missing variables in script
	std::string final_script = FLUID::parseScript(manta_script, smd);
	
	// Write script
	std::ofstream myfile;
	myfile.open(smd->domain->manta_filepath);
	myfile << final_script;
	myfile.close();
}

void FLUID::exportSmokeData(SmokeModifierData *smd)
{
	bool highres = smd->domain->flags & MOD_SMOKE_HIGHRES;

	char parent_dir[1024];
	BLI_split_dir_part(smd->domain->manta_filepath, parent_dir, sizeof(parent_dir));
	
	FLUID::saveSmokeData(parent_dir);
	if (highres)
		FLUID::saveSmokeDataHigh(parent_dir);
}

void FLUID::exportLiquidScript(SmokeModifierData *smd)
{
	bool highres = smd->domain->flags & MOD_SMOKE_HIGHRES;

	std::string manta_script;
	
	manta_script += manta_import
		+ fluid_variables_low
		+ fluid_solver_low
		+ fluid_adaptive_time_stepping_low
		+ liquid_alloc_low
		+ liquid_bounds_low
		+ liquid_init_phi
		+ liquid_variables_low;

	if (highres) {
		manta_script += fluid_variables_high
			+ fluid_solver_high
			+ fluid_adaptive_time_stepping_high
			+ liquid_alloc_high
			+ liquid_bounds_high
			+ liquid_variables_high;
	}

	manta_script += liquid_import_low;
	if (highres)
		manta_script += liquid_import_high;
	
	manta_script += liquid_pre_step_low;
	manta_script += liquid_post_step_low;
	
	manta_script += liquid_step_low;
	if (highres)
		manta_script += liquid_step_high;

	manta_script += liquid_adaptive_step
			+ liquid_standalone_load
			+ fluid_standalone;

	std::string final_script = FLUID::parseScript(manta_script, smd);

	// Write script
	std::ofstream myfile;
	myfile.open(smd->domain->manta_filepath);
	myfile << final_script;
	myfile.close();
}

void FLUID::exportLiquidData(SmokeModifierData *smd)
{
	bool highres = smd->domain->flags & MOD_SMOKE_HIGHRES;

	char parent_dir[1024];
	BLI_split_dir_part(smd->domain->manta_filepath, parent_dir, sizeof(parent_dir));
	
	FLUID::saveLiquidData(parent_dir);
	if (highres)
		FLUID::saveLiquidDataHigh(parent_dir);
}

void* FLUID::getGridPointer(std::string gridName, std::string solverName)
{
	if ((gridName == "") && (solverName == "")) return NULL;

	PyGILState_STATE gilstate = PyGILState_Ensure();

	// Get pyobject that holds pointer address as string
	PyObject* main = PyImport_AddModule("__main__");
	PyObject* gridObject = PyObject_GetAttrString(main, gridName.c_str());
	PyObject* func = PyObject_GetAttrString(gridObject, (char*) "getDataPointer");
	PyObject* returnedValue = PyObject_CallObject(func, NULL);
	PyObject* encoded = PyUnicode_AsUTF8String(returnedValue);

	// Convert string pointer to void pointer
	std::string pointerString = PyBytes_AsString(encoded);
	std::istringstream in(pointerString);
	void *gridPointer = NULL;
	in >> gridPointer;
	
	Py_DECREF(gridObject);
	Py_DECREF(func);
	Py_DECREF(returnedValue);
	Py_DECREF(encoded);

	PyGILState_Release(gilstate);
	return gridPointer;
}

void FLUID::updateMeshData(const char* filename)
{
	gzFile gzf;
	float fbuffer[3];
	int ibuffer[3];

	gzf = (gzFile) BLI_gzopen(filename, "rb1"); // do some compression
	if (!gzf)
		std::cout << "updateMeshData: unable to open file" << std::endl;
	
	// Num vertices
	mNumVertices = 0;
	gzread(gzf, &mNumVertices, sizeof(int));
	
	if (with_debug)
		std::cout << "read mesh , num verts: " << mNumVertices << " , in file: "<< filename << std::endl;

	if (mNumVertices)
	{
		mVerticesX.resize(mNumVertices);
		mVerticesY.resize(mNumVertices);
		mVerticesZ.resize(mNumVertices);
		
		// Vertices
		for (int i = 0; i < mNumVertices; i++) {
			gzread(gzf, fbuffer, sizeof(float) * 3);
			
			mVerticesX[i] = fbuffer[0];
			mVerticesY[i] = fbuffer[1];
			mVerticesZ[i] = fbuffer[2];
		}
	}
	
	// Num normals
	mNumNormals = 0;
	gzread(gzf, &mNumNormals, sizeof(float));
	
	if (mNumNormals)
	{
		mNormalsX.resize(mNumNormals);
		mNormalsY.resize(mNumNormals);
		mNormalsZ.resize(mNumNormals);
		
		// Normals
		for (int i = 0; i < mNumNormals; i++) {
			gzread(gzf, fbuffer, sizeof(float) * 3);
			
			mNormalsX[i] = fbuffer[0];
			mNormalsY[i] = fbuffer[1];
			mNormalsZ[i] = fbuffer[2];
		}
	}
	
	// Num triangles
	mNumTriangles = 0;
	gzread(gzf, &mNumTriangles, sizeof(int));
	
	if (mNumTriangles)
	{
		mTrianglesX.resize(mNumTriangles);
		mTrianglesY.resize(mNumTriangles);
		mTrianglesZ.resize(mNumTriangles);
		
		// Triangles
		for (int i = 0; i < mNumTriangles; i++) {
			gzread(gzf, ibuffer, sizeof(int) * 3);
			
			mTrianglesX[i] = ibuffer[0];
			mTrianglesY[i] = ibuffer[1];
			mTrianglesZ[i] = ibuffer[2];
		}
	}

	gzclose(gzf);
}

void FLUID::updateParticleData(const char* filename)
{
	gzFile gzf;
	float fbuffer[3];
	int ibuffer;

	gzf = (gzFile) BLI_gzopen(filename, "rb1"); // do some compression
	if (!gzf)
		std::cout << "updateParticleData: unable to open file" << std::endl;

	char ID[5] = {0,0,0,0,0};
	gzread(gzf, ID, 4);

	if (!strcmp(ID, "PB01")) {
		std::cout << "particle uni file format v01 not supported anymore" << std::endl;
		return;
	}

	// pdata uni header
	const int STR_LEN_PDATA = 256;
	int elementType, bytesPerElement; // type id and byte size
	char info[STR_LEN_PDATA]; // mantaflow build information
	unsigned long long timestamp; // creation time

	// read particle header
	gzread(gzf, &mNumParticles, sizeof(int));
	gzread(gzf, &mParticleDimX, sizeof(int));
	gzread(gzf, &mParticleDimY, sizeof(int));
	gzread(gzf, &mParticleDimZ, sizeof(int));
	gzread(gzf, &elementType, sizeof(int));
	gzread(gzf, &bytesPerElement, sizeof(int));
	gzread(gzf, &info, sizeof(info));
	gzread(gzf, &timestamp, sizeof(unsigned long long));

	if (with_debug)
		std::cout << "read particles , num particles " << mNumParticles << " , in file: "<< filename << std::endl;

	// Sanity check
	const int partSysSize = sizeof(float) * 3 + sizeof(int);
	if (! (bytesPerElement == partSysSize) && (elementType == 0)){
		std::cout << "particle type doesn't match" << std::endl;
	}

	if (mNumParticles)
	{
		mParticlePositionsX.resize(mNumParticles);
		mParticlePositionsY.resize(mNumParticles);
		mParticlePositionsZ.resize(mNumParticles);
		mParticleFlags.resize(mNumParticles);

		for (int i = 0; i < mNumParticles; ++i) {
			gzread(gzf, fbuffer, sizeof(float) * 3);

			mParticlePositionsX[i] = fbuffer[0];
			mParticlePositionsY[i] = fbuffer[1];
			mParticlePositionsZ[i] = fbuffer[2];

			gzread(gzf, &ibuffer, sizeof(int));
			mParticleFlags[i] = ibuffer;
		}
	}

	gzclose(gzf);
}

void FLUID::updatePointers()
{
	if (with_debug)
		std::cout << "Updating pointers low res, ID: " << mCurrentID << std::endl;

	std::string id = std::to_string(mCurrentID);
	std::string solver = "s" + id;
	std::string solver_ext = "_" + solver;
	
	mObstacle    = (int*) getGridPointer("flags" + solver_ext,  solver);
	mNumObstacle = (int*) getGridPointer("numObs" + solver_ext, solver);
	
	mVelocityX = (float*) getGridPointer("x_vel" + solver_ext, solver);
	mVelocityY = (float*) getGridPointer("y_vel" + solver_ext, solver);
	mVelocityZ = (float*) getGridPointer("z_vel" + solver_ext, solver);
	
	mObVelocityX = (float*) getGridPointer("x_obvel" + solver_ext, solver);
	mObVelocityY = (float*) getGridPointer("y_obvel" + solver_ext, solver);
	mObVelocityZ = (float*) getGridPointer("z_obvel" + solver_ext, solver);
	
	mForceX    = (float*) getGridPointer("x_force" + solver_ext, solver);
	mForceY    = (float*) getGridPointer("y_force" + solver_ext, solver);
	mForceZ    = (float*) getGridPointer("z_force" + solver_ext, solver);
	
	mPhiObs = (float*) getGridPointer("phiObsIn" + solver_ext, solver);
	
	// Liquid
	if (mUsingLiquid) {
		mPhiIn  = (float*) getGridPointer("phiIn" + solver_ext,  solver);
		mPhiOut = (float*) getGridPointer("phiOut" + solver_ext, solver);
	}
	
	// Smoke
	if (mUsingSmoke) {
		mDensity        = (float*) getGridPointer("density" + solver_ext,     solver);
		mInflow         = (float*) getGridPointer("inflow"  + solver_ext,     solver);
		
		if (mUsingHeat) {
			mHeat       = (float*) getGridPointer("heat" + solver_ext,    solver);
		}
		if (mUsingFire) {
			mFlame      = (float*) getGridPointer("flame" + solver_ext,   solver);
			mFuel       = (float*) getGridPointer("fuel" + solver_ext,    solver);
			mReact      = (float*) getGridPointer("react" + solver_ext,   solver);
		}
		if (mUsingColors) {
			mColorR     = (float*) getGridPointer("color_r" + solver_ext, solver);
			mColorG     = (float*) getGridPointer("color_g" + solver_ext, solver);
			mColorB     = (float*) getGridPointer("color_b" + solver_ext, solver);
		}
	}
}

void FLUID::updatePointersHigh()
{
	if (with_debug)
		std::cout << "Updating pointers high res" << std::endl;

	std::string id = std::to_string(mCurrentID);
	std::string solver = "s" + id;
	std::string solver_ext = "_" + solver;

	std::string xlsolver = "xl" + id;
	std::string xlsolver_ext = "_" + xlsolver;

	// Liquid
	if (mUsingLiquid) {
		// Nothing to do here
	}
	
	// Smoke
	if (mUsingSmoke) {
		mDensityHigh    = (float*) getGridPointer("density"    + xlsolver_ext, xlsolver);
		mTextureU       = (float*) getGridPointer("texture_u"  + solver_ext,   solver);
		mTextureV       = (float*) getGridPointer("texture_v"  + solver_ext,   solver);
		mTextureW       = (float*) getGridPointer("texture_w"  + solver_ext,   solver);
		mTextureU2      = (float*) getGridPointer("texture_u2" + solver_ext,   solver);
		mTextureV2      = (float*) getGridPointer("texture_v2" + solver_ext,   solver);
		mTextureW2      = (float*) getGridPointer("texture_w2" + solver_ext,   solver);
		
		if (mUsingFire) {
			mFlameHigh  = (float*) getGridPointer("flame" + xlsolver_ext, xlsolver);
			mFuelHigh   = (float*) getGridPointer("fuel"  + xlsolver_ext, xlsolver);
			mReactHigh  = (float*) getGridPointer("react" + xlsolver_ext, xlsolver);
		}
		if (mUsingColors) {
			mColorRHigh = (float*) getGridPointer("color_r" + xlsolver_ext, xlsolver);
			mColorGHigh = (float*) getGridPointer("color_g" + xlsolver_ext, xlsolver);
			mColorBHigh = (float*) getGridPointer("color_b" + xlsolver_ext, xlsolver);
		}
	}
}

void FLUID::saveMesh(char *filename)
{
	std::string path(filename);
	
	mCommands.clear();
	std::ostringstream save_mesh_low;
	
	save_mesh_low <<  "save_mesh_low_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(save_mesh_low.str());
	
	runPythonString(mCommands);
}

void FLUID::saveMeshHigh(char *filename)
{
	std::string path(filename);
	
	mCommands.clear();
	std::ostringstream save_mesh_high;
	
	save_mesh_high <<  "save_mesh_high_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(save_mesh_high.str());
	
	runPythonString(mCommands);
}

void FLUID::saveParticles(char* filename)
{
	std::string path(filename);

	mCommands.clear();
	std::ostringstream save_particles_low;

	save_particles_low << "save_particles_low_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(save_particles_low.str());

	runPythonString(mCommands);
}

void FLUID::saveSmokeData(char *pathname)
{
	std::string path(pathname);
	
	mCommands.clear();
	std::ostringstream save_smoke_data_low;
	save_smoke_data_low <<  "save_smoke_data_low_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(save_smoke_data_low.str());
	
	runPythonString(mCommands);
}

void FLUID::saveSmokeDataHigh(char *pathname)
{
	std::string path(pathname);
	
	mCommands.clear();
	std::ostringstream save_smoke_data_high;
	save_smoke_data_high <<  "save_smoke_data_high_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(save_smoke_data_high.str());
	
	runPythonString(mCommands);
}

void FLUID::saveLiquidData(char *pathname)
{
	std::string path(pathname);
	
	mCommands.clear();
	std::ostringstream save_liquid_data_low;
	save_liquid_data_low <<  "save_liquid_data_low_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(save_liquid_data_low.str());
	
	runPythonString(mCommands);
}

void FLUID::saveLiquidDataHigh(char *pathname)
{
	std::string path(pathname);
	
	mCommands.clear();
	std::ostringstream save_liquid_data_high;
	save_liquid_data_high <<  "save_liquid_data_high_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(save_liquid_data_high.str());
	
	runPythonString(mCommands);
}

void FLUID::loadLiquidData(char *pathname)
{
	std::string path(pathname);
	
	mCommands.clear();
	std::ostringstream load_liquid_data_low;
	load_liquid_data_low <<  "load_liquid_data_low_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(load_liquid_data_low.str());
	
	runPythonString(mCommands);
}

void FLUID::loadLiquidDataHigh(char *pathname)
{
	std::string path(pathname);
	
	mCommands.clear();
	std::ostringstream load_liquid_data_high;
	load_liquid_data_high <<  "load_liquid_data_high_" << mCurrentID << "(r'" << path << "')";
	mCommands.push_back(load_liquid_data_high.str());
	
	runPythonString(mCommands);
}

