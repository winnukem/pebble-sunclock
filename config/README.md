## Configuration pages for the Twilight Sunclock watchface

These pages are fairly simple, but there are a few points worth noting:

1. There are three separate config pages / html files, and the view transfers
between them depending on user button pushes etc. Initially it seemed like a
good idea to simply set document.location in one html page to jump the user
directly to the desired next html page. This however resulted in random NP object
errors, so now all page transitions pass through the core Pebble javascript
via use of pebblejs://close links.

2. In order to provide direct http access to these pages on github, without
requiring anchors to view the raw files, we keep a copy in the project's
gh-pages branch. The "master" copy of the pages is kept here in the master
branch, and then redundantly copied into the gh-pages branch when an update
is desired. This is ugly, and I'd love to know a better way. But it gives us two
things: a single view of all files in the master branch, and clean URL access to
the config files in the github.io gh-pages branch. Now if only there were a way
to automagically sync the two..

3. When the user requests a "decode" of the current (phone) location, this is
done by using an XMLHttpRequest to [geonames.org](http://geonames.org). They
provide several different API possibilities; the one used now is
[findNearbyPlaceNameJSON](http://www.geonames.org/export/web-services.html#findNearbyPlaceName)
and it seems to find hyper-local and occasionally inconsequential names near
the current location. Not clear if it would be better to sacrifice locality
in order to obtain more recognizable names.

4. During initial testing of early versions of the configuration support,
I happened to be working in an area with only 2G coverage. At that time the
html pages were still using jQuery as in the samples I'd copied them from.
Config then became completely unusable unless I had wifi access. Upon
subsequent investigation, this was because the jQuery download calls for
300kB of data. *On each page load.* So the html pages are now written to
operate in stand-alone mode, requiring no downloads of javascript, css or
anything else. The only real loss seems to be a little bit of eye-candy
supplied by jQuery, but these are very simple pages and don't really need it.
