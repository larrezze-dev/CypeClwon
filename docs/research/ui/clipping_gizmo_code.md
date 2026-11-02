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
