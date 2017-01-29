// Copyright (c) 2014, 2016 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Author: mierle@gmail.com (Keir Mierle)
//         shentianweipku@gmail.com (Tianwei Shen)

#ifndef LIBMV_AUTOTRACK_RECONSTRUCTION_H_
#define LIBMV_AUTOTRACK_RECONSTRUCTION_H_

#include "libmv/base/vector.h"
#include "libmv/numeric/numeric.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"

namespace mv {

using libmv::CameraIntrinsics;
using libmv::vector;
using libmv::Mat3;
using libmv::Vec3;

class Model;
struct Marker;
class Tracks;

struct CameraPose {
  CameraPose(): clip(-1), frame(-1) {}	// uninitilized CameraPose is (-1, -1)
  CameraPose(int clip_, int frame_, int intrinsics_, Mat3 R_, Vec3 t_):
	    clip(clip_), frame(frame_), intrinsics(intrinsics_), R(R_), t(t_) {}

  int clip;
  int frame;
  int intrinsics;
  Mat3 R;
  Vec3 t;
};

struct Point {
  Point(int track_ = -1, Vec3 X_ = Vec3(0, 0, 0))
      : track(track_), X(X_){}
  Point(const Point &p) : track(p.track), X(p.X) {}
  int track;

  // The coordinates of the point. Note that not all coordinates are always
  // used; for example points on a plane only use the first two coordinates.
  Vec3 X;
};

// A reconstruction for a set of tracks. The indexing for clip, frame, and
// track should match that of a Tracks object, stored elsewhere.
class Reconstruction {
 public:
  // All methods copy their input reference or take ownership of the pointer.
  void AddCameraPose(const CameraPose& pose);
  int AddCameraIntrinsics(CameraIntrinsics* intrinsics_ptr);
  int AddPoint(const Point& point);
  int  AddModel(Model* model);

  // Returns the corresponding pose or point or NULL if missing.
  CameraPose* CameraPoseForFrame(int clip, int frame);
  const CameraPose* CameraPoseForFrame(int clip, int frame) const;
  Point* PointForTrack(int track);
  const Point* PointForTrack(int track) const;

  const vector<vector<CameraPose> >& camera_poses() const;
  const vector<Point>& AllPoints() const;

  int GetClipNum() const;
  int GetAllPoseNum() const;
  int GetReconstructedCameraNum() const;

  // initialize all intrinsics map to -1
  void InitIntrinsicsMap(Tracks &tracks);
  // initialize intrinsics of clip i to i(CameraPose::intrinsics)
  void InitIntrinsicsMapFixed(Tracks &tracks);
  // set CameraPose::intrinsics for frame (clip, frame)
  bool SetIntrinsicsMap(int clip, int frame, int intrinsics);
  // return CameraPose::intrinsics if (clip, frame) is intrinsics_map, otherwise return -1
  int GetIntrinsicsMap(int clip, int frame) const;

private:
  // Indexed by CameraPose::intrinsics. Owns the intrinsics objects.
  vector<CameraIntrinsics*> camera_intrinsics_;
  // Indexed by Marker::clip then by Marker::frame.
  vector<vector<CameraPose> > camera_poses_;
  // Indexed by Marker::track.
  vector<Point> points_;
  // Indexed by Marker::model_id. Owns model objects.
  vector<Model*> models_;
  // Indexed by Marker::clip then by Marker::frame.
  vector<vector<int> > intrinsics_map;
};

// Reconstruct two frames from the same clip, used as the initial reconstruction
bool ReconstructTwoFrames(const vector<Marker> &markers,
                          const int clip,
                          Reconstruction *reconstruction);
}  // namespace mv

#endif  // LIBMV_AUTOTRACK_RECONSTRUCTION_H_
