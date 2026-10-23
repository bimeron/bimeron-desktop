# Connection button HiDPI rendering

At device pixel ratios above 1, the connection button keeps its existing logical
size, six pixel corner radius, and connect/disconnect state colors. The button
is rasterized at the physical pixel size. Its disconnect outline is the
difference between two filled rounded rectangles, inset by one physical pixel.
The straight segments are filled on whole physical pixels so all four sides
have equal weight. At a device pixel ratio of 1, the button uses its original
Qt stylesheet rendering.

Acceptance criteria: on Windows and Linux at 100%, 150%, and 200% scaling,
the disconnect button has a continuous outline of visually uniform weight;
its hover, pressed, disabled, and connect states remain legible and clickable.
All four corners use the same mirrored physical-pixel coverage so the top and
left edges match the bottom and right edges. A development instance can be
opened with `--visual-check` alongside an installed copy for visual comparison.

On a Windows 4K display at 150% scaling, the captured button had one full-blue
pixel on each straight side and mirrored corner shapes. Linux and other scale
factors still require visual confirmation.
