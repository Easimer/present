## present

present is a simple presentation program for Windows and X11.

### How to build
#### Linux
On Linux you will need development packages for X11 XCB, xcb\_ewmh
and Cairo. You will also need a C++ compiler and `pkg-config`.

On Fedora 30 these are: `libX11-devel`, `libxcb-devel`,
`xcb-util-wm-devel`, and `cairo-devel`

`$ make`
#### Windows
On Windows you will need an installation of Visual Studio
(even VS2012 should be enough).

Open a native x64 developer prompt, `cd` to the source directory
and run `build.bat`.

### prs file format
The presentation file is a simple UTF-8 text file. For a complete
example see `example.prs`. A presentation file starts with the line
`#PRESENT`
that is followed by directives such as `#SLIDE`, `#TITLE`, etc.

#### Directives
`#SLIDE`
This directive begins a new slide. Any line that doesn't begin
with a pound sign is considered to be the contents of the
slide.

`#SUBTITLE`
Set a heading for the current slide. Optional.

This directive is invalid before the first `#SLIDE`!


`#INLINE_IMAGE`
Insert an image into the slide. It's argument is a path (relative
to the .prs file) pointing to an image file (BMP, JPG, PNG, etc.,
see stb\_image.h for what kind of of formats are supported).

This directive is invalid before the first `#SLIDE`!

`#CHAPTER`
Set the chapter title. This string will be shown on the top of every
slide on every slide after this directive but before the next
`#CHAPTER`. Optional.

`#TITLE`
The title of the presentation. May be set anywhere in the file
(after the `#SLIDE` header) and is optional. Shown on the opening
slide.

`#AUTHORS`
The authors of the presentation. May be set anywhere in the file
(after the `#SLIDE` header) and is optional. Shown on the opening
slide.

`#FONT`
Set the font used for the contents of the presentation. If the font
doesn't exist then a platform-specific default will be used. May be
set anywhere in the file (after the `#SLIDE` header) and may be set
multiple times, but only the last instance of `#FONT` will be
considered. Optional.

Note: on the current implementation multiple use of the `#FONT*`
directives will increase memory usage!

`#FONT_TITLE`
Set the font used on the opening slide. If the font doesn't exist then
a platform-specific default will be used. May be set anywhere in the
file (after the `#SLIDE` header) and may be set multiple times, but
only the last instance of `#FONT_TITLE` will be considered. Optional.

Note: on the current implementation multiple use of the `#FONT*`
directives will increase memory usage!

`#FONT_CHAPTER`
Set the font used for the chapter headers. If the font doesn't exist
then a platform-specific default will be used. May be set anywhere in
the file (after the `#SLIDE` header) and may be set multiple times,
but only the last instance of `#FONT_CHAPTER` will be considered.
Optional.

Note: on the current implementation multiple use of the `#FONT*`
directives will increase memory usage!
