/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey 
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL) 
 * http://www.gnu.org/licenses
 *
 * Main file
 *
 ******************************************************************************/
#ifndef _MANTA_PYMAIN_CPP_
#define _MANTA_PYMAIN_CPP_

#include "pythonInclude.h"
#include <stdio.h>
#include "manta.h"
#include "../general.h"
#include "grid.h"
#include "fileio.h"
#include "wchar.h"
#include <fstream>
using namespace std;
namespace Manta {
	extern void guiMain(int argc, char* argv[]);
	extern void guiWaitFinish();
}

using namespace std;
using namespace Manta;

#if PY_MAJOR_VERSION >= 3
typedef wchar_t pyChar;
typedef wstring pyString;
#else
typedef char pyChar;
typedef string pyString;
#endif

//*****************************************************************************
// main...
static bool manta_initialized = false;
const static string clean_code1 = "def del_var(x): \n\
  print (\"deleting\", x) \n\
  try:\n\
    del x\n\
    print (\"deleted\") \n\
  except:\n\
    print (\"not deleted\") \n\
del_var(s) \n\
del_var(uvs)\n\
del_var(velInflow )\n\
del_var(res)\n\
del_var(gs) \n\
del_var(noise) \n\
del_var(source)\n\
del_var(sourceVel)\n\
del_var(flags) \n\
del_var(vel) \n\
del_var(density) \n\
del_var(pressure) \n\
del_var(energy) \n\
del_var(tempFlag)\n\
del_var(sdf_flow)\n\
del_var(source_shape)";
const static string clean_code2 = "del s;del noise;del xl;del xl_noise;del xl_wltnoise;";
		   //for latter full object release	
		   //const static string clean_code2 = "del [s, noise, source, sourceVel, xl, xl_vel, xl_density, xl_flags,xl_source, xl_noise, flags, vel, density, pressure, energy, tempFlag, sdf_flow, forces, source,source_shape, xl_wltnoise]";

void export_fields(int size_x, int size_y, int size_z, float *f_x, float*f_y, float*f_z, char *filename)
{
	assert(size_x>0 && size_y>0 && size_z>0);
	assert(f_x != NULL);
	assert(f_y != NULL);
	assert(f_z != NULL);
	FluidSolver dummy(Vec3i(size_x,size_y,size_z));
	Grid<Vec3> force_fields(&dummy, false);
	for (int x=0; x < size_x; ++x)
	{
		for (int y=0; y < size_y; ++y)
		{
			for (int z=0; z < size_z; ++z)
			{
				force_fields.get(x, y, z) = Vec3(f_x[x],f_y[y],f_z[z]);
			}
		}
	}
	writeGridUni(filename, &force_fields);
	/*rename after export successful*/
	
//	writeGridTxt("s.txt", &force_fields);
}
		   
void export_em_fields(float flow_density, int min_x, int min_y, int min_z, int max_x, int max_y, int max_z, int d_x, int d_y, int d_z, float *inf, float *vel)
{
//	assert(size_x>0 && size_y>0 && size_z>0);
	assert(inf != NULL);
//	assert(vel != NULL);
	FluidSolver dummy(Vec3i(d_x,d_y,d_z));
	Grid<Real> em_inf_fields(&dummy, false);
	em_inf_fields.clear();
	const char* influence_name = "manta_em_influence.uni";
//	const char* velocity_name = "em_vel_fields.uni";
	ifstream em_file(influence_name);
	if (em_file.good()) {
//		em_inf_fields.load(influence_name);	
	}
	em_file.close();
	
//	Grid<Vec3> em_vel_fields(&dummy, false);
//	ifstream vel_file(velocity_name);
//	if (vel_file.good()) {
//		em_vel_fields.load(velocity_name);	
//	}
//	vel_file.close();
	int index(0);
	Vec3i em_size(max_x - min_x, max_y - min_y, max_z - min_z);
	int em_size_x =em_size[0];
	int em_size_xy = em_size[0] * em_size[1];
	for (int x=0; x < em_size[0]; ++x)
	{
		for (int y=0; y < em_size[1]; ++y)
		{
			for (int z=0; z < em_size[2]; ++z)
			{
					index = x + y * em_size_x + z * em_size_xy;
					em_inf_fields.get(x + min_x, y + min_y, z + min_z) += flow_density * inf[index];//f_x[x],f_y[y],f_z[z]);				
//					if(vel != NULL)	
//						em_vel_fields.get(x, y, z) = Vec3(vel[index*3],vel[index*3+1],vel[index*3+2]);
			}
		}
	}
	writeGridUni("manta_em_influence.uni", &em_inf_fields);
	writeGridTxt("manta_em_influence.txt", &em_inf_fields);
//	if (vel != NULL){
//		writeGridUni("em_vel_fields.uni", &em_vel_fields);
//		writeGridTxt("em_vel_fields.txt", &em_vel_fields);
//	}
}

void export_force_fields(int size_x, int size_y, int size_z, float *f_x, float*f_y, float*f_z)
{
	export_fields(size_x, size_y, size_z, f_x, f_y, f_z, "manta_forces.uni");	
}
		   
void runMantaScript(const string& ss,vector<string>& args) {
	string filename = args[0];
	
	// Initialize extension classes and wrappers
	srand(0);
	PyGILState_STATE gilstate = PyGILState_Ensure();
	/*cleaning possible previous setups*/
	PyRun_SimpleString(clean_code2.c_str());
	
	if (! manta_initialized)
	{	
		debMsg("running manta init?", 0);
		Pb::setup(filename, args);
		manta_initialized = true;
	}	
	// Pass through the command line arguments
	// for Py3k compatability, convert to wstring
	vector<pyString> pyArgs(args.size());
	const pyChar ** cargs = new const pyChar*  [args.size()];
	for (size_t i=0; i<args.size(); i++) {
		pyArgs[i] = pyString(args[i].begin(), args[i].end());
		cargs[i] = pyArgs[i].c_str();
	}
	PySys_SetArgv( args.size(), (pyChar**) cargs);
	
	// Try to load python script
	FILE* fp = fopen(filename.c_str(),"rb");
	if (fp == NULL) {
		debMsg("Cannot open '" << filename << "'", 0);
		Pb::finalize();
		return;
	}
	
	// Run the python script file
	debMsg("Loading script '" << filename << "'", 0);
#if defined(WIN32) || defined(_WIN32)
	// known bug workaround: use simplestring
	fseek(fp,0,SEEK_END);
	long filelen=ftell(fp);
	fseek(fp,0,SEEK_SET);
	char* buf = new char[filelen+1];
	fread(buf,filelen,1,fp);
	buf[filelen] = '\0';
	fclose(fp);
	PyRun_SimpleString(buf);
	delete[] buf;    
#else
	// for linux, use this as it produces nicer error messages
	string toExec = "";
	
//	PyRun_SimpleString(ss.c_str());
	PyRun_SimpleFileEx(fp, filename.c_str(), 0);    
//	for (int frame=0; frame < 4; ++frame)
//	{
//		std::string frame_str = static_cast<ostringstream*>( &(ostringstream() << frame) )->str();
//		std::string py_string_0 = string("sim_step(").append(frame_str);
//		std::string py_string_1 = py_string_0.append(")\0");
//		PyRun_SimpleString(py_string_1.c_str());
//	}
	if (fp != NULL){
		fclose(fp);    
	}
#endif
	
	debMsg("Script finished.", 0);
#ifdef GUI
//	guiWaitFinish();
#endif

	// finalize
//	Pb::finalize();
	PyGILState_Release(gilstate);

	delete [] cargs;
}

//int manta_main(int argc,char* argv[]) {
//	debMsg("Version: "<< buildInfoString() , 1);
//
//#ifdef GUI
//	guiMain(argc, argv);    
//#else
//	if (argc<=1) {
//		cerr << "Usage : Syntax is 'manta <config.py>'" << endl;  
//		return 1;
//	}
//
//	vector<string> args;
//	for (int i=1; i<argc; i++) args.push_back(argv[i]);
//	runMantaScript(args);
//#endif        		
//
//	return 0;
//}
#endif