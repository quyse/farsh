#!/bin/sh

if NOPROGRESS=1 ice-cpp.bat exe:release/F.A.R.S.H
then
	echo Uploading...
	cp -f box.geo.indices box.geo.vertices knot.geo.vertices knot.geo.indices zombie.geo.vertices zombie.geo.indices zombie.skeleton zombie.ba hero.ba axe.ba axe.geo.indices axe.geo.vertices labyrint.txt shaders release/F.A.R.S.H.exe ../../drive/F.A.R.S.H.\ Release
fi
