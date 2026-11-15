# Profile list HiDPI frame

On Windows, the profile list uses a rounded frame drawn at physical pixel
resolution. Its focused border is blue; otherwise it uses the existing pale
border color. All four straight edges have one physical pixel of equal weight,
and the corners use mirrored coverage. The list keeps its current item layout,
selection, scrolling, and keyboard focus behavior.

Acceptance criteria: on Windows at 100%, 150%, and 200% scaling, the top and
left borders have the same visible weight as the bottom and right borders.
Focus and item selection remain distinct. Linux keeps its existing Qt frame.
The outline is painted above the list contents so the viewport cannot cover
the rounded corners.
