/*
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
 * Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_profilewidget_types.h"
#include "DNA_curve_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "BKE_profile_widget.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"

#define DEBUG_PRWDGT_EVALUATE

void BKE_profilewidget_free_data(ProfileWidget *prwdgt)
{
  if (prwdgt->path) {
    MEM_freeN(prwdgt->path);
    prwdgt->path = NULL;
  }
  if (prwdgt->table) {
    MEM_freeN(prwdgt->table);
    prwdgt->table = NULL;
  }
  if (prwdgt->segments) {
    MEM_freeN(prwdgt->segments);
    prwdgt->segments = NULL;
  }
}

void BKE_profilewidget_free(ProfileWidget *prwdgt)
{
  if (prwdgt) {
    BKE_profilewidget_free_data(prwdgt);
    MEM_freeN(prwdgt);
  }
}

void BKE_profilewidget_copy_data(ProfileWidget *target, const ProfileWidget *prwdgt)
{
  *target = *prwdgt;

  if (prwdgt->path) {
    target->path = MEM_dupallocN(prwdgt->path);
  }
  if (prwdgt->table) {
    target->table = MEM_dupallocN(prwdgt->table);
  }
  if (prwdgt->segments) {
    target->segments = MEM_dupallocN(prwdgt->segments);
  }
}

ProfileWidget *BKE_profilewidget_copy(const ProfileWidget *prwdgt)
{
  if (prwdgt) {
    ProfileWidget *new_prdgt = MEM_dupallocN(prwdgt);
    BKE_profilewidget_copy_data(new_prdgt, prwdgt);
    return new_prdgt;
  }
  return NULL;
}

/** Removes a specific point from the path of control points.
 * \note: Requiress profilewidget_changed call after. */
bool BKE_profilewidget_remove_point(ProfileWidget *prwdgt, ProfilePoint *point)
{
  ProfilePoint *pts;
  int i_old, i_new, n_removed = 0;

  /* Must have 2 points minimum. */
  if (prwdgt->totpoint <= 2) {
    return false;
  }

  pts = MEM_mallocN((size_t)prwdgt->totpoint * sizeof(ProfilePoint), "path points");

  /* Build the new list without the point when it's found. Keep the first and last points. */
  for (i_old = 1, i_new = 0; i_old < prwdgt->totpoint - 1; i_old++) {
    if (&prwdgt->path[i_old] != point) {
      pts[i_new] = prwdgt->path[i_old];
      i_new++;
    }
    else {
      n_removed++;
    }
  }

  MEM_freeN(prwdgt->path);
  prwdgt->path = pts;
  prwdgt->totpoint -= n_removed;
  return (n_removed != 0);
}

/** Removes every point in the widget with the supplied flag set, except for the first and last.
 * \param flag: ProfilePoint->flag.
 * \note: Requiress profilewidget_changed call after. */
void BKE_profilewidget_remove(ProfileWidget *prwdgt, const short flag)
{
  int i_old, i_new, n_removed = 0;

  /* Copy every point without the flag into the new path. */
  ProfilePoint *new_pts = MEM_mallocN(((size_t)prwdgt->totpoint) * sizeof(ProfilePoint),
                                  "path points");

  /* Build the new list without any of the points with the flag. Keep the first and last points. */
  new_pts[0] = prwdgt->path[0];
  for (i_old = 1, i_new = 1; i_old < prwdgt->totpoint - 1; i_old++) {
    if (!(prwdgt->path[i_old].flag & flag)) {
      new_pts[i_new] = prwdgt->path[i_old];
      i_new++;
    }
    else {
      n_removed++;
    }
  }
  new_pts[i_new] = prwdgt->path[i_old];

  MEM_freeN(prwdgt->path);
  prwdgt->path = new_pts;
  prwdgt->totpoint -= n_removed;
}

/** Adds a new point at the specified location. The choice for which points to place the new vertex
 * between is made by checking which control point line segment is closest to the new point and
 * placing the new vertex in between that segment's points.
 * \note: Requiress profilewidget_changed call after. */
ProfilePoint *BKE_profilewidget_insert(ProfileWidget *prwdgt, float x, float y)
{
  ProfilePoint *new_pt = NULL;
  float new_loc[2] = {x, y};

  /* Don't add more control points  than the maximum size of the higher resolution table. */
  if (prwdgt->totpoint == PROF_TABLE_MAX - 1) {
    return NULL;
  }

  /* Find the index at the line segment that's closest to the new position. */
  float distance;
  float min_distance = FLT_MAX;
  int insert_i = 0;
  for (int i = 0; i < prwdgt->totpoint - 1; i++) {
    float loc1[2] = {prwdgt->path[i].x, prwdgt->path[i].y};
    float loc2[2] = {prwdgt->path[i + 1].x, prwdgt->path[i + 1].y};

    distance = dist_squared_to_line_segment_v2(new_loc, loc1, loc2);
    if (distance < min_distance) {
      min_distance = distance;
      insert_i = i + 1;
    }
  }

  /* Insert the new point at the location we found and copy all of the old points in as well. */
  prwdgt->totpoint++;
  ProfilePoint *new_pts = MEM_mallocN(((size_t)prwdgt->totpoint) * sizeof(ProfilePoint),
                                      "path points");
  for (int i_new = 0, i_old = 0; i_new < prwdgt->totpoint; i_new++) {
    if (i_new != insert_i) {
      /* Insert old points */
      new_pts[i_new].x = prwdgt->path[i_old].x;
      new_pts[i_new].y = prwdgt->path[i_old].y;
      new_pts[i_new].flag = prwdgt->path[i_old].flag & ~PROF_SELECT; /* Deselect old points. */
      i_old++;
    }
    else {
      /* Insert new point. */
      new_pts[i_new].x = x;
      new_pts[i_new].y = y;
      new_pts[i_new].flag = PROF_SELECT;
      new_pt = &new_pts[i_new];
    }
  }

  /* Free the old points and use the new ones. */
  MEM_freeN(prwdgt->path);
  prwdgt->path = new_pts;
  return new_pt;
}

/** Sets the handle type of the selected control points.
 * \param type: Either HD_VECT or HD_AUTO.
 * \note: Requiress profilewidget_changed call after. */
void BKE_profilewidget_handle_set(ProfileWidget *prwdgt, int type)
{
  for (int i = 0; i < prwdgt->totpoint; i++) {
    if (prwdgt->path[i].flag & PROF_SELECT) {
      prwdgt->path[i].flag &= ~(PROF_HANDLE_VECTOR | PROF_HANDLE_AUTO);
      if (type == HD_VECT) {
        prwdgt->path[i].flag |= PROF_HANDLE_VECTOR;
      }
      else if (type == HD_AUTO) {
        prwdgt->path[i].flag |= PROF_HANDLE_AUTO;
      }
    }
  }
}

/** Flips the profile across the diagonal so that its orientation is reversed.
 * \note: Requiress profilewidget_changed call after.  */
void BKE_profilewidget_reverse(ProfileWidget *prwdgt)
{
  /* Quick fix for when there are only two points and reversing shouldn't do anything */
  if (prwdgt->totpoint == 2) {
    return;
  }
  ProfilePoint *new_pts = MEM_mallocN(((size_t)prwdgt->totpoint) * sizeof(ProfilePoint),
                                      "path points");
  /* Mirror the new points across the y = x line */
  for (int i = 0; i < prwdgt->totpoint; i++) {
    new_pts[prwdgt->totpoint - i - 1].x = prwdgt->path[i].y;
    new_pts[prwdgt->totpoint - i - 1].y = prwdgt->path[i].x;
    new_pts[prwdgt->totpoint - i - 1].flag = prwdgt->path[i].flag;
  }

  /* Free the old points and use the new ones */
  MEM_freeN(prwdgt->path);
  prwdgt->path = new_pts;
}

/** Puts the widget's control points in a step pattern. Uses vector handles for each point. */
static void profilewidget_build_steps(ProfileWidget *prwdgt)
{
  int n, step_x, step_y;
  float n_steps_x, n_steps_y;

  n = prwdgt->totpoint;

  n_steps_x = (n % 2 == 0) ? n : (n - 1);
  n_steps_y = (n % 2 == 0) ? (n - 2) : (n - 1);

  for (int i = 0; i < n; i++) {
    step_x = (i + 1) / 2;
    step_y = i / 2;
    prwdgt->path[i].x = 1.0f - ((float)(2 * step_x) / n_steps_x);
    prwdgt->path[i].y = (float)(2 * step_y) / n_steps_y;
    prwdgt->path[i].flag = PROF_HANDLE_VECTOR;
  }
}

/** Resets the profile to the current preset.
 * \note: Requires profilewidget_changed call after. */
void BKE_profilewidget_reset(ProfileWidget *prwdgt)
{
  if (prwdgt->path) {
    MEM_freeN(prwdgt->path);
  }

  int preset = prwdgt->preset;
  switch (preset) {
    case PROF_PRESET_LINE:
      prwdgt->totpoint = 2;
      break;
    case PROF_PRESET_SUPPORTS:
      prwdgt->totpoint = 12;
      break;
    case PROF_PRESET_CORNICE:
      prwdgt->totpoint = 13;
      break;
    case PROF_PRESET_CROWN:
      prwdgt->totpoint = 11;
      break;
    case PROF_PRESET_STEPS:
      /* Use dynamic number of control points based on the set number of segments. */
      if (prwdgt->totsegments == 0) {
        /* If totsegments hasn't been set, use the number of control points for 8 steps. */
        prwdgt->totpoint = 17;
      }
      else {
        prwdgt->totpoint = prwdgt->totsegments + 1;
      }
      break;
  }

  prwdgt->path = MEM_callocN((size_t)prwdgt->totpoint * sizeof(ProfilePoint), "path points");

  switch (preset) {
    case PROF_PRESET_LINE:
      prwdgt->path[0].x = 1.0;
      prwdgt->path[0].y = 0.0;
      prwdgt->path[1].x = 0.0;
      prwdgt->path[1].y = 1.0;
      break;
    case PROF_PRESET_SUPPORTS:
      prwdgt->path[0].x = 1.0;
      prwdgt->path[0].y = 0.0;
      prwdgt->path[0].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[1].x = 1.0;
      prwdgt->path[1].y = 0.5;
      prwdgt->path[1].flag = PROF_HANDLE_VECTOR;
      for (int i = 1; i < 10; i++) {
        prwdgt->path[i + 1].x = 1.0f - (0.5f * (1.0f - cosf((float)((i / 9.0) * M_PI_2))));
        prwdgt->path[i + 1].y = 0.5f + 0.5f * sinf((float)((i / 9.0) * M_PI_2));
      }
      prwdgt->path[10].x = 0.5;
      prwdgt->path[10].y = 1.0;
      prwdgt->path[10].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[11].x = 0.0;
      prwdgt->path[11].y = 1.0;
      prwdgt->path[11].flag = PROF_HANDLE_VECTOR;
      break;
    case PROF_PRESET_CORNICE:
      prwdgt->path[0].x = 1.0f;
      prwdgt->path[0].y = 0.0f;
      prwdgt->path[0].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[1].x = 1.0f;
      prwdgt->path[1].y = 0.125f;
      prwdgt->path[1].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[2].x = 0.92f;
      prwdgt->path[2].y = 0.16f;
      prwdgt->path[3].x = 0.875f;
      prwdgt->path[3].y = 0.25f;
      prwdgt->path[3].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[4].x = 0.8f;
      prwdgt->path[4].y = 0.25f;
      prwdgt->path[4].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[5].x = 0.733f;
      prwdgt->path[5].y = 0.433f;
      prwdgt->path[6].x = 0.582f;
      prwdgt->path[6].y = 0.522f;
      prwdgt->path[7].x = 0.4f;
      prwdgt->path[7].y = 0.6f;
      prwdgt->path[8].x = 0.289f;
      prwdgt->path[8].y = 0.727f;
      prwdgt->path[9].x = 0.25f;
      prwdgt->path[9].y = 0.925f;
      prwdgt->path[9].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[10].x = 0.175f;
      prwdgt->path[10].y = 0.925f;
      prwdgt->path[10].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[11].x = 0.175f;
      prwdgt->path[11].y = 1.0f;
      prwdgt->path[11].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[12].x = 0.0f;
      prwdgt->path[12].y = 1.0f;
      prwdgt->path[12].flag = PROF_HANDLE_VECTOR;
      break;
    case PROF_PRESET_CROWN:
      prwdgt->path[0].x = 1.0f;
      prwdgt->path[0].y = 0.0f;
      prwdgt->path[0].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[1].x = 1.0f;
      prwdgt->path[1].y = 0.25f;
      prwdgt->path[1].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[2].x = 0.75f;
      prwdgt->path[2].y = 0.25f;
      prwdgt->path[2].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[3].x = 0.75f;
      prwdgt->path[3].y = 0.325f;
      prwdgt->path[3].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[4].x = 0.925f;
      prwdgt->path[4].y = 0.4f;
      prwdgt->path[5].x = 0.975f;
      prwdgt->path[5].y = 0.5f;
      prwdgt->path[6].x = 0.94f;
      prwdgt->path[6].y = 0.65f;
      prwdgt->path[7].x = 0.85f;
      prwdgt->path[7].y = 0.75f;
      prwdgt->path[8].x = 0.75f;
      prwdgt->path[8].y = 0.875f;
      prwdgt->path[9].x = 0.7f;
      prwdgt->path[9].y = 1.0f;
      prwdgt->path[9].flag = PROF_HANDLE_VECTOR;
      prwdgt->path[10].x = 0.0f;
      prwdgt->path[10].y = 1.0f;
      prwdgt->path[10].flag = PROF_HANDLE_VECTOR;
      break;
    case PROF_PRESET_STEPS:
      profilewidget_build_steps(prwdgt);
      break;
  }

  if (prwdgt->table) {
    MEM_freeN(prwdgt->table);
    prwdgt->table = NULL;
  }
}

/** Helper for 'profile_widget_create' samples. Returns whether both handles that make up the edge
 * are vector handles. */
static bool is_curved_edge(BezTriple * bezt, int i)
{
  return (bezt[i].h2 != HD_VECT || bezt[i + 1].h1 != HD_VECT);
}

/** Used to set bezier handle locations in the sample creation process. Reduced copy of
 * #calchandleNurb_intern code in curve.c. */
static void calchandle_profile(BezTriple *bezt, const BezTriple *prev, const BezTriple *next)
{
#define point_handle1 ((point_loc)-3)
#define point_handle2 ((point_loc) + 3)

  const float *prev_loc, *next_loc;
  float *point_loc;
  float pt[3];
  float len, len_a, len_b;
  float dvec_a[2], dvec_b[2];

  if (bezt->h1 == 0 && bezt->h2 == 0) {
    return;
  }

  point_loc = bezt->vec[1];

  if (prev == NULL) {
    next_loc = next->vec[1];
    pt[0] = 2.0f * point_loc[0] - next_loc[0];
    pt[1] = 2.0f * point_loc[1] - next_loc[1];
    prev_loc = pt;
  }
  else {
    prev_loc = prev->vec[1];
  }

  if (next == NULL) {
    prev_loc = prev->vec[1];
    pt[0] = 2.0f * point_loc[0] - prev_loc[0];
    pt[1] = 2.0f * point_loc[1] - prev_loc[1];
    next_loc = pt;
  }
  else {
    next_loc = next->vec[1];
  }

  sub_v2_v2v2(dvec_a, point_loc, prev_loc);
  sub_v2_v2v2(dvec_b, next_loc, point_loc);

  len_a = len_v2(dvec_a);
  len_b = len_v2(dvec_b);

  if (len_a == 0.0f) {
    len_a = 1.0f;
  }
  if (len_b == 0.0f) {
    len_b = 1.0f;
  }

  if (bezt->h1 == HD_AUTO || bezt->h2 == HD_AUTO) { /* auto */
    float tvec[2];
    tvec[0] = dvec_b[0] / len_b + dvec_a[0] / len_a;
    tvec[1] = dvec_b[1] / len_b + dvec_a[1] / len_a;

    len = len_v2(tvec) * 2.5614f;
    if (len != 0.0f) {

      if (bezt->h1 == HD_AUTO) {
        len_a /= len;
        madd_v2_v2v2fl(point_handle1, point_loc, tvec, -len_a);
      }
      if (bezt->h2 == HD_AUTO) {
        len_b /= len;
        madd_v2_v2v2fl(point_handle2, point_loc, tvec, len_b);
      }
    }
  }

  if (bezt->h1 == HD_VECT) { /* vector */
    madd_v2_v2v2fl(point_handle1, point_loc, dvec_a, -1.0f / 3.0f);
  }
  if (bezt->h2 == HD_VECT) {
    madd_v2_v2v2fl(point_handle2, point_loc, dvec_b, 1.0f / 3.0f);
  }
#undef point_handle1
#undef point_handle2
}

/** Helper function for 'BKE_profilewidget_create_samples.' Calculates the angle between the
 * handles on the inside of the edge starting at index i. A larger angle means the edge is
 * more curved.
 * \param i_edge: The start index of the edge to calculate the angle for. */
static float bezt_edge_handle_angle(const BezTriple *bezt, int i_edge)
{
  /* Find the direction of the handles that define this edge along the direction of the path. */
  float start_handle_direction[2], end_handle_direction[2];
  /* Handle 2 - point location. */
  sub_v2_v2v2(start_handle_direction, bezt[i_edge].vec[2], bezt[i_edge].vec[1]);
  /* Point location - handle 1. */
  sub_v2_v2v2(end_handle_direction, bezt[i_edge + 1].vec[1], bezt[i_edge + 1].vec[0]);

  float angle = angle_v2v2(start_handle_direction, end_handle_direction);
//  printf("bezt_edge_handle_angle: i: %d, angle:%.2f\n", i_edge, RAD2DEGF(angle));
//  printf("  first point:         (%.2f, %.2f)\n", bezt[i_edge].vec[1][0], bezt[i_edge].vec[1][1]);
//  printf("  first inner handle:  (%.2f, %.2f)\n", bezt[i_edge].vec[2][0], bezt[i_edge].vec[2][1]);
//  printf("  second inner handle: (%.2f, %.2f)\n", bezt[i_edge + 1].vec[0][0], bezt[i_edge + 1].vec[0][1]);
//  printf("  second point:        (%.2f, %.2f)\n", bezt[i_edge + 1].vec[1][0], bezt[i_edge + 1].vec[1][1]);
  return angle;
}

/** Struct to sort curvature of control point edges. */
typedef struct {
  /** The index of the corresponding bezier point. */
  int bezt_index;
  /** The curvature of the edge with the above index. */
  float bezt_curvature;
} CurvatureSortPoint;

/** Helper function for 'BKE_profilewidget_create_samples' for sorting edges based on curvature. */
static int search_points_curvature(const void *in_a, const void *in_b)
{
  const CurvatureSortPoint *a = (const CurvatureSortPoint *)in_a;
  const CurvatureSortPoint *b = (const CurvatureSortPoint *)in_b;

  if (a->bezt_curvature > b->bezt_curvature) {
    return 0;
  }
  else {
    return 1;
  }
}

/** Used for sampling curves along the profile's path. Any points more than the number of user-
 * defined points will be evenly distributed among the curved edges. Then the remainders will be
 * distributed to the most curved edges.
 * \param n_segments: The number of segments to sample along the path. It must be higher than the
 *        number of points used to define the profile (prwdgt->totpoint).
 * \param sample_straight_edges: Whether to sample points between vector handle control points. If
 *        this is true and there are only vector edges the straight edges will still be sampled.
 * \param r_samples: An array of points to put the sampled positions. Must have length n_segments.
 * \return r_samples: Fill the array with the sampled locations and if the point corresponds
 *         to a control point, its handle type */
void BKE_profilewidget_create_samples(ProfileWidget *prwdgt,
                                      int n_segments,
                                      bool sample_straight_edges,
                                      ProfilePoint *r_samples)
{
#ifdef DEBUG_PRWDGT_TABLE
  printf("PROFILEWIDGET CREATE SAMPLES\n");
#endif
  BezTriple *bezt;
  int i, n_left, n_common, i_sample, n_curved_edges;
  int *n_samples;
  CurvatureSortPoint *curve_sorted;
  int totpoints = prwdgt->totpoint;
  int totedges = totpoints - 1;

  BLI_assert(n_segments > 0);

  /* Create Bezier points for calculating the higher resolution path. */
  bezt = MEM_callocN((size_t)totpoints * sizeof(BezTriple), "beztarr");
  for (i = 0; i < totpoints; i++) {
    bezt[i].vec[1][0] = prwdgt->path[i].x;
    bezt[i].vec[1][1] = prwdgt->path[i].y;
    bezt[i].h1 = bezt[i].h2 = (prwdgt->path[i].flag & PROF_HANDLE_VECTOR) ? HD_VECT : HD_AUTO;
  }
  /* Give the first and last bezier points the same handle type as their neighbors. */
  if (totpoints > 2) {
    bezt[0].h1 = bezt[0].h2 = bezt[1].h1;
    bezt[totpoints - 1].h1 = bezt[totpoints - 1].h2 = bezt[totpoints - 2].h2;
  }
  /* Get handle positions for the bezier points. */
  calchandle_profile(&bezt[0], NULL, &bezt[1]);
  for (i = 1; i < totpoints - 1; i++) {
    calchandle_profile(&bezt[i], &bezt[i - 1], &bezt[i + 1]);
  }
  calchandle_profile(&bezt[totpoints - 1], &bezt[totpoints - 2], NULL);

  /* Create a list of edge indices with the most curved at the start, least curved at the end. */
  curve_sorted = MEM_callocN((size_t)totedges * sizeof(CurvatureSortPoint), "curve sorted");
  for (i = 0; i < totedges; i++) {
    curve_sorted[i].bezt_index = i;
  }
  /* Calculate the curvature of each edge once for use when sorting for curvature. */
  for (i = 0; i < totedges; i++) {
    curve_sorted[i].bezt_curvature = bezt_edge_handle_angle(bezt, i);
  }
  qsort(curve_sorted, (size_t)totedges, sizeof(CurvatureSortPoint), search_points_curvature);

  /* Assign the number of sampled points for each edge. */
  n_samples = MEM_callocN((size_t)totedges * sizeof(int),  "create samples numbers");
  int n_added = 0;
  if (n_segments >= totedges) {
    if (sample_straight_edges) {
      /* Assign an even number to each edge if it’s possible, then add the remainder of sampled
       * points starting with the most curved edges. */
      n_common = n_segments / totedges;
      n_left = n_segments % totedges;

      /* Assign the points that fill fit evenly to the edges. */
      if (n_common > 0) {
        for (i = 0; i < totedges; i++) {
          n_samples[i] = n_common;
          n_added += n_common;
        }
      }
    }
    else {
      /* Count the number of curved edges */
      n_curved_edges = 0;
      for (i = 0; i < totedges; i++) {
        if (is_curved_edge(bezt, i)) {
          n_curved_edges++;
        }
      }
      /* Just sample all of the edges if there are no curved edges. */
      n_curved_edges = (n_curved_edges == 0) ? totedges : n_curved_edges;

      /* Give all of the curved edges the same number of points and straight edges one point. */
      n_left = n_segments - (totedges - n_curved_edges); /* Left after 1 for each straight edge */
      n_common = n_left / n_curved_edges; /* Number assigned to all curved edges */
      if (n_common > 0) {
        for (i = 0; i < totedges; i++) {
          /* Add the common number if it's a c  urved edges or if all of them will get it. */
          if (is_curved_edge(bezt, i) || n_curved_edges == totedges) {
            n_samples[i] += n_common;
            n_added += n_common;
          }
          else {
            n_samples[i] = 1;
            n_added++;
          }
        }
      }
      n_left -= n_common * n_curved_edges;
    }
  } else {
    /* Not enough segments to give one to each edge, so just give them to the most curved edges. */
    n_left = n_segments;
  }
  /* Assign the remainder of the points that couldn't be spread out evenly. */
  BLI_assert(n_left < totedges);
  for (i = 0; i < n_left; i++) {
    n_samples[curve_sorted[i].bezt_index]++;
    n_added++;
  }

  BLI_assert(n_added == n_segments); /* n_added is just used for this assert, could remove it. */

  /* Sample the points and add them to the locations table. */
  for (i_sample = 0, i = 0; i < totedges; i++) {
    if (n_samples[i] > 0) {
      /* Carry over the handle type from the control point to its first corresponding sample. */
      r_samples[i_sample].flag = (bezt[i].h2 == HD_VECT) ? PROF_HANDLE_VECTOR : PROF_HANDLE_AUTO;
      /* All extra sample points for this control point get "auto" handles */
      for (int j = i_sample + 1; j < i_sample + n_samples[i]; j++) {
        r_samples[j].flag = PROF_HANDLE_AUTO;
        BLI_assert(j < n_segments);
      }

      /* Do the sampling from bezier points, X values first, then Y values */
      BKE_curve_forward_diff_bezier(bezt[i].vec[1][0],
                                    bezt[i].vec[2][0],
                                    bezt[i + 1].vec[0][0],
                                    bezt[i + 1].vec[1][0],
                                    &r_samples[i_sample].x,
                                    n_samples[i],
                                    sizeof(ProfilePoint));
      BKE_curve_forward_diff_bezier(bezt[i].vec[1][1],
                                    bezt[i].vec[2][1],
                                    bezt[i + 1].vec[0][1],
                                    bezt[i + 1].vec[1][1],
                                    &r_samples[i_sample].y,
                                    n_samples[i],
                                    sizeof(ProfilePoint));
    }
    i_sample += n_samples[i]; /* Add the next set of points after the ones we just added. */
    BLI_assert(i_sample <= n_segments);
  }

#ifdef DEBUG_PRWDGT_TABLE
  printf("n_segments: %d\n", n_segments);
  printf("totedges: %d\n", totedges);
  printf("n_common: %d\n", n_common);
  printf("n_left: %d\n", n_left);
  printf("n_samples: ");
  for (i = 0; i < totedges; i++) {
    printf("%d, ", n_samples[i]);
  }
  printf("\n");
  printf("i_curved_sorted: ");
  for (i = 0; i < totedges; i++) {
    printf("(%d %.2f), ", curve_sorted[i].bezt_index, curve_sorted[i].bezt_curvature);
  }
  printf("\n");
#endif
  MEM_freeN(bezt);
  MEM_freeN(curve_sorted);
  MEM_freeN(n_samples);
}

/** Creates a higher resolution table by sampling the curved points. This table is used for display
 * and evenly spaced evaluation. */
static void profilewidget_make_table(ProfileWidget *prwdgt)
{
  int n_samples = PROF_N_TABLE(prwdgt->totpoint);
  ProfilePoint *new_table = MEM_callocN((size_t)(n_samples + 1) * sizeof(ProfilePoint),
                                        "high-res table");

  BKE_profilewidget_create_samples(prwdgt, n_samples - 1, false, new_table);
  /* Manually add last point at the end of the profile */
  new_table[n_samples - 1].x = 0.0f;
  new_table[n_samples - 1].y = 1.0f;

#ifdef DEBUG_PRWDGT_TABLE
  printf("High-res table samples:\n");
  for (int i = 0; i < n_samples; i++) {
    printf("(%.3f, %.3f), ", new_table[i].x, new_table[i].y);
  }
  printf("\n");
#endif

  if (prwdgt->table) {
    MEM_freeN(prwdgt->table);
  }
  prwdgt->table = new_table;
}

/** Creates the table of points used for displaying a preview of the sampled segment locations on
 * the widget itself. */
static void profilewidget_make_segments_table(ProfileWidget *prwdgt)
{
  int n_samples = prwdgt->totsegments;
  if (n_samples <= 0) {
    return;
  }
  ProfilePoint *new_table = MEM_callocN((size_t)(n_samples + 1) * sizeof(ProfilePoint),
                                        "samples table");

  BKE_profilewidget_create_samples(prwdgt, n_samples, prwdgt->flag & PROF_SAMPLE_STRAIGHT_EDGES,
                               new_table);

  if (prwdgt->segments) {
    MEM_freeN(prwdgt->segments);
  }
  prwdgt->segments = new_table;
}

/** Sets the default settings and clip range for the profile widget. Does not generate either
 * table. */
void BKE_profilewidget_set_defaults(ProfileWidget *prwdgt)
{
  prwdgt->flag = PROF_USE_CLIP;

  BLI_rctf_init(&prwdgt->view_rect, 0.0f, 1.0f, 0.0f, 1.0f);
  prwdgt->clip_rect = prwdgt->view_rect;

  prwdgt->totpoint = 2;
  prwdgt->path = MEM_callocN(2 * sizeof(ProfilePoint), "path points");

  prwdgt->path[0].x = 1.0f;
  prwdgt->path[0].y = 0.0f;
  prwdgt->path[1].x = 1.0f;
  prwdgt->path[1].y = 1.0f;

  prwdgt->changed_timestamp = 0;
}

/** Returns a pointer to a newly allocated profile widget, using the given preset.
  \param preset: Value in eProfileWidgetPresets. */
struct ProfileWidget *BKE_profilewidget_add(int preset)
{
  ProfileWidget *prwdgt = MEM_callocN(sizeof(ProfileWidget), "profile widget");

  BKE_profilewidget_set_defaults(prwdgt);
  prwdgt->preset = preset;
  BKE_profilewidget_reset(prwdgt);
  profilewidget_make_table(prwdgt);

  return prwdgt;
}

/** Should be called after the widget is changed. Does profile and remove double checks and more
 * importantly, recreates the display / evaluation and segments tables. */
void BKE_profilewidget_changed(ProfileWidget *prwdgt, const bool remove_double)
{
  ProfilePoint *points = prwdgt->path;
  rctf *clipr = &prwdgt->clip_rect;
  float thresh;
  float dx, dy;
  int i;

  prwdgt->changed_timestamp++;

  /* Clamp with the clipping rect in case something got past. */
  if (prwdgt->flag & PROF_USE_CLIP) {
    /* Move points inside the clip rectangle. */
    for (i = 0; i < prwdgt->totpoint; i++) {
      points[i].x = max_ff(points[i].x, clipr->xmin);
      points[i].x = min_ff(points[i].x, clipr->xmax);
      points[i].y = max_ff(points[i].y, clipr->ymin);
      points[i].y = min_ff(points[i].y, clipr->ymax);
    }
    /* Ensure zoom-level respects clipping. */
    if (BLI_rctf_size_x(&prwdgt->view_rect) > BLI_rctf_size_x(&prwdgt->clip_rect)) {
      prwdgt->view_rect.xmin = prwdgt->clip_rect.xmin;
      prwdgt->view_rect.xmax = prwdgt->clip_rect.xmax;
    }
    if (BLI_rctf_size_y(&prwdgt->view_rect) > BLI_rctf_size_y(&prwdgt->clip_rect)) {
      prwdgt->view_rect.ymin = prwdgt->clip_rect.ymin;
      prwdgt->view_rect.ymax = prwdgt->clip_rect.ymax;
    }
  }

  /* Remove doubles with a threshold set at 1% of default range. */
  thresh = 0.01f * BLI_rctf_size_x(clipr);
  if (remove_double && prwdgt->totpoint > 2) {
    for (i = 0; i < prwdgt->totpoint - 1; i++) {
      dx = points[i].x - points[i + 1].x;
      dy = points[i].y - points[i + 1].y;
      if (sqrtf(dx * dx + dy * dy) < thresh) {
        if (i == 0) {
          points[i + 1].flag |= PROF_HANDLE_VECTOR;
          if (points[i + 1].flag & PROF_SELECT) {
            points[i].flag |= PROF_SELECT;
          }
        }
        else {
          points[i].flag |= PROF_HANDLE_VECTOR;
          if (points[i].flag & PROF_SELECT) {
            points[i + 1].flag |= PROF_SELECT;
          }
        }
        break; /* Assumes 1 deletion per edit is ok. */
      }
    }
    if (i != prwdgt->totpoint - 1) {
      BKE_profilewidget_remove(prwdgt, 2);
    }
  }

  /* Create the high resolution table for drawing and some evaluation functions. */
  profilewidget_make_table(prwdgt);

  /* Store a table of samples for the segment locations for a preview and the table's user. */
  if (prwdgt->totsegments > 0) {
    profilewidget_make_segments_table(prwdgt);
  }
}

/** Refreshes the higher resolution table sampled from the input points. A call to this or
 * profilewidget_changed is needed before evaluation functions that use the table. Also sets the
 * number of segments used for the display preview of the locations of the sampled points. */
void BKE_profilewidget_initialize(ProfileWidget *prwdgt, short nsegments)
{
  prwdgt->totsegments = nsegments;

  /* Calculate the higher resolution tables for display and evaluation. */
  BKE_profilewidget_changed(prwdgt, false);
}

/** Gives the distance to the next point in the widget's sampled table, in other words the length
 * of the ith edge of the table.
 * \note Requires profilewidget_initialize or profilewidget_changed call before to fill table. */
static float profilewidget_distance_to_next_point(const ProfileWidget *prwdgt, int i)
{
  BLI_assert(prwdgt != NULL);
  BLI_assert(i >= 0);
  BLI_assert(i < prwdgt->totpoint);

  float x = prwdgt->table[i].x;
  float y = prwdgt->table[i].y;
  float x_next = prwdgt->table[i + 1].x;
  float y_next = prwdgt->table[i + 1].y;

  return sqrtf(powf(y_next - y, 2) + powf(x_next - x, 2));
}

/** Calculates the total length of the profile from the curves sampled in the table.
 * \note Requires profilewidget_initialize or profilewidget_changed call before to fill table. */
float BKE_profilewidget_total_length(const ProfileWidget *prwdgt)
{
  float loc1[2], loc2[2];
  float total_length = 0;

  for (int i = 0; i < PROF_N_TABLE(prwdgt->totpoint); i++) {
    loc1[0] = prwdgt->table[i].x;
    loc1[1] = prwdgt->table[i].y;
    loc2[0] = prwdgt->table[i].x;
    loc2[1] = prwdgt->table[i].y;
    total_length += len_v2v2(loc1, loc2);
  }
  return total_length;
}

/** Samples evenly spaced positions along the profile widget's table (generated from path). Fills
 * an entire table at once for a speedup if all of the results are going to be used anyway.
 * \note Requires profilewidget_initialize or profilewidget_changed call before to fill table. */
/* HANS-TODO: Enable this for an "even length sampling" option (and debug it). */
void BKE_profilewidget_create_samples_even_spacing(const ProfileWidget *prwdgt,
                                                   double *x_table_out,
                                                   double *y_table_out)
{
  const float total_length = BKE_profilewidget_total_length(prwdgt);
  const float segment_length = total_length / prwdgt->totsegments;
  float length_travelled = 0.0f;
  float distance_to_next_point = profilewidget_distance_to_next_point(prwdgt, 0);
  float distance_to_previous_point = 0.0f;
  float travelled_since_last_point = 0.0f;
  float segment_left = segment_length;
  float f;
  int i_point = 0;

  /* Travel along the path, recording the locations of segments as we pass them. */
  for (int i = 0; i < prwdgt->totsegments; i++) {
    /* Travel over all of the points that could be inside this segment. */
    while (distance_to_next_point > segment_length * (i + 1) - length_travelled) {
      length_travelled += distance_to_next_point;
      segment_left -= distance_to_next_point;
      travelled_since_last_point += distance_to_next_point;
      i_point++;
      distance_to_next_point = profilewidget_distance_to_next_point(prwdgt, i_point);
      distance_to_previous_point = 0.0f;
    }
    /* We're now at the last point that fits inside the current segment. */
    f = segment_left / (distance_to_previous_point + distance_to_next_point);
    x_table_out[i] = (double)interpf(prwdgt->table[i_point].x, prwdgt->table[i_point + 1].x, f);
    y_table_out[i] = (double)interpf(prwdgt->table[i_point].x, prwdgt->table[i_point + 1].x, f);
    distance_to_next_point -= segment_left;
    distance_to_previous_point += segment_left;

    length_travelled += segment_left;
  }
}

/** Does a single evaluation along the profile's path. Travels down (length_portion * path) length
 * and returns the position at that point.
 * \param length_portion: The portion (0 to 1) of the path's full length to sample at.
 * \note Requires profilewidget_initialize or profilewidget_changed call before to fill table */
void BKE_profilewidget_evaluate_length_portion(const ProfileWidget *prwdgt,
                                               float length_portion,
                                               float *x_out,
                                               float *y_out)
{
#ifdef DEBUG_PRWDGT_EVALUATE
  printf("PROFILEPATH EVALUATE\n");
#endif
  const float total_length = BKE_profilewidget_total_length(prwdgt);
  float requested_length = length_portion * total_length;

  /* Find the last point along the path with a lower length portion than the input. */
  int i = 0;
  float length_travelled = 0.0f;
  while (length_travelled < requested_length) {
    /* Check if we reached the last point before the final one. */
    if (i == PROF_N_TABLE(prwdgt->totpoint) - 2) {
      break;
    }
    float new_length = profilewidget_distance_to_next_point(prwdgt, i);
    if (length_travelled + new_length >= requested_length) {
      break;
    }
    length_travelled += new_length;
    i++;
  }

  /* Now travel the remaining distance of length portion down the path to the next point and
   * find the location where we stop. */
  float distance_to_next_point = profilewidget_distance_to_next_point(prwdgt, i);
  float lerp_factor = (requested_length - length_travelled) / distance_to_next_point;

#ifdef DEBUG_PRWDGT_EVALUATE
  printf("  length portion input: %f\n", (double)length_portion);
  printf("  requested path length: %f\n", (double)requested_length);
  printf("  distance to next point: %f\n", (double)distance_to_next_point);
  printf("  length travelled: %f\n", (double)length_travelled);
  printf("  lerp-factor: %f\n", (double)lerp_factor);
  printf("  ith point  (%f, %f)\n", (double)prwdgt->path[i].x, (double)prwdgt->path[i].y);
  printf("  next point (%f, %f)\n", (double)prwdgt->path[i + 1].x, (double)prwdgt->path[i + 1].y);
#endif

  *x_out = interpf(prwdgt->table[i].x, prwdgt->table[i + 1].x, lerp_factor);
  *y_out = interpf(prwdgt->table[i].y, prwdgt->table[i + 1].y, lerp_factor);
}
