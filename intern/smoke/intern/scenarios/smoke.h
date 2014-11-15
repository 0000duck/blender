#include <string>
using namespace std;
const string smoke_clean = "";

const string smoke_setup_low ="from manta import *\n\
import os, shutil, math, sys\n\
def transform_back(obj, gs):\n\
  obj.scale(gs/2)\n\
  obj.offset(gs/2)\n\
\n\
def load_once(grid, file, dict):\n\
  if grid not in dict:\n\
    print('Loading file' + file + 'in grid')\n\
    grid.load(file)\n\
    dict[grid] = 1\n\
# solver params\n\
res = $RES$\n\
gs = vec3($RESX$,$RESY$,$RESZ$)\n\
s = FluidSolver(name='main', gridSize = gs)\n\
s.timestep = 0.1\n\
timings = Timings()\n\
\n\
# prepare grids\n\
flags = s.create(FlagGrid)\n\
vel = s.create(MACGrid)\n\
density = s.create(LevelsetGrid)\n\
pressure = s.create(RealGrid)\n\
\n\
# noise field\n\
noise = s.create(NoiseField, loadFromFile=True)\n\
noise.posScale = vec3(45)\n\
noise.clamp = True\n\
noise.clampNeg = 0\n\
noise.clampPos = 1\n\
noise.valScale = 1\n\
noise.valOffset = 0.75\n\
noise.timeAnim = 0.2\n\
\n\
flags.initDomain()\n\
flags.fillGrid()\n\
\n\
source_grid = s.create(RealGrid)\n\
source = s.create(Mesh)\n\
forces = s.create(MACGrid)\n\
dict_loaded = dict()\n\
manta_using_colors = False\n\
manta_using_heat = False\n\
low_flags_updated = False\n\
";

const string smoke_setup_high = "xl_gs = vec3($HRESX$, $HRESY$, $HRESZ$) \n\
xl = Solver(name = 'larger', gridSize = xl_gs) \n\
uvs =$UVS_CNT$\n\
if $USE_WAVELETS$:\n\
  upres = $UPRES$\n\
  wltStrength = $WLT_STR$\n\
  if $UPRES$ > 0:\n\
    octaves = int( math.log(upres)/ math.log(2.0) + 0.5 ) \n\
  else:\n\
    octaves = 0\n\
if $USE_WAVELETS$ and $UPRES$ > 0:\n\
  xl.timestep = $XL_TIMESTEP$ \n\
  xl_vel = xl.create(MACGrid) \n\
  xl_density = xl.create(RealGrid) \n\
  xl_flags = xl.create(FlagGrid) \n\
  xl_flags.initDomain() \n\
  xl_flags.fillGrid() \n\
  #xl_source = s.create(Mesh)\n\
  #xl_source.load('manta_flow.obj')\n\
  #transform_back(xl_source, gs)\n\
  xl_noise = xl.create(NoiseField, fixedSeed=256, loadFromFile=True) \n\
  xl_noise.posScale = vec3(20) \n\
  xl_noise.clamp = False \n\
  xl_noise.clampNeg = $NOISE_CN$ \n\
  xl_noise.clampPos = $NOISE_CP$ \n\
  xl_noise.valScale = $NOISE_VALSCALE$ \n\
  xl_noise.valOffset = $NOISE_VALOFFSET$ \n\
  xl_noise.timeAnim = $NOISE_TIMEANIM$ * $UPRES$ \n\
  xl_wltnoise = xl.create(NoiseField, loadFromFile=True) \n\
  xl_wltnoise.posScale = vec3( int(1.0*gs.x) ) * 0.5 \n\
  xl_wltnoise.posScale = xl_wltnoise.posScale * 0.5\n\
  xl_wltnoise.timeAnim = 0.1 \n\
";

const string smoke_init_colors_low = "print(\"INitializing Colors\")\n\
color_r_low = s.create(RealGrid)\n\
color_g_low = s.create(RealGrid)\n\
color_b_low = s.create(RealGrid)\n\
color_r_low.add(density) \n\
color_r_low.multConst(manta_color_r) \n\
\n\
color_g_low.add(density) \n\
color_g_low.multConst(manta_color_g) \n\
\n\
color_b_low.add(density) \n\
color_b_low.multConst(manta_color_b) \n\
manta_using_colors = True\n";

const string smoke_del_colors_low = "\n\
del color_r_low \n\
del color_g_low \n\
del color_b_low \n\
manta_using_colors = False";

const string smoke_init_colors_high = "print(\"INitializing Colors highres\")\n\
color_r_high = xl.create(RealGrid)\n\
color_g_high = xl.create(RealGrid)\n\
color_b_high = xl.create(RealGrid)\n\
color_r_high.add(xl_density) \n\
color_r_high.multConst(manta_color_r) \n\
\n\
color_g_high.add(xl_density) \n\
color_g_high.multConst(manta_color_g) \n\
\n\
color_b_high.add(xl_density) \n\
color_b_high.multConst(manta_color_b) \n\
manta_using_colors = True\n";

const string smoke_init_heat_low = "print(\"INitializing heat lowres\")\n\
heat_low = s.create(RealGrid)\n\
manta_using_heat = True\n";

const string smoke_del_colors_high = "\n\
del color_r_high \n\
del color_g_high \n\
del color_b_high \n\
manta_using_colors = False";


const string smoke_step_low = "def sim_step_low(t):\n\
  print ('Step:' + str(t))\n\
  if \"abc123\" in globals():\n\
    print (abc123)\n\
  #load_once(source,'manta_flow.obj',dict_loaded)\n\
  #if t == 2:#loading data on first sim frame only\n\
  #  print('First frame: loading flows and obstacles')\n\
  #  source.load('manta_flow.obj')\n\
  #  transform_back(source, gs)\n\
  print (\"Density \" , str(density), str(density.getDataPointer()))\n\
  #load emission data\n\
  #source_grid.load('manta_em_influence.uni')\n\
  #density.add(source_grid)\n\
  \n\
  if manta_using_colors:\n\
    advectSemiLagrange(flags=flags, vel=vel, grid=color_r_low, order=$ADVECT_ORDER$)\n\
    advectSemiLagrange(flags=flags, vel=vel, grid=color_g_low, order=$ADVECT_ORDER$)\n\
    advectSemiLagrange(flags=flags, vel=vel, grid=color_b_low, order=$ADVECT_ORDER$)\n\
  advectSemiLagrange(flags=flags, vel=vel, grid=density, order=$ADVECT_ORDER$)\n\
  advectSemiLagrange(flags=flags, vel=vel, grid=vel    , order=$ADVECT_ORDER$, strength=1.0)\n\
  \n\
  setWallBcs(flags=flags, vel=vel)    \n\
      #buoyancy calculated in Blender, from _heat fields\n\
  #addBuoyancy(density=density, vel=vel, gravity=vec3($BUYO_X$,$BUYO_Y$,$BUYO_Z$), flags=flags)\n\
  if manta_using_heat:\n\
    addHeatBuoyancy(density=density, densCoeff = $ALPHA$, vel=vel, gravity=$GRAVITY$, flags=flags, heat = heat_low, heatCoeff = $BETA$*10)\n\
  if $VORTICITY$ > 0.01:\n\
    vorticityConfinement( vel=vel, flags=flags, strength=$VORTICITY$ ) \n\
  addForceField(flags=flags, vel=vel,force=forces)\n\
  \n\
  solvePressure(flags=flags, vel=vel, pressure=pressure, useResNorm=True, openBound='$BOUNDCONDITIONS$')\n\
  setWallBcs(flags=flags, vel=vel)\n\
  \n\
  s.step()\n";
//  if (t>=0 and t<75):\n\
//    densityInflow(flags=flags, density=density, noise=noise, shape=source, scale=1, sigma=0.5)\n\
//    #if noise.valScale > 0.:\n\
//    #  densityInflowMeshNoise( flags=flags, density=density, noise=noise, mesh=source, scale=3, sigma=0.5 )\n\
//    #else:\n\
//    #  densityInflowMesh(flags=flags, density=density, mesh=source, value=1)\n\
//    #applyInflow=True\n\
//  addForceField(flags=flags, vel=vel,force=forces)\n\
//  advectSemiLagrange(flags=flags, vel=vel, grid=density, order=$ADVECT_ORDER$) \n\
//  advectSemiLagrange(flags=flags, vel=vel, grid=vel, order=$ADVECT_ORDER$, strength=1.0) \n\
//  setWallBcs(flags=flags, vel=vel) \n\
//  addBuoyancy(density=density, vel=vel, gravity=vec3($BUYO_X$,$BUYO_Y$,$BUYO_Z$), flags=flags) \n\
//  solvePressure(flags=flags, vel=vel, pressure=pressure, useResNorm=True, openBound='xXyYzZ', cgMaxIterFac=1, cgAccuracy=0.01) \n\
//  setWallBcs(flags=flags, vel=vel) \n\
//  print(\"Writing Grid to \" + str($DENSITY_MEM$) + \" with size\" + str($DENSITY_SIZE$))\n\
//  density.writeGridToMemory(memLoc = \"$DENSITY_MEM$\",sizeAllowed = \"$DENSITY_SIZE$\") \n\
//  #density.save('den%04d_temp.uni' % t) \n\
//  #os.rename('den%04d_temp.uni' % t, 'den%04d.uni' % t) \n\
//  s.step()\n";

const string liquid_step_low = "def sim_step_low(t):\n\
#update flags form density on first step\n\
  density.multConst(-1.)\n\
  print (manta_using_colors)\n\
  global low_flags_updated\n\
  if not low_flags_updated:\n\
    print ('Updating Flags from Levelset on startup!')\n\
    flags.updateFromLevelset(density)\n\
  low_flags_updated = True \n\
  density.reinitMarching(flags=flags, velTransport=vel)\n\
  accuracy = 5e-5\n\
  advectSemiLagrange(flags=flags, vel=vel, grid=density, order=2)\n\
  flags.updateFromLevelset(density)\n\
  \n\
  advectSemiLagrange(flags=flags, vel=vel, grid=vel, order=2)\n\
  addGravity(flags=flags, vel=vel, gravity=vec3(0,0,-0.981))\n\
  \n\
  # print current maximal velocity\n\
  maxvel = vel.getMaxValue()\n\
  print ('Current max velocity %f ' % maxvel)\n\
  \n\
  # pressure solve\n\
  setWallBcs(flags=flags, vel=vel)\n\
  solvePressure(flags=flags, vel=vel, pressure=pressure, cgMaxIterFac=0.5, cgAccuracy=accuracy, useResNorm=False) \n\
  setWallBcs(flags=flags, vel=vel)\n\
  s.step()\n\
  density.multConst(-1.)\n\
";

const string smoke_step_high = "def sim_step_high(t):\n\
  interpolateMACGrid( source=vel, target=xl_vel ) \n\
  sStr = 1.0 * wltStrength  \n\
  sPos = 2.0  \n\
  for o in range(octaves): \n\
    for i in range(uvs): \n\
      uvWeight = getUvWeight(uv[i])  \n\
      applyNoiseVec3( flags=xl_flags, target=xl_vel, noise=xl_wltnoise, scale=sStr * uvWeight, scaleSpatial=sPos , weight=energy, uv=uv[i] ) \n\
    sStr *= 0.06 # magic kolmogorov factor \n\
    sPos *= 2.0 \n\
  for substep in range(upres):  \n\
    advectSemiLagrange(flags=xl_flags, vel=xl_vel, grid=xl_density, order=$ADVECT_ORDER$)  \n\
    if manta_using_colors:\n\
      advectSemiLagrange(flags=xl_flags, vel=xl_vel, grid=color_r_high, order=$ADVECT_ORDER$)\n\
      advectSemiLagrange(flags=xl_flags, vel=xl_vel, grid=color_g_high, order=$ADVECT_ORDER$)\n\
      advectSemiLagrange(flags=xl_flags, vel=xl_vel, grid=color_b_high, order=$ADVECT_ORDER$)\n\
\n\
  xl.step()\n";

const string full_smoke_setup = "from manta import * \n\
import os, shutil, math, sys \n\
def transform_back(obj, gs):\n\
	obj.scale(gs/2)\n\
	obj.offset(gs/2)\n\
\n\
uvs = $UVS_CNT$\n\
solver_dim = $SOLVER_DIM$\n\
velInflow = vec3(0, 0, 1)\n\
if $USE_WAVELETS$:\n\
	upres = $UPRES$\n\
	wltStrength = $WLT_STR$\n\
	if $UPRES$ > 0:\n\
		octaves = int( math.log(upres)/ math.log(2.0) + 0.5 ) \n\
	else:\n\
		octaves = 0\n\
res = $RES$\n\
gs = vec3($RESX$, $RESY$, $RESZ$) \n\
s = Solver(name = 'main', gridSize = gs, dim = solver_dim) \n\
s.timestep = $TIMESTEP$ \n\
noise = s.create(NoiseField, fixedSeed=256, loadFromFile=True) \n\
noise.posScale = vec3(20) \n\
noise.clamp = False \n\
noise.clampNeg = $NOISE_CN$\n\
noise.clampPos = $NOISE_CP$\n\
noise.valScale = $NOISE_VALSCALE$\n\
noise.valOffset = $NOISE_VALOFFSET$\n\
noise.timeAnim = $NOISE_TIMEANIM$ \n\
source = s.create(Mesh)\n\
source.load('manta_flow.obj')\n\
transform_back(source, gs)\n\
sourceVel = s.create(Mesh)\n\
sourceVel.load('manta_flow.obj')\n\
transform_back(sourceVel, gs)\n\
xl_gs = vec3($HRESX$, $HRESY$, $HRESZ$) \n\
xl = Solver(name = 'larger', gridSize = xl_gs, dim = solver_dim) \n\
if $USE_WAVELETS$ and $UPRES$ > 0:\n\
	xl.timestep = $XL_TIMESTEP$ \n\
	xl_vel = xl.create(MACGrid) \n\
	xl_density = xl.create(RealGrid) \n\
	xl_flags = xl.create(FlagGrid) \n\
	xl_flags.initDomain() \n\
	xl_flags.fillGrid() \n\
	xl_source = s.create(Mesh)\n\
	xl_source.load('manta_flow.obj')\n\
	transform_back(xl_source, gs)\n\
	xl_noise = xl.create(NoiseField, fixedSeed=256, loadFromFile=True) \n\
	xl_noise.posScale = vec3(20) \n\
	xl_noise.clamp = False \n\
	xl_noise.clampNeg = $NOISE_CN$ \n\
	xl_noise.clampPos = $NOISE_CP$ \n\
	xl_noise.valScale = $NOISE_VALSCALE$ \n\
	xl_noise.valOffset = $NOISE_VALOFFSET$ \n\
	xl_noise.timeAnim = $NOISE_TIMEANIM$ * $UPRES$ \n\
flags = s.create(FlagGrid) \n\
flags.initDomain() \n\
flags.fillGrid() \n\
uv = [] \n\
for i in range(uvs): \n\
	uvGrid = s.create(VecGrid) \n\
	uv.append(uvGrid) \n\
	resetUvGrid( uv[i] ) \n\
vel = s.create(MACGrid) \n\
density = s.create(RealGrid) \n\
pressure = s.create(RealGrid) \n\
energy = s.create(RealGrid) \n\
tempFlag  = s.create(FlagGrid)\n\
sdf_flow  = s.create(LevelsetGrid)\n\
forces = s.create(MACGrid)\n\
source.meshSDF(source, sdf_flow, 1.1)\n\
source_shape = s.create(Cylinder, center=gs*vec3(0.5,0.1,0.5), radius=res*0.14, z=gs*vec3(0, 0.02, 0))\n\
xl_wltnoise = s.create(NoiseField, loadFromFile=True) \n\
xl_wltnoise.posScale = vec3( int(1.0*gs.x) ) * 0.5 \n\
xl_wltnoise.posScale = xl_wltnoise.posScale * 0.5\n\
xl_wltnoise.timeAnim = 0.1 \n\
\n\
\n\
def sim_step(t):\n\
	forces.load('manta_forces.uni')\n\
	addForceField(flags=flags, vel=vel,force=forces)\n\
	addBuoyancy(density=density, vel=vel, gravity=vec3($BUYO_X$,$BUYO_Y$,$BUYO_Z$), flags=flags) \n\
	advectSemiLagrange(flags=flags, vel=vel, grid=density, order=$ADVECT_ORDER$) \n\
	advectSemiLagrange(flags=flags, vel=vel, grid=vel, order=$ADVECT_ORDER$) \n\
	for i in range(uvs): \n\
		advectSemiLagrange(flags=flags, vel=vel, grid=uv[i], order=$ADVECT_ORDER$) \n\
		updateUvWeight( resetTime=16.5 , index=i, numUvs=uvs, uv=uv[i] )\n\
	applyInflow=False\n\
	if (t>=0 and t<75):\n\
		densityInflowMesh(flags=flags, density=density, mesh=source, value=1)\n\
		applyInflow=True\n\
	setWallBcs(flags=flags, vel=vel) \n\
	vorticityConfinement( vel=vel, flags=flags, strength=0.2 ) \n\
	solvePressure(flags=flags, vel=vel, pressure=pressure, useResNorm=True, openBound='xXyYzZ', cgMaxIterFac=1, cgAccuracy=0.01) \n\
	setWallBcs(flags=flags, vel=vel) \n\
	computeEnergy(flags=flags, vel=vel, energy=energy)\n\
	tempFlag.copyFrom(flags)\n\
	extrapolateSimpleFlags( flags=flags, val=tempFlag, distance=2, flagFrom=FlagObstacle, flagTo=FlagFluid )\n\
	extrapolateSimpleFlags( flags=tempFlag, val=energy, distance=6, flagFrom=FlagFluid, flagTo=FlagObstacle )\n\
	computeWaveletCoeffs(energy)\n\
	print(\"Writing Grid to \" + $DENSITY_MEM$ + \"with size\" + $DENSITY_SIZE$)\n\
	density.writeGridToMemory(memLoc = $DENSITY_MEM$,sizeAllowed = $DENSITY_SIZE$)\n\
	density.save('den%04d_temp.uni' % t) \n\
	os.rename('den%04d_temp.uni' % t, 'den%04d.uni' % t) \n\
	s.step()\n\
	\n\
	interpolateMACGrid( source=vel, target=xl_vel ) \n\
	sStr = 1.0 * wltStrength  \n\
	sPos = 2.0  \n\
	for o in range(octaves): \n\
		for i in range(uvs): \n\
			uvWeight = getUvWeight(uv[i])  \n\
			applyNoiseVec3( flags=xl_flags, target=xl_vel, noise=xl_wltnoise, scale=sStr * uvWeight, scaleSpatial=sPos , weight=energy, uv=uv[i] ) \n\
		sStr *= 0.06 # magic kolmogorov factor \n\
		sPos *= 2.0 \n\
	for substep in range(upres):  \n\
		advectSemiLagrange(flags=xl_flags, vel=xl_vel, grid=xl_density, order=$ADVECT_ORDER$)  \n\
	if (applyInflow): \n\
		densityInflowMesh(flags=xl_flags, density=xl_density, mesh=source, value=1)\n\
	xl_density.save('densityXl_%04d.uni' % t)\n\
	xl.step()\n\
";
