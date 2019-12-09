
/* TODO UBO */
uniform vec2 sizeViewport;
uniform vec2 sizeViewportInv;

/* Per Object */
uniform vec4 gpModelMatrix[4];
uniform float thicknessScale;
uniform float thicknessWorldScale;
#define thicknessIsScreenSpace (thicknessWorldScale < 0.0)

/* Per Layer */
uniform float thicknessOffset;

in int ma1;
in int ma2;
/* Position contains thickness in 4th component. */
in vec4 pos;  /* Prev adj vert */
in vec4 pos1; /* Current edge */
in vec4 pos2; /* Current edge */
in vec4 pos3; /* Next adj vert */

out vec4 finalColor;

void discard_vert()
{
  /* We set the vertex at the camera origin to generate 0 fragments. */
  gl_Position = vec4(0.0, 0.0, -3e36, 0.0);
}

vec2 project_to_screenspace(vec4 v)
{
  return ((v.xy / v.w) * 0.5 + 0.5) * sizeViewport;
}

vec2 rotate_90deg(vec2 v)
{
  /* Counter Clock-Wise. */
  return vec2(-v.y, v.x);
}

mat4 model_matrix_get()
{
  return mat4(gpModelMatrix[0], gpModelMatrix[1], gpModelMatrix[2], gpModelMatrix[3]);
}

vec3 transform_point(mat4 m, vec3 v)
{
  return (m * vec4(v, 1.0)).xyz;
}

void stroke_vertex()
{
  /* Enpoints, we discard the vertices. */
  if (ma1 == -1 || ma2 == -1) {
    discard_vert();
    return;
  }

  mat4 model_mat = model_matrix_get();

  /* Avoid using a vertex attrib for quad positioning. */
  float x = float(gl_VertexID & 1);       /* [0..1] */
  float y = float(gl_VertexID & 2) - 1.0; /* [-1..1] */

  vec3 wpos_adj = transform_point(model_mat, (x == 0.0) ? pos.xyz : pos3.xyz);
  vec3 wpos1 = transform_point(model_mat, pos1.xyz);
  vec3 wpos2 = transform_point(model_mat, pos2.xyz);

  vec4 ndc_adj = point_world_to_ndc(wpos_adj);
  vec4 ndc1 = point_world_to_ndc(wpos1);
  vec4 ndc2 = point_world_to_ndc(wpos2);

  gl_Position = (x == 0.0) ? ndc1 : ndc2;

  /* TODO case where ndc1 & ndc2 is behind camera */
  vec2 ss_adj = project_to_screenspace(ndc_adj);
  vec2 ss1 = project_to_screenspace(ndc1);
  vec2 ss2 = project_to_screenspace(ndc2);
  /* Screenspace Lines tangents. */
  vec2 line = normalize(ss2 - ss1);
  vec2 line_adj = normalize((x == 0.0) ? (ss1 - ss_adj) : (ss_adj - ss2));
  /* Mitter tangent vector. */
  vec2 miter_tan = normalize(line_adj + line);
  float miter_dot = dot(miter_tan, line_adj);
  /* Break corners after a certain angle to avoid really thick corners. */
  const float miter_limit = 0.7071; /* cos(45°) */
  miter_tan = (miter_dot < miter_limit) ? line : (miter_tan / miter_dot);

  vec2 miter = rotate_90deg(miter_tan);

  /* Position contains thickness in 4th component. */
  float thickness = (x == 0.0) ? pos1.w : pos2.w;
  /* Modify stroke thickness by object and layer factors.-*/
  thickness *= thicknessScale;
  thickness += thicknessOffset;
  thickness = max(1.0, thickness);

  if (thicknessIsScreenSpace) {
    /* Multiply offset by view Z so that offset is constant in screenspace.
     * (e.i: does not change with the distance to camera) */
    thickness *= gl_Position.w;
  }
  else {
    /* World space point size. */
    thickness *= thicknessWorldScale * ProjectionMatrix[1][1] * sizeViewport.y;
  }
  /* Multiply scalars first. */
  y *= thickness;

  gl_Position.xy += miter * sizeViewportInv.xy * y;

  finalColor = vec4(0.0, 0.0, 0.0, 1.0);
}

void dots_vertex()
{
  /* TODO */
}

void fill_vertex()
{
  mat4 model_mat = model_matrix_get();

  vec3 wpos = transform_point(model_mat, pos1.xyz);
  gl_Position = point_world_to_ndc(wpos);
  gl_Position.z += 1e-2;

  finalColor = vec4(1.0);
}

void main()
{
  /* Trick to detect if a drawcall is stroke or fill. */
  bool is_fill = (gl_InstanceID == 0);

  if (!is_fill) {
    stroke_vertex();
  }
  else {
    fill_vertex();
  }
}
