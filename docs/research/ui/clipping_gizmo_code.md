# Clipping gizmo: code findings (REA, panelarea.dll, source file pnglslclip.c)

The gizmo is NOT in paneldib.dll or vistas3d.dll (they only forward `pndibglsl_*_rcp_clipping_*`). It lives in panelarea.dll
(image base 0x180000000). Names are stripped (FUN_*); addresses below are VAs.

## Receptor state machine (receptor->estado_receptor, offset +0x40)
0 = SELECCIONAR_ORIGEN_VOLUMEN (click on model picks the volume origin), 1 = SELECCIONAR_OBJETO_CAPTURABLE (hover/pick gizmo parts),
2 = dragging arrow, 3 = dragging plane arc, 4 = dragging circle (rotates the whole volume). Max 6 planes (num_planos <= 6).
Gizmo objects: `flecha`, `plano`, `arco_rotacion`, `circulo_rotacion`, `ref_a_origen`; pick tolerance 5 px.

## Functions
- 0x180058ac0 left-down: state 0 -> pick ray vs model (0x180055590/0x1800553d0), stores hit point as volume origin (+0x20..+0x30), creates 3 planes (0x18004f090 idx 0,1,2); state 1 -> picks part, goes to state 2/3/4 and stores mouse x,y (+0x44/+0x48).
- 0x1800583f0 drag (mouse move, button held): states 2/3 -> 0x180054680; state 4 -> 0x1800544a0, adds delta to volume angle (+0x38, = angulo_giro_antihorario_rad_xy_global); mouse x,y are updated on EVERY event (incremental).
- 0x1800544a0 volume rotation delta; 0x180054680 plane move/rotation; 0x18004ddc0 builds gizmo geometry; 0x180055ad0 highlights parts; 0x180058830 right-click (toggles plane side/mode via 0x180053a00).

## Angle formula (arc of a plane = element 1, circle of the volume = state 4, same maths)
dx = x - x_prev, dy = y - y_prev (pixels). If hypot(dx,dy) <= 0.001: no change.
If |dy| >= |dx|: d = dy, D = viewport height (px); else d = dx, D = viewport width (px).
delta = d * 2*pi / D  (signed; one full turn per viewport height/width of mouse travel; only the dominant axis counts).
(Which of the two outputs of escal_getDimensionesViewport is width/height was inferred, not proven: measured 0.48 deg/px vertical and 0.26 deg/px horizontal give D = 750 and 1385 px, consistent with a viewport of about 1385 x 750.)
Plane arc: new = plane.angulo (+0x22, angulo_giro_antihorario_rad_normal_plano) + delta; if new >= 1.3962634 rad or <= -1.3962634 rad (80 deg) the WHOLE step is rejected (not clamped); else stored and the plane redrawn (0x18004cd40).
No atan2, no ray-plane intersection: it is purely screen-delta based.

## Arrow (element 0, translation along the plane normal)
0x180055880(...,0.4) gives a signed factor from the mouse delta projected on the normal; displacement = factor * 0x180043620(scale, viewport, ceil(|delta px|)); point moved with ebageom_desplaza_punto3D along the (negated) normal.

## Gizmo size constants (0x18004ddc0)
m = max(extent_x, extent_y) of the volume (0x180051300 returns x, y, z extents); L0 = max(0.1*m, 1.0); if z_extent < 1.1*L0 then L0 = 0.75*z_extent; L = min(L0, 0.75*z_extent).
Arrow length L, head length 0.25*L, head radius 0.4*0.25*L = 0.1*L, shaft radius = head radius/3.
Arc `arco_rotacion`: 2D arc centred at the origin from (0,-0.6L) to (0,+0.6L) (radius 0.6L), 64 segments, then projected onto the plane axes.
Circle `circulo_rotacion` (volume rotation, only shown when 0x180052bd0 is true): centre displaced 0.75L along the normal, radius 4*(0.1*L) = 0.4L, 64 segments.

## Not found / open
- Initial plane position: in state 0 the origin is the model point picked by the click (mouse ray hit), NOT a fixed offset from the volume centre. The 3 planes are created through that point by 0x18004f090 (not decompiled). How the "activate section" command (no click) sets Xo/Yo/Zo_centro_volumen and the first plane offset was NOT established here (needs 0x18004f090, 0x180055590 and the caller of pndibglsl_iniciar_rcp_clipping_planos_independientes at 0x18003d410).
- Exact meaning of escal_getDimensionesViewport output order (width/height) is unverified.

## Initial position of planes and volume (REA, panelarea.dll; second pass)

Entry points `pndibglsl_iniciar_rcp_clipping_planos_independientes` (0x18003d410) and `..._volumen` (0x18003d490) have no callers inside
panelarea.dll (exports, called from paneldib/vistas3d). They forward to 0x180057b60 (planes) and 0x180057e20 (volume), then redraw.

### Independent planes (X/Y/Z button, NO click)
0x180057b60(receptor, axis, scene): resets the state, takes the scene bbox (0x1800534c0 = `cybsgedb_get_bbox_escena`; if empty the bbox is
forced to [-1,1]^3) and calls 0x18004eb80(axis, mode=0, side=0). Mode 0: origin = `bsBoundingBox_get_center` of the scene bbox.
Result: ONE plane through the CENTRE of the scene envelope (all three coordinates), normal = the chosen axis (0=X, 1=Y, 2=Z), own angle 0,
receptor state 1 (SELECCIONAR_OBJETO_CAPTURABLE). No click and no offset. Answers (1) and (2) for planes.
Right-click toggles the plane side flag (0x180053a00: side = (side == 0), redraw); side 1 negates the first in-plane axis (flips the normal).
Plane frame (0x180051c20): axis 0 -> U=(0,1,0) V=(0,0,1); axis 1 -> U=(-1,0,0) V=(0,0,1); axis 2 -> U=(1,0,0) V=(0,1,0); normal W = U x V.

### Clipping volume
0x180057e20: only allowed with zero planes; takes the scene bbox as above, draws it as a wireframe box of its 8 corners (0x18004d970 ->
0x18004d450) and sets state 0 (SELECCIONAR_ORIGEN_VOLUMEN). So on activation the volume is the bare scene bbox with NO margin and NO planes
yet; planes appear only after the click (answer 3 for the preview). Empty scene: box [-1,1]^3.

### Planes created by the click in state 0 (0x180058ac0 -> 0x18004f090)
0x18004f090(axis) calls 0x18004eb80(axis, mode=1, side=0) and (axis, 1, side=1); 0x180058ac0 calls it for axis 0,1,2 => 6 planes (matches max 6).
Mode 1: take the volume bbox extents; margin per axis m_i = max(0.1 * (max_i - min_i), 1.0); box = [min_i - m_i, max_i + m_i].
For axis a the plane origin is the picked point (receptor +0x20..) with coordinate a replaced by the box face: side 0 -> max face, side 1 -> min face.
Plane frame is then rotated about the volume axis by the volume angle (`bsMat4x4_make_rotation_around_axis`, angle = volume angle +0x38).
Not verified: which bbox 0x180058ac0 passes as the mode 1 bbox (scene bbox is the probable one; the caller was not decompiled).

### Own angle of a plane (item 4)
Range: strictly inside (-80 deg, +80 deg), steps reaching the limit are rejected, not clamped (already noted above).
In 0x180051c20 the angle is applied as: W = U x V; U' = U rotated about W by the angle (`mt_gira_v1_sobre_v2`); V' = W x U'.
Read literally this is a spin of the in-plane axes about the plane NORMAL; whether that moves the cutting plane or only the gizmo frame
depends on `ejes2d_ecuacion_del_plano`, which is in BASICOS.DLL and was NOT decompiled, so it is NOT established. The earlier measurement in
the original (arc turns the planes about the vertical axis) is not confirmed nor contradicted by the code; only the volume circle is clearly
about a fixed axis (mode 1 rotation). Pending: decompile BASICOS.DLL ejes2d_ecuacion_del_plano and mt_gira_v1_sobre_v2 argument order.
Also not examined: ray pick 0x180055590.

## Third pass: own angle of a plane and volume margin (REA: panelarea.dll, BASICOS.DLL, CYPEMATH.DLL)

### (1) The arc DOES move the cutting plane, about the in-plane axis V (CONFIRMED by code)
Chain: 0x18004eb80 -> 0x180050390 -> 0x1800508c0 -> 0x180051650 -> 0x180051850 -> 0x180051c20, then `ejes2d_ecuacion_del_plano` (BASICOS 0x1800d9e70)
-> `cybsgedb_append_plano_corte` (the clipping plane stored in the scene database).
- `ejes2d_ecuacion_del_plano(origin, a1, a2)`: (A,B,C) = unit(a1 x a2) (`ejes2d_vector_director_plano`, 0x1800df750; a final helper 0x1800dfa40 on the unit vector was NOT decompiled), D = -(A*ox + B*oy + C*oz). 0x180051650 calls it with a1 = -U', a2 = V', so the normal is -(U' x V').
- `mt_gira_v1_sobre_v2(sin, cos, v1, v2)` (CYPEMATH 0xb690) rotates the PAIR in place: v1' = v1*cos + v2*sin ; v2' = v2*cos - v1*sin (rotation in the v1,v2 plane, i.e. about v1 x v2). 0x180051c20 passes v1 = W (= U x V), v2 = U. So W' = W*cos + U*sin, U' = U*cos - W*sin; then V' = W' x U' = V (unchanged, algebraically verified).
- Hence the angle is a rotation of the plane about its in-plane axis V, passing through the same origin (the origin is not modified by the angle). The earlier literal reading "U about the normal" was wrong (the tuple order hides that W is the first argument).
- Axes per plane type (0x180051c20): X plane: U=(0,1,0) V=(0,0,1) W=(1,0,0); Y plane: U=(-1,0,0) V=(0,0,1) W=(0,1,0); Z plane: U=(1,0,0) V=(0,1,0) W=(0,0,1). Side flag 1 negates U (and so W).
- Consequence: X and Y planes (vertical) rotate about the global Z axis (matches the measurement: the plane narrows/widens in isometric). Z plane (horizontal) rotates about the global Y axis (tilts), NOT about Z (about Z it would not change). With W' = (sin*U + cos*W): for X plane W' = (cos, sin, 0); for Y plane W' = (-sin, cos, 0); for Z plane W' = (sin, 0, cos).
- In mode 1 (volume planes) the whole frame (origin, U, V) is then rotated by the volume angle about the axis in receptor +0x20.. (`bsMat4x4_make_rotation_around_axis`), after the own angle.
- Not established: sign convention of the retained side (which half-space is clipped) and the sign of positive mouse delta on screen.

### (2) Volume margin (CONFIRMED)
- The bbox passed by 0x180058ac0 to 0x18004f090 (-> 0x18004eb80 mode 1) is `*(receptor+0x0E)`, the SAME scene bbox that 0x180057e20 stores at activation (0x1800534c0 = scene bbox) and draws as a wireframe (0x18004d970) with NO margin. Empty scene [-1,1]^3.
- Margin m_i = max(0.1*extent_i, 1.0) per axis, added on both sides, computed only at the click (state 0 -> 1), not at activation. Before the click only the bare box is shown.
- At the click: the previous origin marker is deleted (`_pnglslclip_origen_volumen_plano`), the picked point is stored (+0x20..), 6 planes are created (axes 0,1,2 x sides 0,1) plus an extra call 0x18004f200 (NOT decompiled; probably the origin marker/gizmo), state becomes 1. The plane origin is the picked point with coordinate a replaced by the expanded face.
