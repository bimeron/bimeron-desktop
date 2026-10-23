# Combo box HiDPI rendering

On Windows at device pixel ratios above 1 and on Linux at all scales, ordinary combo boxes retain their
logical size, six pixel corner radius, text padding, arrow, colors, and popup
behavior. The one physical pixel outline has the same weight on every edge and
uses mirrored coverage in all four corners. Table editor combo boxes retain
their borderless style. Windows at 100% scaling keeps Qt's stylesheet rendering.

Acceptance criteria: on a Windows 4K display at 150% and 200% scaling, and on
Linux at 100% and fractional scaling, the
language and profile editor selectors have even borders with no clipped edge.
Text and arrow remain centered and legible; hover, focus, disabled, popup, and
keyboard selection continue to work.
In the popup, selection and keyboard focus fill the entire option row with a
flat pale blue background and no rectangular outline. While open, the combo
box has square bottom corners and the popup has square top corners and rounded
bottom corners. The popup window is clipped to that same shape so there is no
inner curve inside a square outer corner. The native rectangular popup shadow
and container frame are disabled, and the open combo box's stylesheet also uses square bottom
corners so it cannot show through the custom raster edge.
The popup's lower outline is rasterized symmetrically at physical pixel
resolution above the option rows. Highlighting the last visible option cannot
cover or weaken that outline, including at fractional scaling.
Closing the popup by selecting an option, clicking elsewhere, or pressing Escape
restores the combo box's rounded bottom corners.
