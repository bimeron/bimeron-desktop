# Checkbox HiDPI rendering

On Windows at device pixel ratios above 1, checkbox indicators retain their existing
logical size, four pixel corner radius, colors, and check mark. The one
physical pixel outline is rasterized at the device resolution with matching
coverage in all four corners. The label and checkbox interaction use Qt's
normal controls. At a device pixel ratio of 1, the existing stylesheet draws
the complete checkbox.

Acceptance criteria: on Windows at 150% and 200% scaling, unchecked and checked
checkboxes have a continuous border with equal top, left, bottom, and right
weight. Hover and disabled states remain distinct, labels remain legible, and
clicking the indicator or label toggles the setting. The same rendering applies
to Settings and profile editor checkboxes. Linux keeps Qt's stylesheet rendering.
The label starts after the indicator and its seven logical pixel spacing; no
part of the text overlaps the indicator.
